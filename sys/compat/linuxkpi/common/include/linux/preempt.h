#ifndef __LINUX_PREEMPT_H
#define __LINUX_PREEMPT_H

#include <linux/linkage.h>
#include <linux/list.h>

#define in_interrupt() (TD_IS_IDLETHREAD(curthread))

#endif
