#ifndef _LINUX_BH_H
#define _LINUX_BH_H

extern void raise_softirq(void);

static inline void
local_bh_enable(void)
{
#ifdef __linux__
	__local_bh_enable_ip(_THIS_IP_, SOFTIRQ_DISABLE_OFFSET);
#else
	raise_softirq();
	critical_exit();
#endif	
}

static inline void
local_bh_disable(void)
{
#ifdef __linux__
	__local_bh_disable_ip(_THIS_IP_, SOFTIRQ_DISABLE_OFFSET);
#else
	critical_enter();
#endif
}


#endif
