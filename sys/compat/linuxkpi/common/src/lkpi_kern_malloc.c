/*-
 * Copyright (c) 1987, 1991, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2005-2009 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_malloc.c	8.3 (Berkeley) 1/4/94
 */

/*
 * Kernel malloc(9) implementation -- general purpose kernel memory allocator
 * based on memory types.  Back end is implemented using the UMA(9) zone
 * allocator.  A set of fixed-size buckets are used for smaller allocations,
 * and a special UMA allocation interface is used for larger allocations.
 * Callers declare memory types, and statistics are maintained independently
 * for each memory type.  Statistics are maintained per-CPU for performance
 * reasons.  See malloc(9) and comments in malloc.h for a detailed
 * description.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/vmmeter.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/vmem.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_pageout.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <vm/uma_dbg.h>

#ifdef DEBUG_MEMGUARD
#include <vm/memguard.h>
#endif
#ifdef DEBUG_REDZONE
#include <vm/redzone.h>
#endif

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>

dtrace_malloc_probe_func_t	dtrace_malloc_probe;
#endif

/*
 * When realloc() is called, if the new size is sufficiently smaller than
 * the old size, realloc() will allocate a new, smaller block to avoid
 * wasting memory. 'Sufficiently smaller' is defined as: newsize <=
 * oldsize / 2^n, where REALLOC_FRACTION defines the value of 'n'.
 */
#ifndef REALLOC_FRACTION
#define	REALLOC_FRACTION	1	/* new block if <= half the size */
#endif


#include <linux/lkpi_uma.h>
#include <linux/lkpi_malloc.h>
#include <machine/cpufunc.h>

#define KMEM_ZSHIFT	4
#define KMEM_ZBASE	16
#define KMEM_ZMASK	(KMEM_ZBASE - 1)

#define KMEM_ZMAX	32768
#define KMEM_ZSIZE	(KMEM_ZMAX >> KMEM_ZSHIFT)
static uint8_t kmemsize[KMEM_ZSIZE + 1];

#ifndef MALLOC_DEBUG_MAXZONES
#define	MALLOC_DEBUG_MAXZONES	1
#endif
static int numzones = MALLOC_DEBUG_MAXZONES;

/*
 * Small malloc(9) memory allocations are allocated from a set of UMA buckets
 * of various sizes.
 *
 * XXX: The comment here used to read "These won't be powers of two for
 * long."  It's possible that a significant amount of wasted memory could be
 * recovered by tuning the sizes of these buckets.
 */
struct {
	int kz_size;
	char *kz_name;
	uma_zone_t kz_zone[MALLOC_DEBUG_MAXZONES];
} lkpi_kmemzones[] = {
	{16, "16", },
	{32, "32", },
	{64, "64", },
	{128, "128", },
	{256, "256", },
	{512, "512", },
	{1024, "1024", },
	{2048, "2048", },
	{4096, "4096", },
	{8192, "8192", },
	{16384, "16384", },
	{32768, "32768", },
	{0, NULL},
};

static u_long kmem_zmax = KMEM_ZMAX;
static time_t t_malloc_fail;

/*
 * An allocation has succeeded -- update malloc type statistics for the
 * amount of bucket size.  Occurs within a critical section so that the
 * thread isn't preempted and doesn't migrate while updating per-PCU
 * statistics.
 */
static void
malloc_type_zone_allocated(struct malloc_type *mtp, unsigned long size,
    int zindx)
{
	struct malloc_type_internal *mtip;
	struct malloc_type_stats *mtsp;

	disable_intr();
	mtip = mtp->ks_handle;
	mtsp = &mtip->mti_stats[curcpu];
	if (size > 0) {
		mtsp->mts_memalloced += size;
		mtsp->mts_numallocs++;
	}
	if (zindx != -1)
		mtsp->mts_size |= 1 << zindx;

#ifdef KDTRACE_HOOKS
	if (dtrace_malloc_probe != NULL) {
		uint32_t probe_id = mtip->mti_probes[DTMALLOC_PROBE_MALLOC];
		if (probe_id != 0)
			(dtrace_malloc_probe)(probe_id,
			    (uintptr_t) mtp, (uintptr_t) mtip,
			    (uintptr_t) mtsp, size, zindx);
	}
#endif

	enable_intr();
}


/*
 * Initialize the kernel memory allocator
 */
