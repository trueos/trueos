
/*-
 * Copyright (c) 1995 Søren Schmidt
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/* XXX we use functions that might not exist. */
#include "opt_compat.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/capsicum.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syscallsubr.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/un.h>

#include <net/if.h>
#include <net/vnet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>

#include <compat/linux/linux_file.h>
#include <compat/linux/linux_socket.h>
#include <compat/linux/linux_timer.h>
#include <compat/linux/linux_util.h>

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))

static int
linux_to_bsd_msg_flags(int flags)
{
	int ret_flags = 0;

	if (flags & LINUX_MSG_OOB)
		ret_flags |= MSG_OOB;
	if (flags & LINUX_MSG_PEEK)
		ret_flags |= MSG_PEEK;
	if (flags & LINUX_MSG_DONTROUTE)
		ret_flags |= MSG_DONTROUTE;
	if (flags & LINUX_MSG_CTRUNC)
		ret_flags |= MSG_CTRUNC;
	if (flags & LINUX_MSG_TRUNC)
		ret_flags |= MSG_TRUNC;
	if (flags & LINUX_MSG_DONTWAIT)
		ret_flags |= MSG_DONTWAIT;
	if (flags & LINUX_MSG_EOR)
		ret_flags |= MSG_EOR;
	if (flags & LINUX_MSG_WAITALL)
		ret_flags |= MSG_WAITALL;
	if (flags & LINUX_MSG_NOSIGNAL)
		ret_flags |= MSG_NOSIGNAL;
#if 0 /* not handled */
	if (flags & LINUX_MSG_PROXY)
		;
	if (flags & LINUX_MSG_FIN)
		;
	if (flags & LINUX_MSG_SYN)
		;
	if (flags & LINUX_MSG_CONFIRM)
		;
	if (flags & LINUX_MSG_RST)
		;
	if (flags & LINUX_MSG_ERRQUEUE)
		;
#endif
	return (ret_flags);
}



struct linux_send_args {
	int s;
	l_uintptr_t msg;
	int len;
	int flags;
};

static int
linux_send(struct thread *td, struct linux_send_args *args)
{
	struct sendto_args /* {
		int s;
		caddr_t buf;
		int len;
		int flags;
		caddr_t to;
		int tolen;
	} */ bsd_args;

	bsd_args.s = args->s;
	bsd_args.buf = (caddr_t)PTRIN(args->msg);
	bsd_args.len = args->len;
	bsd_args.flags = args->flags;
	bsd_args.to = NULL;
	bsd_args.tolen = 0;
	return (sys_sendto(td, &bsd_args));
}

struct linux_recv_args {
	int s;
	l_uintptr_t msg;
	int len;
	int flags;
};

static int
linux_recv(struct thread *td, struct linux_recv_args *args)
{
	struct recvfrom_args /* {
		int s;
		caddr_t buf;
		int len;
		int flags;
		struct sockaddr *from;
		socklen_t fromlenaddr;
	} */ bsd_args;

	bsd_args.s = args->s;
	bsd_args.buf = (caddr_t)PTRIN(args->msg);
	bsd_args.len = args->len;
	bsd_args.flags = linux_to_bsd_msg_flags(args->flags);
	bsd_args.from = NULL;
	bsd_args.fromlenaddr = 0;
	return (sys_recvfrom(td, &bsd_args));
}

/* Argument list sizes for linux_socketcall */

#define LINUX_AL(x) ((x) * sizeof(l_ulong))

static const unsigned char lxs_args[] = {
	LINUX_AL(0) /* unused*/,	LINUX_AL(3) /* socket */,
	LINUX_AL(3) /* bind */,		LINUX_AL(3) /* connect */,
	LINUX_AL(2) /* listen */,	LINUX_AL(3) /* accept */,
	LINUX_AL(3) /* getsockname */,	LINUX_AL(3) /* getpeername */,
	LINUX_AL(4) /* socketpair */,	LINUX_AL(4) /* send */,
	LINUX_AL(4) /* recv */,		LINUX_AL(6) /* sendto */,
	LINUX_AL(6) /* recvfrom */,	LINUX_AL(2) /* shutdown */,
	LINUX_AL(5) /* setsockopt */,	LINUX_AL(5) /* getsockopt */,
	LINUX_AL(3) /* sendmsg */,	LINUX_AL(3) /* recvmsg */,
	LINUX_AL(4) /* accept4 */,	LINUX_AL(5) /* recvmmsg */,
	LINUX_AL(4) /* sendmmsg */
};

#define	LINUX_AL_SIZE	(nitems(lxs_args) - 1)

int
linux_socketcall(struct thread *td, struct linux_socketcall_args *args)
{
	l_ulong a[6];
	void *arg;
	int error;

	if (args->what < LINUX_SOCKET || args->what > LINUX_AL_SIZE)
		return (EINVAL);
	error = copyin(PTRIN(args->args), a, lxs_args[args->what]);
	if (error)
		return (error);

	arg = a;
	switch (args->what) {
	case LINUX_SOCKET:
		return (linux_socket(td, arg));
	case LINUX_BIND:
		return (linux_bind(td, arg));
	case LINUX_CONNECT:
		return (linux_connect(td, arg));
	case LINUX_LISTEN:
		return (linux_listen(td, arg));
	case LINUX_ACCEPT:
		return (linux_accept(td, arg));
	case LINUX_GETSOCKNAME:
		return (linux_getsockname(td, arg));
	case LINUX_GETPEERNAME:
		return (linux_getpeername(td, arg));
	case LINUX_SOCKETPAIR:
		return (linux_socketpair(td, arg));
	case LINUX_SEND:
		return (linux_send(td, arg));
	case LINUX_RECV:
		return (linux_recv(td, arg));
	case LINUX_SENDTO:
		return (linux_sendto(td, arg));
	case LINUX_RECVFROM:
		return (linux_recvfrom(td, arg));
	case LINUX_SHUTDOWN:
		return (linux_shutdown(td, arg));
	case LINUX_SETSOCKOPT:
		return (linux_setsockopt(td, arg));
	case LINUX_GETSOCKOPT:
		return (linux_getsockopt(td, arg));
	case LINUX_SENDMSG:
		return (linux_sendmsg(td, arg));
	case LINUX_RECVMSG:
		return (linux_recvmsg(td, arg));
	case LINUX_ACCEPT4:
		return (linux_accept4(td, arg));
	case LINUX_RECVMMSG:
		return (linux_recvmmsg(td, arg));
	case LINUX_SENDMMSG:
		return (linux_sendmmsg(td, arg));
	}

	uprintf("LINUX: 'socket' typ=%d not implemented\n", args->what);
	return (ENOSYS);
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */
