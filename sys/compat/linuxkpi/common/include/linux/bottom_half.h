#ifndef _LINUX_BH_H
#define _LINUX_BH_H

extern void raise_softirq(void);
extern void local_bh_enable(void);
extern void local_bh_disable(void);

#endif
