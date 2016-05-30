
#ifndef _LINUX_IRQ_H
#define _LINUX_IRQ_H

#include <linux/smp.h>
#include <linux/cache.h>
#include <linux/spinlock.h>
#include <linux/gfp.h>
#include <linux/irqhandler.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/kthread.h>


struct msi_desc;
struct irq_common_data {
	unsigned int		state_use_accessors;
	void			*handler_data;
	struct msi_desc		*msi_desc;
};

struct irq_data {
	u32			mask;
	unsigned int		irq;
	unsigned long		hwirq;
	struct irq_chip		*chip;
	void			*chip_data;
	struct irq_common_data	*common;
};

struct irq_chip {
	const char	*name;
	void		(*irq_mask)(struct irq_data *data);
	void		(*irq_unmask)(struct irq_data *data);
};


#include <linux/irqdesc.h>


enum {
	IRQD_TRIGGER_MASK		= 0xf,
	IRQD_SETAFFINITY_PENDING	= (1 <<  8),
	IRQD_NO_BALANCING		= (1 << 10),
	IRQD_PER_CPU			= (1 << 11),
	IRQD_AFFINITY_SET		= (1 << 12),
	IRQD_LEVEL			= (1 << 13),
	IRQD_WAKEUP_STATE		= (1 << 14),
	IRQD_MOVE_PCNTXT		= (1 << 15),
	IRQD_IRQ_DISABLED		= (1 << 16),
	IRQD_IRQ_MASKED			= (1 << 17),
	IRQD_IRQ_INPROGRESS		= (1 << 18),
	IRQD_WAKEUP_ARMED		= (1 << 19),
	IRQD_FORWARDED_TO_VCPU		= (1 << 20),
};

enum {
	IRQS_AUTODETECT		= 0x00000001,
	IRQS_SPURIOUS_DISABLED	= 0x00000002,
	IRQS_POLL_INPROGRESS	= 0x00000008,
	IRQS_ONESHOT		= 0x00000020,
	IRQS_REPLAY		= 0x00000040,
	IRQS_WAITING		= 0x00000080,
	IRQS_PENDING		= 0x00000200,
	IRQS_SUSPENDED		= 0x00000800,
};

enum {
	IRQTF_RUNTHREAD,
	IRQTF_WARNED,
	IRQTF_AFFINITY,
	IRQTF_FORCED_THREAD,
};

#define linux_irqd_to_state(d) ACCESS_PRIVATE((d)->common, state_use_accessors)


static inline void
irqd_set(struct irq_data *d, unsigned int mask)
{
	linux_irqd_to_state(d) |= mask;
}

static inline void
irqd_clear(struct irq_data *d, unsigned int mask)
{
	linux_irqd_to_state(d) &= ~mask;
}

static inline bool
irqd_has_set(struct irq_data *d, unsigned int mask)
{
	return linux_irqd_to_state(d) & mask;
}

static inline bool
irqd_irq_disabled(struct irq_data *d)
{
	return linux_irqd_to_state(d) & IRQD_IRQ_DISABLED;
}

static inline bool
irqd_irq_inprogress(struct irq_data *d)
{
	return linux_irqd_to_state(d) & IRQD_IRQ_INPROGRESS;
}


static inline bool
irq_wait_for_poll(struct irq_desc *desc)
{

	do {
		spin_unlock(&desc->lock);
		while (irqd_irq_inprogress(&desc->irq_data))
			cpu_relax();
		spin_lock(&desc->lock);
	} while (irqd_irq_inprogress(&desc->irq_data));

	return !irqd_irq_disabled(&desc->irq_data) && desc->action;
}

static inline bool
irq_check_poll(struct irq_desc *desc)
{
	if (!(desc->istate & IRQS_POLL_INPROGRESS))
		return false;
	return irq_wait_for_poll(desc);
}

static inline bool
irq_pm_check_wakeup(struct irq_desc *desc)
{
	return (false);
}

static inline bool
irq_may_run(struct irq_desc *desc)
{
	unsigned int mask = IRQD_IRQ_INPROGRESS | IRQD_WAKEUP_ARMED;

	if (!irqd_has_set(&desc->irq_data, mask))
		return true;

	if (irq_pm_check_wakeup(desc))
		return false;

	return irq_check_poll(desc);
}

static inline void
linux_irq_wake_thread(struct irq_desc *desc, struct irqaction *action)
{
	if (action->thread->flags & PF_EXITING)
		return;
	if (test_and_set_bit(IRQTF_RUNTHREAD, &action->thread_flags))
		return;
	desc->threads_oneshot |= action->thread_mask;
	atomic_inc(&desc->threads_active);
	wake_up_process(action->thread);
}

static inline irqreturn_t
handle_irq_event_percpu(struct irq_desc *desc)
{
	irqreturn_t retval = IRQ_NONE;
	unsigned int flags = 0, irq = desc->irq_data.irq;
	struct irqaction *action;

	for_each_action_of_desc(desc, action) {
		irqreturn_t res;

		res = action->handler(irq, action->dev_id);
		switch (res) {
		case IRQ_WAKE_THREAD:
			MPASS(action->thread_fn != NULL);
			linux_irq_wake_thread(desc, action);
		case IRQ_HANDLED:
			flags |= action->flags;
			break;
		default:
			break;
		}
		retval |= res;
	}
	return (retval);
}

static inline irqreturn_t
handle_irq_event(struct irq_desc *desc)
{
	irqreturn_t ret;

	desc->istate &= ~IRQS_PENDING;
	irqd_set(&desc->irq_data, IRQD_IRQ_INPROGRESS);
	spin_unlock(&desc->lock);

	ret = handle_irq_event_percpu(desc);

	spin_lock(&desc->lock);
	irqd_clear(&desc->irq_data, IRQD_IRQ_INPROGRESS);
	return ret;
}

static inline void
handle_simple_irq(struct irq_desc *desc)
{
	spin_lock(&desc->lock);

	if (!irq_may_run(desc))
		goto out_unlock;

	desc->istate &= ~(IRQS_REPLAY | IRQS_WAITING);

	if (unlikely(desc->action == NULL || irqd_irq_disabled(&desc->irq_data))) {
		desc->istate |= IRQS_PENDING;
		goto out_unlock;
	}

	handle_irq_event(desc);

out_unlock:
	spin_unlock(&desc->lock);
	
}


extern void
irq_set_chip_and_handler_name(unsigned int irq, struct irq_chip *chip,
			      irq_flow_handler_t handle, const char *name);

static inline void irq_set_chip_and_handler(unsigned int irq, struct irq_chip *chip,
					    irq_flow_handler_t handle)
{
	irq_set_chip_and_handler_name(irq, chip, handle, NULL);
}


#endif
