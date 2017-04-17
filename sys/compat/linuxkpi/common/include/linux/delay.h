/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2015 Mellanox Technologies, Ltd.
 * Copyright (c) 2014 François Tigeot
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _LINUX_DELAY_H_
#define	_LINUX_DELAY_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pcpu.h>

#include <machine/cpu.h>

#include <linux/jiffies.h>

static inline void
linux_msleep(int ms)
{
	pause("lnxsleep", msecs_to_jiffies(ms));
}

#undef msleep
#define	msleep	linux_msleep

/* undefined */
extern void linux_bad_udelay(void);

static inline unsigned long long
rdtsc_ordered(void)
{
	mb();
	return (rdtsc());
}


static inline void
delay_tsc(unsigned long __loops)
{
	u64 bclock, now, loops = __loops;
	int cpu;

	critical_enter();
	cpu = curcpu;
	bclock = rdtsc_ordered();
	for (;;) {
		now = rdtsc_ordered();
		if ((now - bclock) >= loops)
			break;

		critical_exit();
		cpu_spinwait();
		critical_enter();
		if (unlikely(cpu != curcpu)) {
			loops -= (now - bclock);
			cpu = curcpu;
			bclock = rdtsc_ordered();
		}
	}
	critical_exit();
}


static inline void
linux_const_udelay(unsigned long xloops)
{
	int d0;

	xloops *= 4;
	__asm__("mull %%edx"
		:"=d" (xloops), "=&a" (d0)
		:"1" (xloops), "0"
		 (tsc_freq/4));

	delay_tsc(xloops++);
}

static inline void
linux_udelay(unsigned long usecs)
{
	linux_const_udelay(usecs * 0x000010c7);
}

/* 0x10c7 is 2**32 / 1000000 (rounded up) */
#define udelay(n)							\
	({								\
		if (__builtin_constant_p(n)) {				\
			if ((n) / 20000 >= 1)				\
				 linux_bad_udelay();			\
			else						\
				linux_const_udelay(MAX(2, (n)) * 0x10c7ul); \
		} else {						\
			linux_udelay(n);				\
		}							\
	})




static inline void
mdelay(unsigned long msecs)
{
	while (msecs--)
		udelay(1000);
}

static inline void
ndelay(unsigned long x)
{
	udelay(howmany(x, 1000));
}

static inline void
usleep_range(unsigned long min, unsigned long max)
{
	udelay(max);
}

#endif	/* _LINUX_DELAY_H_ */
