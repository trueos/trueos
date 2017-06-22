#ifndef _LINUX___ATOMIC_H_
#define _LINUX___ATOMIC_H_
#include <sys/cdefs.h>
#include <sys/types.h>
#include <machine/atomic.h>
#include <asm/atomic.h>
#if defined(__i386__) || defined(__amd64__)
#include <asm/atomic64.h>
#include <asm/atomic-long.h>
#endif
#define smp_rmb() rmb()
#define smb_wmb() wmb()
#define smb_mb() mb()

#define smp_mb__before_atomic() smb_mb()

#endif
