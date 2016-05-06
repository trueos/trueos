
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





struct irq_data {
	u32			mask;
	unsigned int		irq;
	unsigned long		hwirq;
	struct irq_chip		*chip;
	void			*chip_data;
};

struct irq_chip {
	const char	*name;
	void		(*irq_mask)(struct irq_data *data);
	void		(*irq_unmask)(struct irq_data *data);
};


#include <linux/irqdesc.h>

extern void handle_simple_irq(struct irq_desc *desc);

extern void
irq_set_chip_and_handler_name(unsigned int irq, struct irq_chip *chip,
			      irq_flow_handler_t handle, const char *name);

static inline void irq_set_chip_and_handler(unsigned int irq, struct irq_chip *chip,
					    irq_flow_handler_t handle)
{
	irq_set_chip_and_handler_name(irq, chip, handle, NULL);
}


#endif
