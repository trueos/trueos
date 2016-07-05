/*-
 * Copyright (c) 2001 Alexander Kabaev
 * Copyright (c) 2016 Matthew Macy
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
 *    derived from this software without specific prior written permission.
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

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/syscallsubr.h>
#include <sys/systm.h>


#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/reg.h>

#include <amd64/linux/linux.h>
#include <amd64/linux/linux_proto.h>
#include <compat/linux/linux_signal.h>

#define CPU_ENABLE_SSE

/*
 *   Linux ptrace requests numbers. Mostly identical to FreeBSD,
 *   except for MD ones and PT_ATTACH/PT_DETACH.
 */
#define	PTRACE_TRACEME		0
#define	PTRACE_PEEKTEXT		1
#define	PTRACE_PEEKDATA		2
#define	PTRACE_PEEKUSR		3
#define	PTRACE_POKETEXT		4
#define	PTRACE_POKEDATA		5
#define	PTRACE_POKEUSR		6
#define	PTRACE_CONT		7
#define	PTRACE_KILL		8
#define	PTRACE_SINGLESTEP	9

#define PTRACE_ATTACH		16
#define PTRACE_DETACH		17

#define	PTRACE_SYSCALL		24

#define PTRACE_GETREGS		12
#define PTRACE_SETREGS		13
#define PTRACE_GETFPREGS	14
#define PTRACE_SETFPREGS	15
#define PTRACE_GETFPXREGS	18
#define PTRACE_SETFPXREGS	19

#define PTRACE_SETOPTIONS	0x4200
#define PTRACE_GETSIGINFO	0x4202
#define PTRACE_SETSIGINFO	0x4203
#define PTRACE_GETREGSET	0x4204
#define PTRACE_SETREGSET	0x4205
#define PTRACE_SEIZE		0x4206


/* Wait extended result codes for the above trace options.  */
#define PTRACE_EVENT_FORK	1
#define PTRACE_EVENT_VFORK	2
#define PTRACE_EVENT_CLONE	3
#define PTRACE_EVENT_EXEC	4
#define PTRACE_EVENT_VFORK_DONE	5
#define PTRACE_EVENT_EXIT	6
#define PTRACE_EVENT_SECCOMP	7
/* Extended result codes which enabled by means other than options.  */
#define PTRACE_EVENT_STOP	128

/* Options set using PTRACE_SETOPTIONS or using PTRACE_SEIZE @data param */
#define PTRACE_O_TRACESYSGOOD	1
#define PTRACE_O_TRACEFORK	(1 << PTRACE_EVENT_FORK)
#define PTRACE_O_TRACEVFORK	(1 << PTRACE_EVENT_VFORK)
#define PTRACE_O_TRACECLONE	(1 << PTRACE_EVENT_CLONE)
#define PTRACE_O_TRACEEXEC	(1 << PTRACE_EVENT_EXEC)
#define PTRACE_O_TRACEVFORKDONE	(1 << PTRACE_EVENT_VFORK_DONE)
#define PTRACE_O_TRACEEXIT	(1 << PTRACE_EVENT_EXIT)
#define PTRACE_O_TRACESECCOMP	(1 << PTRACE_EVENT_SECCOMP)

/* eventless options */
#define PTRACE_O_EXITKILL		(1 << 20)
#define PTRACE_O_SUSPEND_SECCOMP	(1 << 21)

#define PTRACE_O_MASK		(\
	0x000000ff | PTRACE_O_EXITKILL | PTRACE_O_SUSPEND_SECCOMP)



/*
 * Linux keeps debug registers at the following
 * offset in the user struct
 */
#define LINUX_DBREG_OFFSET	252
#define LINUX_DBREG_SIZE	(8*sizeof(l_int))

static __inline int
map_signum(int signum)
{
	if (!signum)
		return (0);
	
	signum = linux_to_bsd_signal(signum);
	return ((signum == SIGSTOP)? 0 : signum);
}