/* ARGSUSED*/
static void
lkpi_mallocinit(void *dummy __unused)
{
	int i;
	uint8_t indx;

	lkpi_uma_startup2();

	if (kmem_zmax < PAGE_SIZE || kmem_zmax > KMEM_ZMAX)
		kmem_zmax = KMEM_ZMAX;

	for (i = 0, indx = 0; lkpi_kmemzones[indx].kz_size != 0; indx++) {
		int size = lkpi_kmemzones[indx].kz_size;
		char *name = lkpi_kmemzones[indx].kz_name;
		int subzone;

		for (subzone = 0; subzone < numzones; subzone++) {
			lkpi_kmemzones[indx].kz_zone[subzone] =
				lkpi_uma_zcreate(name, size,
#ifdef INVARIANTS
			    mtrash_ctor, mtrash_dtor, mtrash_init, mtrash_fini,
#else
			    NULL, NULL, NULL, NULL,
#endif
			    UMA_ALIGN_PTR, UMA_ZONE_MALLOC);
		}		    
		for (;i <= size; i+= KMEM_ZBASE)
			kmemsize[i >> KMEM_ZSHIFT] = indx;

	}
}
SYSINIT(lkpi_kmem, SI_SUB_KMEM, SI_ORDER_SECOND, lkpi_mallocinit, NULL);

/*
 *	malloc:
 *
 *	Allocate a block of memory.
 *
 *	If M_NOWAIT is set, this routine will not block and return NULL if
 *	the allocation fails.
 */
void *
lkpi_malloc(unsigned long size, struct malloc_type *mtp, int flags)
{
	int indx;
	struct malloc_type_internal *mtip;
	caddr_t va;
	uma_zone_t zone;
	struct thread *td = curthread;
#if defined(DIAGNOSTIC) || defined(DEBUG_REDZONE)
	unsigned long osize = size;
#endif
	if (td->td_critnest || td->td_intr_nesting_level)
		flags |= M_NOVM | M_USE_RESERVE;

#ifdef INVARIANTS
	KASSERT(mtp->ks_magic == M_MAGIC, ("malloc: bad malloc type magic"));
	/*
	 * Check that exactly one of M_WAITOK or M_NOWAIT is specified.
	 */
	indx = flags & (M_WAITOK | M_NOWAIT);
	if (indx != M_NOWAIT && indx != M_WAITOK) {
		static	struct timeval lasterr;
		static	int curerr, once;
		if (once == 0 && ppsratecheck(&lasterr, &curerr, 1)) {
			printf("Bad malloc flags: %x\n", indx);
			kdb_backtrace();
			flags |= M_WAITOK;
			once++;
		}
	}
#endif
#ifdef MALLOC_MAKE_FAILURES
	if ((flags & M_NOWAIT) && (malloc_failure_rate != 0)) {
		atomic_add_int(&malloc_nowait_count, 1);
		if ((malloc_nowait_count % malloc_failure_rate) == 0) {
			atomic_add_int(&malloc_failure_count, 1);
			t_malloc_fail = time_uptime;
			return (NULL);
		}
	}
#endif
	if (flags & M_WAITOK)
		KASSERT(curthread->td_intr_nesting_level == 0,
		   ("malloc(M_WAITOK) in interrupt context"));
#ifdef DEBUG_MEMGUARD
	if (memguard_cmp_mtp(mtp, size)) {
		va = memguard_alloc(size, flags);
		if (va != NULL)
			return (va);
		/* This is unfortunate but should not be fatal. */
	}
#endif

#ifdef DEBUG_REDZONE
	size = redzone_size_ntor(size);
#endif

	if (size <= kmem_zmax) {
		mtip = mtp->ks_handle;
		if (size & KMEM_ZMASK)
			size = (size & ~KMEM_ZMASK) + KMEM_ZBASE;
		indx = kmemsize[size >> KMEM_ZSHIFT];
		KASSERT(mtip->mti_zone < numzones,
		    ("mti_zone %u out of range %d",
		    mtip->mti_zone, numzones));
		zone = lkpi_kmemzones[indx].kz_zone[mtip->mti_zone];
#ifdef MALLOC_PROFILE
		krequests[size >> KMEM_ZSHIFT]++;
#endif
		va = lkpi_uma_zalloc(zone, flags);
		if (va != NULL)
			size = zone->uz_size;
		malloc_type_zone_allocated(mtp, va == NULL ? 0 : size, indx);
	} else {
		size = roundup(size, PAGE_SIZE);
		zone = NULL;
		va = uma_large_malloc(size, flags);
		malloc_type_allocated(mtp, va == NULL ? 0 : size);
	}
	if (flags & M_WAITOK)
		KASSERT(va != NULL, ("malloc(M_WAITOK) returned NULL"));
	else if (va == NULL)
		t_malloc_fail = time_uptime;
#ifdef DIAGNOSTIC
	if (va != NULL && !(flags & M_ZERO)) {
		memset(va, 0x70, osize);
	}
#endif
#ifdef DEBUG_REDZONE
	if (va != NULL)
		va = redzone_setup(va, osize);
#endif
	return ((void *) va);
}

