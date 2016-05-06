#ifndef _LINUX_IRQDESC_H
#define _LINUX_IRQDESC_H

#include <linux/rcupdate.h>

struct irq_desc {
	struct irq_data		irq_data;
	irq_flow_handler_t	handle_irq;
};


static inline void generic_handle_irq_desc(struct irq_desc *desc)
{
	desc->handle_irq(desc);
}

int generic_handle_irq(unsigned int irq);

#endif
