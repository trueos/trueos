/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2015 Mellanox Technologies, Ltd.
 * Copyright (c) 2015 François Tigeot
 * Copyright (c) 2015 Matthew Dillon <dillon@backplane.com>
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
#ifndef	_LINUX_MM_H_
#define	_LINUX_MM_H_

#include <linux/spinlock.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/mm_types.h>
#include <linux/pfn.h>

#include <asm/pgtable.h>


#define	PAGE_ALIGN(x)	ALIGN(x, PAGE_SIZE)



#define VM_NORESERVE	0x00200000	/* should the VM suppress accounting */
#define VM_PFNMAP	0x00000400	/* Page-ranges managed without "struct page", just pure PFN */

#define VM_LOCKED	0x00002000
#define VM_IO           0x00004000	/* Memory mapped I/O or similar */

#define VM_MAYREAD	0x00000010	/* limits for mprotect() etc */
#define VM_MAYWRITE	0x00000020
#define VM_MAYEXEC	0x00000040
#define VM_MAYSHARE	0x00000080

					/* Used by sys_madvise() */
#define VM_SEQ_READ	0x00008000	/* App will access data sequentially */
#define VM_RAND_READ	0x00010000	/* App will not benefit from clustered reads */

#define VM_DONTCOPY	0x00020000      /* Do not copy this vma on fork */
#define VM_DONTEXPAND	0x00040000	/* Cannot expand with mremap() */
#define VM_DONTDUMP	0x04000000	/* Do not include in the core dump */

#define VM_PFNINTERNAL	0x80000000	/* FreeBSD private flag to vm_insert_pfn */

#define VMA_MAX_PREFAULT_RECORD 1

typedef int (*pte_fn_t)(pte_t *pte, pgtable_t token, unsigned long addr,
			void *data);
struct vm_area_struct {
	vm_offset_t	vm_start;
	vm_offset_t	vm_end;
	vm_offset_t	vm_pgoff;
	vm_paddr_t	vm_pfn;		/* PFN For mmap. */
	vm_size_t	vm_len;		/* length for mmap. */
	pgprot_t	vm_page_prot;
	unsigned long vm_flags;		/* Flags, see mm.h. */
	struct mm_struct *vm_mm;	/* The address space we belong to. */
	void * vm_private_data;		/* was vm_pte (shared mem) */
	const struct vm_operations_struct *vm_ops;
	struct linux_file *vm_file;
	/* internal operation */
	int vm_pfn_count;
	int *vm_pfn_pcount;
	vm_object_t vm_obj;
	vm_map_t vm_cached_map;
};

/*
 * Compute log2 of the power of two rounded up count of pages
 * needed for size bytes.
 */
static inline int
get_order(unsigned long size)
{
	int order;

	size = (size - 1) >> PAGE_SHIFT;
	order = 0;
	while (size) {
		order++;
		size >>= 1;
	}
	return (order);
}

static inline void *
lowmem_page_address(struct page *page)
{

	return page_address(page);
}

/*
 * This only works via mmap ops.
 */
static inline int
io_remap_pfn_range(struct vm_area_struct *vma,
    unsigned long addr, unsigned long pfn, unsigned long size,
    vm_memattr_t prot)
{
	vma->vm_page_prot = prot;
	vma->vm_pfn = pfn;
	vma->vm_len = size;

	return (0);
}

static inline int
apply_to_page_range(struct mm_struct *mm, unsigned long address,
		    unsigned long size, pte_fn_t fn, void *data)
{
	panic("XXX implement me!!!");
	return (-ENOTSUP);
}

static inline int
zap_vma_ptes(struct vm_area_struct *vma, unsigned long address,
		 unsigned long size)
{
	panic("XXX implement me!!!");
	return (-ENOTSUP);
}
static inline int
remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
		unsigned long pfn, unsigned long size, pgprot_t prot)
{
	panic("XXX implement me!!!");
}


static inline unsigned long
vma_pages(struct vm_area_struct *vma)
{
	return ((vma->vm_end - vma->vm_start) >> PAGE_SHIFT);
}

#define	offset_in_page(off)	((off) & (PAGE_SIZE - 1))

static inline void
set_page_dirty(struct vm_page *page)
{
	vm_page_dirty(page);
}
static inline void
mark_page_accessed(struct vm_page *page)
{
	vm_page_reference(page);
}


static inline void
get_page(struct vm_page *page)
{
	vm_page_lock(page);
	vm_page_hold(page);
	vm_page_unlock(page);
}

long get_user_pages(unsigned long start, unsigned long nr_pages,
			    int write, int force, struct page **pages,
			    struct vm_area_struct **vmas);

/*
 * doesn't attempt to fault and will return short.
 */
int __get_user_pages_fast(unsigned long start, int nr_pages, int write,
			  struct page **pages);


long get_user_pages_remote(struct task_struct *tsk, struct mm_struct *mm,
			    unsigned long start, unsigned long nr_pages,
			    int write, int force, struct page **pages,
			   struct vm_area_struct **vmas);

