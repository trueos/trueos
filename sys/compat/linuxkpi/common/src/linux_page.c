/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2016 Matt Macy (mmacy@nextbsd.org)
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>

#include <linux/page.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include <linux/pfn_t.h>


#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/vm_extern.h>

#include <asm/smp.h>


extern u_int	cpu_feature;
extern u_int	cpu_stdext_feature;
extern int	linux_skip_prefault;

#if defined(__i386__) || defined(__amd64__)
static void
__wbinvd(void *arg)
{
	wbinvd();
}

int
wbinvd_on_all_cpus(void)
{

	return (on_each_cpu(__wbinvd, NULL, 1));
}

static int
needs_set_memattr(vm_page_t m, vm_memattr_t attr)
{
	vm_memattr_t mode;

	mode = m->md.pat_mode;

	if ((mode == 0) && !(m->flags & PG_FICTITIOUS) &&
	    (attr == VM_MEMATTR_DEFAULT))
		return (0);

	if (mode != attr)
		return (1);
	return (0);
}
#endif


int
vm_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn, pgprot_t pgprot)
{
	vm_object_t vm_obj;
	vm_page_t page;
	pmap_t pmap = vma->vm_cached_map->pmap;
	vm_memattr_t attr = pgprot2cachemode(pgprot);

	if (__predict_false(linux_skip_prefault) && (vma->vm_pfn_count > 0))
		return (-EBUSY);

	vm_obj = vma->vm_obj;
	page = PHYS_TO_VM_PAGE((pfn << PAGE_SHIFT));

#if defined(__i386__) || defined(__amd64__)
	if (needs_set_memattr(page, attr)) {
		page->flags |= PG_FICTITIOUS;
		pmap_page_set_memattr(page, attr);
	}
#endif

	MPASS(vma->vm_flags & VM_PFNINTERNAL);
	if ((vma->vm_flags & VM_PFNINTERNAL) && (vma->vm_pfn_count == 0)) {
		vm_page_tryxbusy(page);
		vma->vm_pfn_array[vma->vm_pfn_count++] = pfn;
	} else
		pmap_enter_quick(pmap, addr, page, pgprot & VM_PROT_ALL);
	return (0);
}

int
vm_insert_pfn(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn)
{

	return (vm_insert_pfn_prot(vma, addr, pfn, vma->vm_page_prot));
}

int
vm_insert_mixed(struct vm_area_struct *vma, unsigned long addr, pfn_t pfn)
{
	unsigned long pfnval;

	pfnval = pfn.val & ~PFN_FLAGS_MASK;
	return (vm_insert_pfn_prot(vma, addr, pfnval, vma->vm_page_prot));
}


void
linux_clflushopt(u_long addr)
{
	if (cpu_stdext_feature & CPUID_STDEXT_CLFLUSHOPT)
		clflushopt(addr);
	else if (cpu_feature & CPUID_CLFSH)
		clflush(addr);
	else
		pmap_invalidate_cache();
}

/*
 * Hash of vmmap addresses.  This is infrequently accessed and does not
 * need to be particularly large.  This is done because we must store the
 * caller's idea of the map size to properly unmap.
 */
struct vmmap {
	LIST_ENTRY(vmmap)	vm_next;
	void 			*vm_addr;
	unsigned long		vm_size;
};

struct vmmaphd {
	struct vmmap *lh_first;
};
#define	VMMAP_HASH_SIZE	64
#define	VMMAP_HASH_MASK	(VMMAP_HASH_SIZE - 1)
#define	VM_HASH(addr)	((uintptr_t)(addr) >> PAGE_SHIFT) & VMMAP_HASH_MASK
static struct vmmaphd vmmaphead[VMMAP_HASH_SIZE];
static struct mtx vmmaplock;

static void
vmmap_init(void *arg)
{
	int i;

	mtx_init(&vmmaplock, "IO map lock", NULL, MTX_DEF);
	for (i = 0; i < VMMAP_HASH_SIZE; i++)
		LIST_INIT(&vmmaphead[i]);

}
SYSINIT(vmmap_compat, SI_SUB_DRIVERS, SI_ORDER_SECOND, vmmap_init, NULL);



static void
vmmap_add(void *addr, unsigned long size)
{
	struct vmmap *vmmap;

	vmmap = kmalloc(sizeof(*vmmap), GFP_KERNEL);
	mtx_lock(&vmmaplock);
	vmmap->vm_size = size;
	vmmap->vm_addr = addr;
	LIST_INSERT_HEAD(&vmmaphead[VM_HASH(addr)], vmmap, vm_next);
	mtx_unlock(&vmmaplock);
}

static struct vmmap *
vmmap_remove(void *addr)
{
	struct vmmap *vmmap;

	mtx_lock(&vmmaplock);
	LIST_FOREACH(vmmap, &vmmaphead[VM_HASH(addr)], vm_next)
		if (vmmap->vm_addr == addr)
			break;
	if (vmmap)
		LIST_REMOVE(vmmap, vm_next);
	mtx_unlock(&vmmaplock);

	return (vmmap);
}

