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
#include <sys/rwlock.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>

#include <linux/page.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/fs.h>


#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/vm_extern.h>


struct page *
shmem_read_mapping_page_gfp(struct address_space *as, int pindex, gfp_t gfp)
{
	vm_page_t page;
	vm_object_t object;
	int rv;

	object = as->vm_obj;
	VM_OBJECT_WLOCK(object);
	page = vm_page_grab(object, pindex, VM_ALLOC_NORMAL);
	if (page->valid != VM_PAGE_BITS_ALL) {
		if (vm_pager_has_page(object, pindex, NULL, NULL)) {
			rv = vm_pager_get_pages(object, &page, 1, NULL, NULL);
			if (rv != VM_PAGER_OK) {
				vm_page_lock(page);
				vm_page_free(page);
				vm_page_unlock(page);
				return (NULL);
			}
		} else {
			pmap_zero_page(page);
			page->valid = VM_PAGE_BITS_ALL;
			page->dirty = 0;
		}
	}
	vm_page_lock(page);
	vm_page_wire(page);
	vm_page_unlock(page);
	vm_page_xunbusy(page);
	VM_OBJECT_WUNLOCK(object);
	return (page);
}
