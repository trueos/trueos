/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
 */
#ifndef	_LINUX_PAGE_H_
#define _LINUX_PAGE_H_

#include <linux/types.h>

#include <sys/param.h>

#include <machine/atomic.h>
#include <vm/vm.h>
#include <vm/vm_page.h>

#define page	vm_page

#define	virt_to_page(x)	PHYS_TO_VM_PAGE(vtophys((x)))

#define	clear_page(page)		memset((page), 0, PAGE_SIZE)
#define	pgprot_noncached(prot)		VM_MEMATTR_UNCACHEABLE
#define	pgprot_writecombine(prot)	VM_MEMATTR_WRITE_COMBINING

#undef	PAGE_MASK
#define	PAGE_MASK	(~(PAGE_SIZE-1))
/*
 * Modifying PAGE_MASK in the above way breaks trunc_page, round_page, and btoc
 * macros.  Therefore, redefine them in a way that makes sense so linuxkpi
 * consumers don't get totally broken behavior.
 */
#undef	btoc
#define	btoc(x)	(((vm_offset_t)(x)+PAGE_SIZE-1)>>PAGE_SHIFT)
#undef	round_page
#define	round_page(x)	((((uintptr_t)(x)) + PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#undef	trunc_page
#define	trunc_page(x)	((uintptr_t)(x) & ~(PAGE_SIZE-1))

struct io_mapping {
	vm_paddr_t base;
	unsigned long size;
	vm_prot_t prot;
};

caddr_t kmap(vm_page_t page);
caddr_t kmap_atomic(vm_page_t page);
void kunmap(caddr_t vaddr);
void kunmap_atomic(caddr_t vaddr);
void mark_page_accessed(vm_page_t page);
void page_cache_release(vm_page_t page);

static inline struct
io_mapping *io_mapping_create_wc(vm_paddr_t base, unsigned long size)
{
	return (NULL);
}

void iomap_free(resource_size_t base, unsigned long size);

static inline void
io_mapping_free(struct io_mapping *mapping)
{
#ifdef notyet	
	iomap_free(mapping->base, mapping->size);
	kfree(mapping);
#endif	
}

void * iomap_atomic_prot_pfn(unsigned long pfn, vm_prot_t prot);

void iounmap_atomic(void *vaddr);


static inline void
io_mapping_unmap_atomic(void *vaddr)
{
	iounmap_atomic(vaddr);
}

void *io_mapping_map_atomic_wc(struct io_mapping *mapping, unsigned long offset);
#endif	/* _LINUX_PAGE_H_ */
