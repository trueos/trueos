#ifndef _LINUX___ATOMIC_H_
#define _LINUX___ATOMIC_H_

#include <machine/atomic.h>
#include <asm/atomic.h>

#define smp_rmb() rmb()
#define smb_wmb() wmb()
#define smb_mb() mb()

#define smp_mb__before_atomic() smb_mb()

#define atomic64_add(i, v)  atomic_add_acq_long((volatile u_long *)(v), (i))
#define atomic64_sub(i, v)  atomic_add_acq_long((volatile u_long *)(v), -(i))
#define atomic_long_cmpxchg(v, o, n) atomic_cmpset_acq_long((volatile u_long *)(v), (o), (n))
#define atomic_long_set(v, i) (*((u_long *)v) = i)
#endif
