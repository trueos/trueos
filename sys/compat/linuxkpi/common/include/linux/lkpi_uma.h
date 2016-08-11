/*-
 * Copyright (c) 2002, 2003, 2004, 2005 Jeffrey Roberson <jeff@FreeBSD.org>
 * Copyright (c) 2004, 2005 Bosko Milekic <bmilekic@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 * $FreeBSD$
 *
 */

/*
 * uma.h - External definitions for the Universal Memory Allocator
 *
*/

#ifndef _LKPI_UMA_H_
#define _LKPI_UMA_H_

#include <sys/param.h>		/* For NULL */
#include <sys/malloc.h>		/* For M_* */
#include <vm/uma.h>


void lkpi_zone_drain(uma_zone_t);

/* Function proto types */

/*
 * Create a new uma zone
 *
 * Arguments:
 *	name  The text name of the zone for debugging and stats. This memory
 *		should not be freed until the zone has been deallocated.
 *	size  The size of the object that is being created.
 *	ctor  The constructor that is called when the object is allocated.
 *	dtor  The destructor that is called when the object is freed.
 *	init  An initializer that sets up the initial state of the memory.
 *	fini  A discard function that undoes initialization done by init.
 *		ctor/dtor/init/fini may all be null, see notes above.
 *	align A bitmask that corresponds to the requested alignment
 *		eg 4 would be 0x3
 *	flags A set of parameters that control the behavior of the zone.
 *
 * Returns:
 *	A pointer to a structure which is intended to be opaque to users of
 *	the interface.  The value may be null if the wait flag is not set.
 */
uma_zone_t lkpi_uma_zcreate(const char *name, size_t size, uma_ctor ctor,
			    uma_dtor dtor, uma_init uminit, uma_fini fini,
			    int align, uint32_t flags);

/*
 * Destroys an empty uma zone.  If the zone is not empty uma complains loudly.
 *
 * Arguments:
 *	zone  The zone we want to destroy.
 *
 */
void lkpi_uma_zdestroy(uma_zone_t zone);

/*
 * Allocates an item out of a zone
 *
 * Arguments:
 *	zone  The zone we are allocating from
 *	arg   This data is passed to the ctor function
 *	flags See sys/malloc.h for available flags.
 *
 * Returns:
 *	A non-null pointer to an initialized element from the zone is
 *	guaranteed if the wait flag is M_WAITOK.  Otherwise a null pointer
 *	may be returned if the zone is empty or the ctor failed.
 */

void *lkpi_uma_zalloc_arg(uma_zone_t zone, void *arg, int flags);

/*
 * Allocates an item out of a zone without supplying an argument
 *
 * This is just a wrapper for uma_zalloc_arg for convenience.
 *
 */
static __inline void *lkpi_uma_zalloc(uma_zone_t zone, int flags);

static __inline void *
lkpi_uma_zalloc(uma_zone_t zone, int flags)
{
	return (lkpi_uma_zalloc_arg(zone, NULL, flags));
}

/*
 * Frees an item back into the specified zone.
 *
 * Arguments:
 *	zone  The zone the item was originally allocated out of.
 *	item  The memory to be freed.
 *	arg   Argument passed to the destructor
 *
 * Returns:
 *	Nothing.
 */

void lkpi_uma_zfree_arg(uma_zone_t zone, void *item, void *arg);

/*
 * Frees an item back to a zone without supplying an argument
 *
 * This is just a wrapper for uma_zfree_arg for convenience.
 *
 */
static __inline void
lkpi_uma_zfree(uma_zone_t zone, void *item)
{
	lkpi_uma_zfree_arg(zone, item, NULL);
}

/*
 * XXX The rest of the prototypes in this header are h0h0 magic for the VM.
 * If you think you need to use it for a normal zone you're probably incorrect.
 */

/*
 * Finishes starting up the allocator.  This should
 * be called when kva is ready for normal allocs.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	Nothing
 *
 * Discussion:
 *	uma_startup2 is called by kmeminit() to enable us of uma for malloc.
 */

void lkpi_uma_startup2(void);

/*
 * Reclaims unused memory for all zones
 *
 * Arguments:
 *	None
 * Returns:
 *	None
 *
 * This should only be called by the page out daemon.
 */

void lkpi_uma_reclaim(void);

/*
 * Sets the alignment mask to be used for all zones requesting cache
 * alignment.  Should be called by MD boot code prior to starting VM/UMA.
 *
 * Arguments:
 *	align The alignment mask
 *
 * Returns:
 *	Nothing
 */
void lkpi_uma_set_align(int align);

/*
 * Set a reserved number of items to hold for M_USE_RESERVE allocations.  All
 * other requests must allocate new backing pages.
 */
void lkpi_uma_zone_reserve(uma_zone_t zone, int nitems);