struct linux_pt_reg {
	unsigned long	r15;
	unsigned long	r14;
	unsigned long	r13;
	unsigned long	r12;
	unsigned long	bp;
	unsigned long	bx;
	unsigned long	r11;
	unsigned long	r10;
	unsigned long	r9;
	unsigned long	r8;
	unsigned long	ax;
	unsigned long	cx;
	unsigned long	dx;
	unsigned long	si;
	unsigned long	di;
	unsigned long	orig_ax;
	unsigned long	ip;
	unsigned long	cs;
	unsigned long	flags;
	unsigned long	sp;
	unsigned long	ss;
	unsigned long	fs_base;
	unsigned long	gs_base;
	unsigned long	ds;
	unsigned long	es;
	unsigned long	fs;
	unsigned long	gs;
};


/*
 *   Translate i386 ptrace registers between Linux and FreeBSD formats.
 *   The translation is pretty straighforward, for all registers, but
 *   orig_eax on Linux side and r_trapno and r_err in FreeBSD
 */
static void
map_regs_to_linux(struct reg *bsd_r, struct linux_pt_reg *linux_r)
{
	linux_r->r15 = bsd_r->r_r15;
	linux_r->r14 = bsd_r->r_r14;
	linux_r->r13 = bsd_r->r_r13;
	linux_r->r12 = bsd_r->r_r12;
	linux_r->r11 = bsd_r->r_r11;
	linux_r->r10 = bsd_r->r_r10;
	linux_r->r9 = bsd_r->r_r9;
	linux_r->r8 = bsd_r->r_r8;
	linux_r->di = bsd_r->r_rdi;
	linux_r->si = bsd_r->r_rsi;
	linux_r->bp = bsd_r->r_rbp;
	linux_r->bx = bsd_r->r_rbx;
	linux_r->dx = bsd_r->r_rdx;
	linux_r->cx = bsd_r->r_rcx;
	linux_r->ax = bsd_r->r_rax;
	linux_r->orig_ax = bsd_r->r_rax;
	linux_r->fs = bsd_r->r_fs;
	linux_r->gs = bsd_r->r_gs;
	linux_r->es = bsd_r->r_es;
	linux_r->ds = bsd_r->r_ds;
	linux_r->ip = bsd_r->r_rip;
	linux_r->cs = bsd_r->r_cs;
	linux_r->flags = bsd_r->r_rflags;
	linux_r->sp = bsd_r->r_rsp;
	linux_r->ss = bsd_r->r_ss;

	linux_r->fs_base = 0;
	linux_r->gs_base = 0;
}


static void
map_regs_from_linux(struct reg *bsd_r, struct linux_pt_reg *linux_r)
{
	bsd_r->r_r15 = linux_r->r15;
	bsd_r->r_r14 = linux_r->r14;
	bsd_r->r_r13 = linux_r->r13;
	bsd_r->r_r12 = linux_r->r12;
	bsd_r->r_r11 = linux_r->r11;
	bsd_r->r_r10 = linux_r->r10;
	bsd_r->r_r9 = linux_r->r9;
	bsd_r->r_r8 = linux_r->r8;

	bsd_r->r_rdi = linux_r->di;
	bsd_r->r_rsi = linux_r->si;
	bsd_r->r_rbp = linux_r->bp;
	bsd_r->r_rbx = linux_r->bx;
	bsd_r->r_rdx = linux_r->dx;
	bsd_r->r_rcx = linux_r->cx;
	bsd_r->r_rax = linux_r->ax;

	bsd_r->r_fs = linux_r->fs;
	bsd_r->r_gs = linux_r->gs;
	bsd_r->r_es = linux_r->es;
	bsd_r->r_ds = linux_r->ds;
	bsd_r->r_rip = linux_r->ip;
	bsd_r->r_cs = linux_r->cs;
	bsd_r->r_rflags = linux_r->flags;
	bsd_r->r_rsp = linux_r->sp;
	bsd_r->r_ss = linux_r->ss;
}



struct linux_pt_fpreg {
	unsigned short	cwd;
	unsigned short	swd;
	unsigned short	twd;	/* Note this is not the same as
				   the 32bit/x87/FSAVE twd */
	unsigned short	fop;
	uint64_t	rip;
	uint64_t	rdp;
	uint32_t	mxcsr;
	uint32_t	mxcsr_mask;
	uint32_t	st_space[32];	/* 8*16 bytes for each FP-reg = 128 bytes */
	uint32_t	xmm_space[64];	/* 16*16 bytes for each XMM-reg = 256 bytes */
	uint32_t	padding[24];
};


