/*-
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
#include <sys/sched.h>
#include <sys/sf_buf.h>

#include <linux/page.h>

struct sf_buf *
vtosf(caddr_t vaddr)
{
	panic("IMPLEMENT ME!!!");
	return (NULL);
}

caddr_t
kmap(vm_page_t page)
{
	sf = sf_buf_alloc(page, SFB_NOWAIT | SFB_CPUPRIVATE);
	if (sf == NULL) {
		sched_unpin();
		return (-EFAULT);
	}
	return (char *)sf_buf_kva(sf);
}

caddr_t
kmap_atomic(vm_page_t page)
{
	caddr_t vaddr;

	sched_pin();
	if ((vaddr = kmap(page)) == NULL)
		sched_unpin();
	return (vaddr);
}
	
void
kunmap(caddr_t vaddr)
{
	struct sf_buf *sf;

	sf = vtosf(vaddr);
	sf_buf_free(sf);
	sched_unpin();
}

void
kunmap_atomic(caddr_t vaddr)
{
	kunmap(vaddr);
	sched_unpin();
}

void
mark_page_accessed(vm_page_t page)
{
	vm_page_reference(page);
}

void
set_page_dirty(vm_page_t page)
{
	vm_page_dirty(page);
}

void
page_cache_release(vm_page_t page)
{
	vm_page_lock(page);
	vm_page_unwire(page, PQ_INACTIVE);
	vm_page_unlock(page);
}

void *
iomap_atomic_prot_pfn(unsigned long pfn, vm_prot_t prot);
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