/*
 * Reserves the maximum KVA space required by the zone and configures the zone
 * to use a VM_ALLOC_NOOBJ-based backend allocator.
 *
 * Arguments:
 *	zone  The zone to update.
 *	nitems  The upper limit on the number of items that can be allocated.
 *
 * Returns:
 *	0  if KVA space can not be allocated
 *	1  if successful
 *
 * Discussion:
 *	When the machine supports a direct map and the zone's items are smaller
 *	than a page, the zone will use the direct map instead of allocating KVA
 *	space.
 */
int lkpi_uma_zone_reserve_kva(uma_zone_t zone, int nitems);

/*
 * Sets a high limit on the number of items allowed in a zone
 *
 * Arguments:
 *	zone  The zone to limit
 *	nitems  The requested upper limit on the number of items allowed
 *
 * Returns:
 *	int  The effective value of nitems after rounding up based on page size
 */
int lkpi_uma_zone_set_max(uma_zone_t zone, int nitems);

/*
 * Obtains the effective limit on the number of items in a zone
 *
 * Arguments:
 *	zone  The zone to obtain the effective limit from
 *
 * Return:
 *	0  No limit
 *	int  The effective limit of the zone
 */
int lkpi_uma_zone_get_max(uma_zone_t zone);

/*
 * Sets a warning to be printed when limit is reached
 *
 * Arguments:
 *	zone  The zone we will warn about
 *	warning  Warning content
 *
 * Returns:
 *	Nothing
 */
void lkpi_uma_zone_set_warning(uma_zone_t zone, const char *warning);

/*
 * Sets a function to run when limit is reached
 *
 * Arguments:
 *	zone  The zone to which this applies
 *	fx  The function ro run
 *
 * Returns:
 *	Nothing
 */
void lkpi_uma_zone_set_maxaction(uma_zone_t zone, uma_maxaction_t);

/*
 * Obtains the approximate current number of items allocated from a zone
 *
 * Arguments:
 *	zone  The zone to obtain the current allocation count from
 *
 * Return:
 *	int  The approximate current number of items allocated from the zone
 */
int lkpi_uma_zone_get_cur(uma_zone_t zone);

/*
 * The following two routines (uma_zone_set_init/fini)
 * are used to set the backend init/fini pair which acts on an
 * object as it becomes allocated and is placed in a slab within
 * the specified zone's backing keg.  These should probably not
 * be changed once allocations have already begun, but only be set
 * immediately upon zone creation.
 */
void lkpi_uma_zone_set_init(uma_zone_t zone, uma_init uminit);
void lkpi_uma_zone_set_fini(uma_zone_t zone, uma_fini fini);

/*
 * The following two routines (uma_zone_set_zinit/zfini) are
 * used to set the zinit/zfini pair which acts on an object as
 * it passes from the backing Keg's slab cache to the
 * specified Zone's bucket cache.  These should probably not
 * be changed once allocations have already begun, but only be set
 * immediately upon zone creation.
 */
void lkpi_uma_zone_set_zinit(uma_zone_t zone, uma_init zinit);
void lkpi_uma_zone_set_zfini(uma_zone_t zone, uma_fini zfini);

/*
 * Replaces the standard backend allocator for this zone.
 *
 * Arguments:
 *	zone   The zone whose backend allocator is being changed.
 *	allocf A pointer to the allocation function
 *
 * Returns:
 *	Nothing
 *
 * Discussion:
 *	This could be used to implement pageable allocation, or perhaps
 *	even DMA allocators if used in conjunction with the OFFPAGE
 *	zone flag.
 */

void lkpi_uma_zone_set_allocf(uma_zone_t zone, uma_alloc allocf);

/*
 * Used for freeing memory provided by the allocf above
 *
 * Arguments:
 *	zone  The zone that intends to use this free routine.
 *	freef The page freeing routine.
 *
 * Returns:
 *	Nothing
 */

void lkpi_uma_zone_set_freef(uma_zone_t zone, uma_free freef);

/*
 * Used to pre-fill a zone with some number of items
 *
 * Arguments:
 *	zone    The zone to fill
 *	itemcnt The number of items to reserve
 *
 * Returns:
 *	Nothing
 *
 * NOTE: This is blocking and should only be done at startup
 */
void lkpi_uma_prealloc(uma_zone_t zone, int itemcnt);

/*
 * Used to determine if a fixed-size zone is exhausted.
 *
 * Arguments:
 *	zone    The zone to check
 *
 * Returns:
 *	Non-zero if zone is exhausted.
 */
int lkpi_uma_zone_exhausted(uma_zone_t zone);
int lkpi_uma_zone_exhausted_nolock(uma_zone_t zone);

void lkpi_uma_reclaim_wakeup(void);
void lkpi_uma_reclaim_worker(void *);

#endif	/* _VM_UMA_H_ */