static void
map_fpregs_to_linux(struct fpreg *bsd_r, struct linux_pt_fpreg *linux_r)
{
	linux_r->cwd = bsd_r->fpr_env[0];
	linux_r->swd = bsd_r->fpr_env[1];
	linux_r->twd = bsd_r->fpr_env[2];
	linux_r->fop = bsd_r->fpr_env[3];
	bcopy(bsd_r->fpr_acc, linux_r->st_space, sizeof(linux_r->st_space));
}

static void
map_fpregs_from_linux(struct fpreg *bsd_r, struct linux_pt_fpreg *linux_r)
{
	bsd_r->fpr_env[0] = linux_r->cwd;
	bsd_r->fpr_env[1] = linux_r->swd;
	bsd_r->fpr_env[2] = linux_r->twd;
	bsd_r->fpr_env[3] = linux_r->fop;
	bcopy(bsd_r->fpr_acc, linux_r->st_space, sizeof(bsd_r->fpr_acc));
}

struct linux_pt_fpxreg {
	l_ushort	cwd;
	l_ushort	swd;
	l_ushort	twd;
	l_ushort	fop;
	l_long		fip;
	l_long		fcs;
	l_long		foo;
	l_long		fos;
	l_long		mxcsr;
	l_long		reserved;
	l_long		st_space[32];
	l_long		xmm_space[32];
	l_long		padding[56];
};

static int
linux_proc_read_fpxregs(struct thread *td, struct linux_pt_fpxreg *fpxregs)
{

	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);
	if (cpu_fxsr == 0 || (td->td_proc->p_flag & P_INMEM) == 0)
		return (EIO);
	bcopy(&get_pcb_user_save_td(td)->sv_xmm, fpxregs, sizeof(*fpxregs));
	return (0);
}

static int
linux_proc_write_fpxregs(struct thread *td, struct linux_pt_fpxreg *fpxregs)
{

	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);
	if (cpu_fxsr == 0 || (td->td_proc->p_flag & P_INMEM) == 0)
		return (EIO);
	bcopy(fpxregs, &get_pcb_user_save_td(td)->sv_xmm, sizeof(*fpxregs));
	return (0);
}

static int
do_ptrace_options(struct thread *td, int pid, unsigned long data)
{
	if (data & (PTRACE_O_TRACEFORK|PTRACE_O_TRACEVFORK|PTRACE_O_TRACECLONE))
		return (kern_ptrace(td, PT_FOLLOW_FORK, pid, 0, 1));

	return (0);
}