/*
 *	free:
 *
 *	Free a block of memory allocated by malloc.
 *
 *	This routine may not block.
 */
void
lkpi_free(void *addr, struct malloc_type *mtp)
{
	uma_slab_t slab;
	u_long size;

	KASSERT(mtp->ks_magic == M_MAGIC, ("free: bad malloc type magic"));

	/* free(NULL, ...) does nothing */
	if (addr == NULL)
		return;

#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(addr)) {
		memguard_free(addr);
		return;
	}
#endif

#ifdef DEBUG_REDZONE
	redzone_check(addr);
	addr = redzone_addr_ntor(addr);
#endif

	slab = vtoslab((vm_offset_t)addr & (~UMA_SLAB_MASK));

	if (slab == NULL)
		panic("free: address %p(%p) has not been allocated.\n",
		    addr, (void *)((u_long)addr & (~UMA_SLAB_MASK)));

	if (!(slab->us_flags & UMA_SLAB_MALLOC)) {
#ifdef INVARIANTS
		struct malloc_type **mtpp = addr;
#endif
		size = slab->us_keg->uk_size;
#ifdef INVARIANTS
		/*
		 * Cache a pointer to the malloc_type that most recently freed
		 * this memory here.  This way we know who is most likely to
		 * have stepped on it later.
		 *
		 * This code assumes that size is a multiple of 8 bytes for
		 * 64 bit machines
		 */
		mtpp = (struct malloc_type **)
		    ((unsigned long)mtpp & ~UMA_ALIGN_PTR);
		mtpp += (size - sizeof(struct malloc_type *)) /
		    sizeof(struct malloc_type *);
		*mtpp = mtp;
#endif
		if (slab->us_flags & UMA_SLAB_LKPI)
			lkpi_uma_zfree_arg(LIST_FIRST(&slab->us_keg->uk_zones), addr, slab);
		else
			uma_zfree_arg(LIST_FIRST(&slab->us_keg->uk_zones), addr, slab);
	} else {
		size = slab->us_size;
		uma_large_free(slab);
	}
	malloc_type_freed(mtp, size);
}

/*
 *	realloc: change the size of a memory block
 */
void *
lkpi_realloc(void *addr, unsigned long size, struct malloc_type *mtp, int flags)
{
	uma_slab_t slab;
	unsigned long alloc;
	void *newaddr;

	KASSERT(mtp->ks_magic == M_MAGIC,
	    ("realloc: bad malloc type magic"));

	/* realloc(NULL, ...) is equivalent to malloc(...) */
	if (addr == NULL)
		return (lkpi_malloc(size, mtp, flags));

	/*
	 * XXX: Should report free of old memory and alloc of new memory to
	 * per-CPU stats.
	 */

#ifdef DEBUG_MEMGUARD
	if (is_memguard_addr(addr))
		return (memguard_realloc(addr, size, mtp, flags));
#endif

#ifdef DEBUG_REDZONE
	slab = NULL;
	alloc = redzone_get_size(addr);
#else
	slab = vtoslab((vm_offset_t)addr & ~(UMA_SLAB_MASK));

	/* Sanity check */
	KASSERT(slab != NULL,
	    ("realloc: address %p out of range", (void *)addr));

	/* Get the size of the original block */
	if (!(slab->us_flags & UMA_SLAB_MALLOC))
		alloc = slab->us_keg->uk_size;
	else
		alloc = slab->us_size;

	/* Reuse the original block if appropriate */
	if (size <= alloc
	    && (size > (alloc >> REALLOC_FRACTION) || alloc == MINALLOCSIZE))
		return (addr);
#endif /* !DEBUG_REDZONE */

	/* Allocate a new, bigger (or smaller) block */
	if ((newaddr = lkpi_malloc(size, mtp, flags)) == NULL)
		return (NULL);

	/* Copy over original contents */
	bcopy(addr, newaddr, min(size, alloc));
	lkpi_free(addr, mtp);
	return (newaddr);
}
