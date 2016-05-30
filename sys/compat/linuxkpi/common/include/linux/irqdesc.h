#ifndef _LINUX_IRQDESC_H
#define _LINUX_IRQDESC_H

#include <linux/rcupdate.h>
#include <linux/idr.h>
#include <linux/interrupt.h>

struct irq_desc {
	struct irq_data		irq_data;
	irq_flow_handler_t	handle_irq;
	spinlock_t		lock;
	struct irqaction	*action;	/* IRQ action list */
	unsigned int		istate;
	unsigned long		threads_oneshot;
	atomic_t		threads_active;
};

#define for_each_action_of_desc(desc, act)			\
	for (act = desc->act; act; act = act->next)


struct idr *irq_idr;

static inline void
generic_handle_irq_desc(struct irq_desc *desc)
{
	desc->handle_irq(desc);
}


static inline struct irq_desc *
irq_to_desc(unsigned int irq)
{
	return (idr_find(irq_idr, irq));
}

static inline int
generic_handle_irq(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc)
		return -EINVAL;
	generic_handle_irq_desc(desc);
	return 0;
}

#endif