#define put_page(page) __free_hot_cold_page(page);
#define copy_highpage(to, from) pmap_copy_page(from, to)

extern struct vm_area_struct * find_vma(struct mm_struct * mm, unsigned long addr);

static inline pgprot_t
vm_get_page_prot(unsigned long vm_flags)
{
	return (vm_flags & VM_PROT_ALL);
}


int vm_insert_mixed(struct vm_area_struct *vma, unsigned long addr, pfn_t pfn);

void vma_set_page_prot(struct vm_area_struct *vma);

int vm_insert_pfn(struct vm_area_struct *vma, unsigned long addr,
			unsigned long pfn);
int vm_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr,
			unsigned long pfn, pgprot_t pgprot);

static inline vm_page_t
vmalloc_to_page(const void *addr)
{
	vm_paddr_t paddr;

	paddr = pmap_kextract((vm_offset_t)addr);
	return (PHYS_TO_VM_PAGE(paddr));
}

static inline void *
vmalloc_32(unsigned long size)
{
	return (contigmalloc(size, M_KMALLOC, M_WAITOK, 0, UINT_MAX, 1, 1));

}

int is_vmalloc_addr(void *addr);

#define VM_FAULT_OOM	0x0001
#define VM_FAULT_SIGBUS	0x0002
#define VM_FAULT_MAJOR	0x0004
#define VM_FAULT_WRITE	0x0008	/* Special case for get_user_pages */
#define VM_FAULT_HWPOISON 0x0010	/* Hit poisoned small page */
#define VM_FAULT_HWPOISON_LARGE 0x0020  /* Hit poisoned large page. Index encoded in upper bits */
#define VM_FAULT_SIGSEGV 0x0040

#define VM_FAULT_NOPAGE	0x0100	/* ->fault installed the pte, not return page */
#define VM_FAULT_LOCKED	0x0200	/* ->fault locked the returned page */
#define VM_FAULT_RETRY	0x0400	/* ->fault blocked, must retry */
#define VM_FAULT_FALLBACK 0x0800	/* huge page fault failed, fall back to small */

#define FAULT_FLAG_WRITE	0x01	/* Fault was a write access */
#define FAULT_FLAG_MKWRITE	0x02	/* Fault was mkwrite of existing pte */
#define FAULT_FLAG_ALLOW_RETRY	0x04	/* Retry fault if blocking */
#define FAULT_FLAG_RETRY_NOWAIT	0x08	/* Don't drop mmap_sem and wait when retrying */
#define FAULT_FLAG_KILLABLE	0x10	/* The fault task is in SIGKILL killable region */
#define FAULT_FLAG_TRIED	0x20	/* Second try */
#define FAULT_FLAG_USER		0x40	/* The fault originated in userspace */
#define FAULT_FLAG_REMOTE	0x80	/* faulting for non current tsk/mm */
#define FAULT_FLAG_INSTRUCTION  0x100	/* The fault was during an instruction fetch */


#define VM_MIXEDMAP	0x10000000	/* Can contain "struct page" and pure PFN pages */


struct vm_fault {
	unsigned int flags;		/* FAULT_FLAG_xxx flags */
	gfp_t gfp_mask;			/* gfp mask to be used for allocations */
	pgoff_t pgoff;			/* Logical page offset based on vma */
	void __user *virtual_address;	/* Faulting virtual address */

	struct page *cow_page;		/* Handler may choose to COW */
	struct page *page;		/* ->fault handlers should return a
					 * page here, unless VM_FAULT_NOPAGE
					 * is set (which is also implied by
					 * VM_FAULT_ERROR).
					 */
	/* for ->map_pages() only */
	pgoff_t max_pgoff;		/* map pages for offset from pgoff till
					 * max_pgoff inclusive */
	pte_t *pte;			/* pte entry associated with ->pgoff */
};



struct vm_operations_struct {
	void (*open)(struct vm_area_struct * area);
	void (*close)(struct vm_area_struct * area);
	int (*mremap)(struct vm_area_struct * area);
	int (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf);
	int (*pmd_fault)(struct vm_area_struct *, unsigned long address,
						pmd_t *, unsigned int flags);
	void (*map_pages)(struct vm_area_struct *vma, struct vm_fault *vmf);

	/* notification that a previously read-only page is about to become
	 * writable, if an error is returned it will cause a SIGBUS */
	int (*page_mkwrite)(struct vm_area_struct *vma, struct vm_fault *vmf);

	/* same as page_mkwrite when using VM_PFNMAP|VM_MIXEDMAP */
	int (*pfn_mkwrite)(struct vm_area_struct *vma, struct vm_fault *vmf);

	/* called by access_process_vm when get_user_pages() fails, typically
	 * for use by special VMAs that can switch between memory and hardware
	 */
	int (*access)(struct vm_area_struct *vma, unsigned long addr,
		      void *buf, int len, int write);

};

#endif	/* _LINUX_MM_H_ */