#if defined(__i386__) || defined(__amd64__)
void *
_ioremap_attr(vm_paddr_t phys_addr, unsigned long size, int attr)
{
	void *addr;

	addr = pmap_mapdev_attr(phys_addr, size, attr);
	if (addr == NULL)
		return (NULL);
	vmmap_add(addr, size);

	return (addr);
}
#endif

void
iounmap(void *addr)
{
	struct vmmap *vmmap;

	vmmap = vmmap_remove(addr);
	if (vmmap == NULL)
		return;
#if defined(__i386__) || defined(__amd64__)
	pmap_unmapdev((vm_offset_t)addr, vmmap->vm_size);
#endif
	kfree(vmmap);
}


void *
vmap(struct page **pages, unsigned int count, unsigned long flags, int prot)
{
	vm_offset_t off;
	size_t size;
	int i, attr;

	size = count * PAGE_SIZE;
	off = kva_alloc(size);
	if (off == 0)
		return (NULL);
	vmmap_add((void *)off, size);
	attr = pgprot2cachemode(prot);
	if (attr != VM_MEMATTR_DEFAULT) {
		for (i = 0; i < count; i++) {
			vm_page_lock(pages[i]);
			pages[i]->flags |= PG_FICTITIOUS;
			vm_page_unlock(pages[i]);
			pmap_page_set_memattr(pages[i], attr);
		}

	}

	pmap_qenter(off, pages, count);
	return ((void *)off);
}

void
vunmap(void *addr)
{
	struct vmmap *vmmap;

	vmmap = vmmap_remove(addr);
	if (vmmap == NULL)
		return;
	pmap_qremove((vm_offset_t)addr, vmmap->vm_size / PAGE_SIZE);
	kva_free((vm_offset_t)addr, vmmap->vm_size);
	kfree(vmmap);
}

#if defined(__LP64__)



void *
kmap(vm_page_t page)
{
	vm_offset_t daddr;

	daddr = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(page));

	return ((void *)daddr);
}

void *
kmap_atomic_prot(vm_page_t page, pgprot_t prot)
{
	vm_memattr_t attr = pgprot2cachemode(prot);

	sched_pin();
	if (attr != VM_MEMATTR_DEFAULT) {
		vm_page_lock(page);
		page->flags |= PG_FICTITIOUS;
		vm_page_unlock(page);
		pmap_page_set_memattr(page, attr);
	}
	return (kmap(page));
}

void *
kmap_atomic(vm_page_t page)
{

	return (kmap_atomic_prot(page, VM_PROT_ALL));
}

void
kunmap(vm_page_t page)
{

}

void
kunmap_atomic(void *vaddr)
{
	sched_unpin();
}


#else

static struct sf_buf *
vtosf(caddr_t vaddr)
{
	panic("IMPLEMENT ME!!!");
	return (NULL);
}

static struct sf_buf *
pagetosf(vm_page_t page)
{
	panic("IMPLEMENT ME!!!");
	return (NULL);
}

void *
kmap(vm_page_t page)
{
	struct sf_buf *sf;

	sf = sf_buf_alloc(page, SFB_NOWAIT | SFB_CPUPRIVATE);
	if (sf == NULL) {
		sched_unpin();
		return (-EFAULT);
	}
	return (char *)sf_buf_kva(sf);
}

void *
kmap_atomic(vm_page_t page)
{
	caddr_t vaddr;

	sched_pin();
	if ((vaddr = kmap(page)) == NULL)
		sched_unpin();
	return (vaddr);
}
	
void
kunmap(vm_page_t page)
{
	struct sf_buf *sf;

	sf = pagetosf(page);
	sf_buf_free(sf);
}

void
kunmap_atomic(caddr_t vaddr)
{

	struct sf_buf *sf;

	sf = vtosf(vaddr);
	sf_buf_free(sf);
	sched_unpin();
}
#endif

void
page_cache_release(vm_page_t page)
{
	vm_page_lock(page);
	vm_page_unwire(page, PQ_INACTIVE);
	vm_page_unlock(page);
}

void *
iomap_atomic_prot_pfn(unsigned long pfn, vm_prot_t prot)
{
	sched_pin();
	return (void *)pmap_mapdev_attr(pfn << PAGE_SHIFT,
					PAGE_SIZE, prot);
}

struct io_mapping *
io_mapping_create_wc(vm_paddr_t base, unsigned long size)
{
	struct io_mapping *iomap;

	if ((iomap = kmalloc(sizeof(*iomap), GFP_KERNEL)) == NULL)
		return (NULL);

	/* resource allocation happens when we look up the address on FreeBSD */
	iomap->base = base;
	iomap->size = size;
	return (iomap);
}

void
iounmap_atomic(void *vaddr)
{
	pmap_unmapdev((vm_offset_t)vaddr, PAGE_SIZE);
	sched_unpin();
}

