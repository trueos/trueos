#ifndef _LINUX___ATOMIC_H_
#define _LINUX___ATOMIC_H_
#include <machine/atomic.h>

#define smp_rmb() rmb()
#define smb_wmb() wmb()

#endif
