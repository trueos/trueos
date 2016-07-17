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


static inline struct task_struct *
pid_task(pid_t pid, enum pid_type type)
{
	struct thread *td;

	/* pid corresponds to a thread in this context */
	td = tdfind(pid, -1);
	if (td == NULL)
		return (NULL);
	return (task_struct_get(td));
}

#define pid_nr(n) (n)
#define pid_vnr(n) (n)
#define from_kuid_munged(a, uid) (uid)
#endif
