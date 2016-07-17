/*-
 * Copyright (c) 2013 Dmitry Chagin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sdt.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <amd64/linux/linux.h>
#include <amd64/linux/linux_proto.h>
#include <compat/linux/linux_dtrace.h>
#include <compat/linux/linux_util.h>

/* DTrace init */
LIN_SDT_PROVIDER_DECLARE(LINUX_DTRACE);


DUMMY(adjtimex);
DUMMY(afs_syscall);
DUMMY(create_module);
DUMMY(delete_module);
DUMMY(epoll_ctl_old);
DUMMY(epoll_wait_old);
DUMMY(finit_module);
DUMMY(get_kernel_syms);
DUMMY(getpmsg);
DUMMY(get_mempolicy);
DUMMY(init_module);
DUMMY(kexec_load);
DUMMY(kcmp);
DUMMY(lookup_dcookie);
DUMMY(mbind);
DUMMY(mq_open);
DUMMY(mq_unlink);
DUMMY(mq_timedsend);
DUMMY(mq_timedreceive);
DUMMY(mq_notify);
DUMMY(mq_getsetattr);
DUMMY(nfsservctl);
DUMMY(pivot_root);
DUMMY(putpmsg);
DUMMY(query_module);
DUMMY(quotactl);
DUMMY(remap_file_pages);
DUMMY(security);
DUMMY(semtimedop);
DUMMY(sendfile);
DUMMY(setfsuid);
DUMMY(setfsgid);
DUMMY(set_mempolicy);
DUMMY(set_thread_area);
DUMMY(sysfs);
DUMMY(syslog);
DUMMY(swapoff);
DUMMY(tuxcall);
DUMMY(vhangup);

/* linux 2.6.11: */
DUMMY(add_key);
DUMMY(request_key);
DUMMY(keyctl);
/* linux 2.6.13: */
DUMMY(ioprio_set);
DUMMY(ioprio_get);
DUMMY(inotify_init);
DUMMY(inotify_add_watch);
DUMMY(inotify_rm_watch);
/* linux 2.6.16: */
DUMMY(migrate_pages);
DUMMY(unshare);
/* linux 2.6.17: */
DUMMY(splice);
DUMMY(sync_file_range);
DUMMY(tee);
DUMMY(vmsplice);
/* linux 2.6.18: */
DUMMY(move_pages);
/* linux 2.6.19: */
DUMMY(getcpu);
/* linux 2.6.22: */
DUMMY(signalfd);
DUMMY(timerfd_create);
/* linux 2.6.25: */
DUMMY(timerfd_settime);
DUMMY(timerfd_gettime);
/* linux 2.6.27: */
DUMMY(signalfd4);
DUMMY(inotify_init1);
/* linux 2.6.30: */
DUMMY(preadv);
DUMMY(pwritev);
/* linux 2.6.31 */
DUMMY(rt_tsigqueueinfo);
DUMMY(perf_event_open);
/* linux 2.6.33: */
DUMMY(fanotify_init);
DUMMY(fanotify_mark);
/* later: */
DUMMY(name_to_handle_at);
DUMMY(open_by_handle_at);
DUMMY(clock_adjtime);
DUMMY(setns);
DUMMY(process_vm_readv);
DUMMY(process_vm_writev);

#define DUMMY_XATTR(s)						\
int								\
linux_ ## s ## xattr(						\
    struct thread *td, struct linux_ ## s ## xattr_args *arg)	\
{								\
								\
	return (ENOATTR);					\
}
DUMMY_XATTR(set);
DUMMY_XATTR(lset);
DUMMY_XATTR(fset);
DUMMY_XATTR(get);
DUMMY_XATTR(lget);
DUMMY_XATTR(fget);
DUMMY_XATTR(list);
DUMMY_XATTR(llist);
DUMMY_XATTR(flist);
DUMMY_XATTR(remove);
DUMMY_XATTR(lremove);
DUMMY_XATTR(fremove);
