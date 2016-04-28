#ifndef _LINUX_PID_H
#define _LINUX_PID_H

#include <linux/rcupdate.h>

enum pid_type
{
	PIDTYPE_PID,
	PIDTYPE_PGID,
	PIDTYPE_SID,
	PIDTYPE_MAX
};


extern struct task_struct *pid_task(pid_t pid, enum pid_type);
extern struct task_struct *get_pid_task(pid_t pid, enum pid_type);


#endif