void *
io_mapping_map_wc(struct io_mapping *mapping, unsigned long offset)
{
	resource_size_t phys_addr;

	BUG_ON(offset >= mapping->size);
	phys_addr = mapping->base + offset;

	return ioremap_wc(phys_addr, PAGE_SIZE);
}


void *
io_mapping_map_atomic_wc(struct io_mapping *mapping,
			 unsigned long offset)
{
	vm_paddr_t phys_addr;
	unsigned long pfn;

	BUG_ON(offset >= mapping->size);
	phys_addr = mapping->base + offset;
	pfn = (unsigned long) (phys_addr >> PAGE_SHIFT);
	mapping->prot = PAT_WRITE_COMBINING;
	return iomap_atomic_prot_pfn(pfn, mapping->prot);
}

void
io_mapping_free(struct io_mapping *mapping)
{
	/* assuming the resource is released elsewhere */
	kfree(mapping);
}

int
set_memory_uc(unsigned long addr, int numpages)
{

	return (pmap_change_attr(addr, numpages, VM_MEMATTR_UNCACHEABLE));
}

int
set_pages_uc(vm_page_t page, int numpages)
{
	unsigned long addr = (unsigned long)page_address(page);

	return set_memory_uc(addr, numpages);
}

int
set_memory_wc(unsigned long addr, int numpages)
{

	return (pmap_change_attr(addr, numpages, PAT_WRITE_COMBINING));
}

int
set_pages_wc(vm_page_t page, int numpages)
{
	unsigned long addr = (unsigned long)VM_PAGE_TO_PHYS(page);

	return set_memory_wc(addr, numpages);
}
int
set_memory_wb(unsigned long addr, int numpages)
{

	return (pmap_change_attr(addr, numpages, PAT_WRITE_BACK));
}

int
set_pages_wb(vm_page_t page, int numpages)
{
	unsigned long addr = (unsigned long)VM_PAGE_TO_PHYS(page);

	return set_memory_wb(addr, numpages);
}


/* look at actual flags e.g. GFP_KERNEL | GFP_DMA32 | __GFP_ZERO */
vm_page_t
alloc_page(gfp_t flags)
{
	vm_page_t page;
	int tries;
	int req;

	req = VM_ALLOC_ZERO | VM_ALLOC_NOOBJ;
	tries = 0;
retry:
	page = vm_page_alloc_contig(NULL, 0, req, 1, 0, 0xffffffff,
	    PAGE_SIZE, 0, VM_MEMATTR_UNCACHEABLE);
	if (page == NULL) {
		if (tries < 1) {
			if (!vm_page_reclaim_contig(req, 1, 0, 0xffffffff,
			    PAGE_SIZE, 0))
				VM_WAIT;
			tries++;
			goto retry;
		}
		return (NULL);
	}
	if ((flags & __GFP_ZERO) && ((page->flags & PG_ZERO) == 0))
		pmap_zero_page(page);
	return (page);
}

vm_paddr_t
page_to_phys(vm_page_t page)
{
	return (VM_PAGE_TO_PHYS(page));
}

void *
acpi_os_ioremap(vm_paddr_t pa, vm_size_t size)
{
	return ((void *)pmap_mapbios(pa, size));
}

void
unmap_mapping_range(void *obj, loff_t const holebegin, loff_t const holelen, int even_cows)
{
	vm_object_t devobj;
	vm_page_t page;
	int i, page_count;

#ifdef LINUX_VERBOSE_DEBUG
	BACKTRACE();
	printf("unmap_mapping_range: obj: %p holebegin %zu, holelen: %zu, even_cows: %d\n",
	       obj, holebegin, holelen, even_cows);
#endif
	devobj = cdev_pager_lookup(obj);
	if (devobj != NULL) {
		page_count = OFF_TO_IDX(holelen);

		VM_OBJECT_WLOCK(devobj);
retry:
		for (i = 0; i < page_count; i++) {
			page = vm_page_lookup(devobj, i);
			if (page == NULL)
				continue;
			if (vm_page_sleep_if_busy(page, "linuxkpi"))
				goto retry;
			cdev_pager_free_page(devobj, page);
		}
		VM_OBJECT_WUNLOCK(devobj);
		vm_object_deallocate(devobj);
	}
}

#if defined(__i386__) || defined(__amd64__)

int
set_pages_array_wb(struct page **pages, int addrinarray)
{
	int i;

	for (i = 0; i < addrinarray; i++)
		set_pages_wb(pages[i], 1);
	return (0);
}

int
set_pages_array_wc(struct page **pages, int addrinarray)
{
	int i;

	for (i = 0; i < addrinarray; i++)
		set_pages_wc(pages[i], 1);
	return (0);
}

int
set_pages_array_uc(struct page **pages, int addrinarray)
{
	int i;

	for (i = 0; i < addrinarray; i++)
		set_pages_uc(pages[i], 1);
	return (0);
}
#endif