int
linux_ptrace(struct thread *td, struct linux_ptrace_args *uap)
{
	union {
		struct linux_pt_reg	reg;
		struct linux_pt_fpreg	fpreg;
		struct linux_pt_fpxreg	fpxreg;
	} r;
	union {
		struct reg		bsd_reg;
		struct fpreg		bsd_fpreg;
		struct dbreg		bsd_dbreg;
	} u;
	struct ptrace_lwpinfo pl;
	l_siginfo_t lsiginfo;
	void *addr;
	pid_t pid;
	int error, req, sig, count;

	count = error = 0;

	/* by default, just copy data intact */
	req  = uap->req;
	pid  = (pid_t)uap->pid;
	addr = (void *)uap->addr;

	switch (req) {
	case PTRACE_TRACEME:
	case PTRACE_POKETEXT:
	case PTRACE_POKEDATA:
	case PTRACE_KILL:
		error = kern_ptrace(td, req, pid, addr, uap->data);
		break;
	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA: {
		/* need to preserve return value */
		int rval = td->td_retval[0];
		error = kern_ptrace(td, req, pid, addr, 0);
		if (error == 0)
			error = copyout(td->td_retval, (void *)uap->data,
			    sizeof(l_int));
		td->td_retval[0] = rval;
		break;
	}
	case PTRACE_DETACH:
		error = kern_ptrace(td, PT_DETACH, pid, (void *)1,
				    map_signum(uap->data));
		break;
	case PTRACE_SINGLESTEP:
	case PTRACE_CONT:
		error = kern_ptrace(td, req, pid, (void *)1,
		     map_signum(uap->data));
		break;
	case PTRACE_ATTACH:
		error = kern_ptrace(td, PT_ATTACH, pid, addr, uap->data);
		break;
	case PTRACE_GETREGS:
	case PTRACE_GETREGSET:
		/* Linux is using data where FreeBSD is using addr */
		error = kern_ptrace(td, PT_GETREGS, pid, &u.bsd_reg, 0);
		if (error == 0) {
			map_regs_to_linux(&u.bsd_reg, &r.reg);
			error = copyout(&r.reg, (void *)uap->data,
			    sizeof(r.reg));
		}
		break;
	case PTRACE_SETREGS:
	case PTRACE_SETREGSET:
		/* Linux is using data where FreeBSD is using addr */
		error = copyin((void *)uap->data, &r.reg, sizeof(r.reg));
		if (error == 0) {
			map_regs_from_linux(&u.bsd_reg, &r.reg);
			error = kern_ptrace(td, PT_SETREGS, pid, &u.bsd_reg, 0);
		}
		break;
	case PTRACE_GETFPREGS:
		/* Linux is using data where FreeBSD is using addr */
		error = kern_ptrace(td, PT_GETFPREGS, pid, &u.bsd_fpreg, 0);
		if (error == 0) {
			map_fpregs_to_linux(&u.bsd_fpreg, &r.fpreg);
			error = copyout(&r.fpreg, (void *)uap->data,
			    sizeof(r.fpreg));
		}
		break;
	case PTRACE_SETFPREGS:
		/* Linux is using data where FreeBSD is using addr */
		error = copyin((void *)uap->data, &r.fpreg, sizeof(r.fpreg));
		if (error == 0) {
			map_fpregs_from_linux(&u.bsd_fpreg, &r.fpreg);
			error = kern_ptrace(td, PT_SETFPREGS, pid,
			    &u.bsd_fpreg, 0);
		}
		break;
	case PTRACE_SETFPXREGS:
#ifdef CPU_ENABLE_SSE
		error = copyin((void *)uap->data, &r.fpxreg, sizeof(r.fpxreg));
		if (error)
			break;
#endif
		/* FALL THROUGH */
	case PTRACE_GETFPXREGS: {
#ifdef CPU_ENABLE_SSE
		struct proc *p;
		struct thread *td2;

		if (sizeof(struct linux_pt_fpxreg) != sizeof(struct savexmm)) {
			static int once = 0;
			if (!once) {
				printf("linux: savexmm != linux_pt_fpxreg\n");
				once = 1;
			}
			error = EIO;
			break;
		}

		if ((p = pfind(uap->pid)) == NULL) {
			error = ESRCH;
			break;
		}

		/* Exiting processes can't be debugged. */
		if ((p->p_flag & P_WEXIT) != 0) {
			error = ESRCH;
			goto fail;
		}

		if ((error = p_candebug(td, p)) != 0)
			goto fail;

		/* System processes can't be debugged. */
		if ((p->p_flag & P_SYSTEM) != 0) {
			error = EINVAL;
			goto fail;
		}

		/* not being traced... */
		if ((p->p_flag & P_TRACED) == 0) {
			error = EPERM;
			goto fail;
		}

		/* not being traced by YOU */
		if (p->p_pptr != td->td_proc) {
			error = EBUSY;
			goto fail;
		}

		/* not currently stopped */
		if (!P_SHOULDSTOP(p) || (p->p_flag & P_WAITED) == 0) {
			error = EBUSY;
			goto fail;
		}

		if (req == PTRACE_GETFPXREGS) {
			_PHOLD(p);	/* may block */
			td2 = FIRST_THREAD_IN_PROC(p);
			error = linux_proc_read_fpxregs(td2, &r.fpxreg);
			_PRELE(p);
			PROC_UNLOCK(p);
			if (error == 0)
				error = copyout(&r.fpxreg, (void *)uap->data,
				    sizeof(r.fpxreg));
		} else {
			/* clear dangerous bits exactly as Linux does*/
			r.fpxreg.mxcsr &= 0xffbf;
			_PHOLD(p);	/* may block */
			td2 = FIRST_THREAD_IN_PROC(p);
			error = linux_proc_write_fpxregs(td2, &r.fpxreg);
			_PRELE(p);
			PROC_UNLOCK(p);
		}
		break;

	fail:
		PROC_UNLOCK(p);
#else
		error = EIO;
#endif
		break;
	}
	case PTRACE_PEEKUSR:
	case PTRACE_POKEUSR: {
		error = EIO;

		/* check addr for alignment */
		if (uap->addr < 0 || uap->addr & (sizeof(l_int) - 1))
			break;
		/*
		 * Allow linux programs to access register values in
		 * user struct. We simulate this through PT_GET/SETREGS
		 * as necessary.
		 */
		if (uap->addr < sizeof(struct linux_pt_reg)) {
			error = kern_ptrace(td, PT_GETREGS, pid, &u.bsd_reg, 0);
			if (error != 0)
				break;

			map_regs_to_linux(&u.bsd_reg, &r.reg);
			if (req == PTRACE_PEEKUSR) {
				error = copyout((char *)&r.reg + uap->addr,
				    (void *)uap->data, sizeof(l_int));
				break;
			}

			*(l_int *)((char *)&r.reg + uap->addr) =
			    (l_int)uap->data;

			map_regs_from_linux(&u.bsd_reg, &r.reg);
			error = kern_ptrace(td, PT_SETREGS, pid, &u.bsd_reg, 0);
		}

		/*
		 * Simulate debug registers access
		 */
		if (uap->addr >= LINUX_DBREG_OFFSET &&
		    uap->addr <= LINUX_DBREG_OFFSET + LINUX_DBREG_SIZE) {
			error = kern_ptrace(td, PT_GETDBREGS, pid, &u.bsd_dbreg,
			    0);
			if (error != 0)
				break;

			uap->addr -= LINUX_DBREG_OFFSET;
			if (req == PTRACE_PEEKUSR) {
				error = copyout((char *)&u.bsd_dbreg +
				    uap->addr, (void *)uap->data,
				    sizeof(l_int));
				break;
			}

			*(l_int *)((char *)&u.bsd_dbreg + uap->addr) =
			     uap->data;
			error = kern_ptrace(td, PT_SETDBREGS, pid,
			    &u.bsd_dbreg, 0);
		}

		break;
	}
	case PTRACE_SYSCALL:
		error = kern_ptrace(td, PT_SYSCALL, pid, 0, 0);
		break;
	case PTRACE_SETOPTIONS:
		error = do_ptrace_options(td, pid, uap->data);
		break;
	case PTRACE_GETSIGINFO:
		error = kern_ptrace(td, PT_LWPINFO, pid, &pl, 0);
		if (error)
			break;
		if ((pl.pl_flags & PL_FLAG_SI)) {
			sig = bsd_to_linux_signal(pl.pl_siginfo.si_signo);
			siginfo_to_lsiginfo(&pl.pl_siginfo, &lsiginfo, sig);
		} else {
			bzero(&lsiginfo, sizeof(lsiginfo));
		}
			
		error = copyout(&lsiginfo, (unsigned long *)uap->data, sizeof(lsiginfo));
		break;
	case PTRACE_SEIZE:
		error = EINVAL;
		break;
		/* XXX need to add extended kern_ptrace */	
		error = kern_ptrace(td, PT_ATTACH, pid, 0, 0);
		if (error) {
			printf("linux: PT_ATTACH %d\n", error);
			break;
		}
		error = do_ptrace_options(td, pid, uap->data);
		if (error) {
			printf("linux: do_ptrace_options %d\n", error);
			break;
		}
#if 0
		error = kern_wait(td, pid, &tmpstat, 0, &tmpru);
		if (error) {
			printf("linux: kern_wait %d\n", error);
			break;
		}

		while ((error = kern_ptrace(td, PT_CONTINUE, pid, (void *)1, 0)) == EBUSY && count < 10) {
			pause("dbatch", hz/10);
			count++;
		}
		if (error)
			printf("linux: PT_CONTINUE %d\n", error);
#endif		
		break;
	default:
		printf("linux: ptrace(%x, ...) not implemented\n",
		    (unsigned int)uap->req);
		error = EINVAL;
		break;
	}

	return (error);
}
