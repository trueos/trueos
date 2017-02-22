/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2016 Matt Macy (mmacy@nextbsd.org)
 * Copyright (c) 2017 Mellanox Technologies, Ltd.
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

#include <machine/bus.h>

#include <linux/gfp.h>
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
	return (m->md.pat_mode != attr);
}
#endif

int
vm_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn, pgprot_t pgprot)
{
	vm_object_t vm_obj;
	vm_page_t page;
	pmap_t pmap = vma->vm_cached_map->pmap;
	vm_memattr_t attr = pgprot2cachemode(pgprot);
	vm_offset_t off;

	vm_obj = vma->vm_obj;
	page = PHYS_TO_VM_PAGE((pfn << PAGE_SHIFT));
	off = OFF_TO_IDX(addr - vma->vm_start);

	MPASS(off <= OFF_TO_IDX(vma->vm_end));
#if defined(__i386__) || defined(__amd64__)
	if (needs_set_memattr(page, attr))
		pmap_page_set_memattr(page, attr);
#endif
	if ((page->flags & PG_FICTITIOUS) && ((page->oflags & VPO_UNMANAGED) == 0))
		page->oflags |= VPO_UNMANAGED;
	page->valid = VM_PAGE_BITS_ALL;
	pmap_enter(pmap, addr, page, pgprot & VM_PROT_ALL, (pgprot & VM_PROT_ALL) | PMAP_ENTER_NOSLEEP, 0);
	(*vma->vm_pfn_pcount)++;
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
__linux_clflushopt(u_long addr)
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

void
iounmap_atomic(void *vaddr)
{
	pmap_unmapdev((vm_offset_t)vaddr, PAGE_SIZE);
	sched_unpin();
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

int
arch_io_reserve_memtype_wc(resource_size_t start, resource_size_t size)
{
	return (set_memory_wc(start, size >> PAGE_SHIFT));
}

void
arch_io_free_memtype_wc(resource_size_t start, resource_size_t size)
{
	set_memory_wb(start, size >> PAGE_SHIFT);
}

vm_page_t
linux_alloc_pages(gfp_t flags, unsigned int order)
{
	unsigned long npages = 1UL << order;
	int req = (flags & M_ZERO) ? (VM_ALLOC_ZERO | VM_ALLOC_NOOBJ |
	    VM_ALLOC_NORMAL) : (VM_ALLOC_NOOBJ | VM_ALLOC_NORMAL);
	vm_page_t page;

	if (order == 0 && (flags & GFP_DMA32) == 0) {
		page = vm_page_alloc(NULL, 0, req);
		if (page == NULL)
			return (NULL);
	} else {
		vm_paddr_t pmax = (flags & GFP_DMA32) ?
		    BUS_SPACE_MAXADDR_32BIT : BUS_SPACE_MAXADDR;
retry:
		page = vm_page_alloc_contig(NULL, 0, req,
		    npages, 0, pmax, PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);

		if (page == NULL) {
			if (flags & M_WAITOK) {
				if (!vm_page_reclaim_contig(req,
				    npages, 0, pmax, PAGE_SIZE, 0)) {
					VM_WAIT;
				}
				flags &= ~M_WAITOK;
				goto retry;
			}
			return (NULL);
		}
	}
	if (flags & M_ZERO) {
		unsigned long x;

		for (x = 0; x != npages; x++) {
			vm_page_t pgo = page + x;

			if ((pgo->flags & PG_ZERO) == 0)
				pmap_zero_page(pgo);
		}
	}
	return (page);
}

void
linux_free_pages(vm_page_t page, unsigned int order)
{
	unsigned long npages = 1UL << order;
	unsigned long x;

	for (x = 0; x != npages; x++) {
		vm_page_t pgo = page + x;

		vm_page_lock(pgo);
		vm_page_free(pgo);
		vm_page_unlock(pgo);
	}
}

vm_offset_t
linux_alloc_kmem(gfp_t flags, unsigned int order)
{
	size_t size = ((size_t)PAGE_SIZE) << order;
	vm_offset_t addr;

	if ((flags & GFP_DMA32) == 0) {
		addr = kmem_malloc(kmem_arena, size, flags & GFP_NATIVE_MASK);
	} else {
		addr = kmem_alloc_contig(kmem_arena, size,
		    flags & GFP_NATIVE_MASK, 0, BUS_SPACE_MAXADDR_32BIT, size, 0,
		    VM_MEMATTR_DEFAULT);
	}
	return (addr);
}

void
linux_free_kmem(vm_offset_t addr, unsigned int order)
{
	size_t size = ((size_t)PAGE_SIZE) << order;

	kmem_free(kmem_arena, addr, size);
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


