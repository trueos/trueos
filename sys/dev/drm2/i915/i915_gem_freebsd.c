/*
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include <dev/drm2/i915/i915_drv.h>
#include <dev/drm2/i915/intel_drv.h>

#include <sys/resourcevar.h>
#include <sys/sched.h>

#include <vm/vm.h>
#include <vm/vm_pageout.h>

#include <machine/md_var.h>


MALLOC_DEFINE(DRM_I915_GEM, "i915", "i915 DRM GEM");

#define GEM_PARANOID_CHECK_GTT 0

long i915_gem_wired_pages_cnt;
int i915_intr_pf;

vm_page_t
i915_gem_wire_page(vm_object_t object, vm_pindex_t pindex, bool *fresh)
{
	vm_page_t page;
	int rv;

	VM_OBJECT_ASSERT_WLOCKED(object);
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
			if (fresh != NULL)
				*fresh = true;
		} else {
			pmap_zero_page(page);
			page->valid = VM_PAGE_BITS_ALL;
			page->dirty = 0;
			if (fresh != NULL)
				*fresh = false;
		}
	} else if (fresh != NULL) {
		*fresh = false;
	}
	vm_page_lock(page);
	vm_page_wire(page);
	vm_page_unlock(page);
	vm_page_xunbusy(page);
	atomic_add_long(&i915_gem_wired_pages_cnt, 1);
	return (page);
}

#if GEM_PARANOID_CHECK_GTT
static void
i915_gem_assert_pages_not_mapped(struct drm_device *dev, vm_page_t *ma,
    int page_count)
{
	struct drm_i915_private *dev_priv;
	vm_paddr_t pa;
	unsigned long start, end;
	u_int i;
	int j;

	dev_priv = dev->dev_private;
	start = OFF_TO_IDX(dev_priv->mm.gtt_start);
	end = OFF_TO_IDX(dev_priv->mm.gtt_end);
	for (i = start; i < end; i++) {
		pa = intel_gtt_read_pte_paddr(i);
		for (j = 0; j < page_count; j++) {
			if (pa == VM_PAGE_TO_PHYS(ma[j])) {
				panic("Page %p in GTT pte index %d pte %x",
				    ma[i], i, intel_gtt_read_pte(i));
			}
		}
	}
}
#endif


/**
 * i915_gem_fault - fault a page into the GTT
 * vma: VMA in question
 * vmf: fault info
 *
 * The fault handler is set up by drm_gem_mmap() when a object is GTT mapped
 * from userspace.  The fault handler takes care of binding the object to
 * the GTT (if needed), allocating and programming a fence register (again,
 * only if needed based on whether the old reg is still valid or the object
 * is tiled) and inserting a new PTE into the faulting process.
 *
 * Note that the faulting process may involve evicting existing objects
 * from the GTT and/or fence registers to make room.  So performance may
 * suffer if the GTT working set is large or there are few fence registers
 * left.
 */

static int
i915_gem_pager_fault(vm_object_t vm_obj, vm_ooffset_t offset, int prot,
    vm_page_t *mres)
{
	struct drm_gem_object *gem_obj = vm_obj->handle;
	struct drm_i915_gem_object *obj = to_intel_bo(gem_obj);
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_ggtt_view view = i915_ggtt_view_normal;
	vm_page_t page, oldpage;
	int ret = 0;
#ifdef FREEBSD_WIP
	bool write = (prot & VM_PROT_WRITE) != 0;
#else
	bool write = true;
#endif /* FREEBSD_WIP */
	bool pinned;

	vm_object_pip_add(vm_obj, 1);

	/*
	 * Remove the placeholder page inserted by vm_fault() from the
	 * object before dropping the object lock. If
	 * i915_gem_release_mmap() is active in parallel on this gem
	 * object, then it owns the drm device sx and might find the
	 * placeholder already. Then, since the page is busy,
	 * i915_gem_release_mmap() sleeps waiting for the busy state
	 * of the page cleared. We will be unable to acquire drm
	 * device lock until i915_gem_release_mmap() is able to make a
	 * progress.
	 */
	if (*mres != NULL) {
		oldpage = *mres;
		vm_page_lock(oldpage);
		vm_page_remove(oldpage);
		vm_page_unlock(oldpage);
		*mres = NULL;
	} else
		oldpage = NULL;
	VM_OBJECT_WUNLOCK(vm_obj);
retry:
	ret = 0;
	pinned = 0;
	page = NULL;

	if (i915_intr_pf) {
		ret = i915_mutex_lock_interruptible(dev);
		if (ret != 0)
			goto out;
	} else
		mutex_lock(&dev->struct_mutex);

	/*
	 * Since the object lock was dropped, other thread might have
	 * faulted on the same GTT address and instantiated the
	 * mapping for the page.  Recheck.
	 */
	VM_OBJECT_WLOCK(vm_obj);
	page = vm_page_lookup(vm_obj, OFF_TO_IDX(offset));
	if (page != NULL) {
		if (vm_page_busied(page)) {
			mutex_unlock(&dev->struct_mutex);
			vm_page_lock(page);
			VM_OBJECT_WUNLOCK(vm_obj);
			vm_page_busy_sleep(page, "915pee");
			goto retry;
		}
		goto have_page;
	} else
		VM_OBJECT_WUNLOCK(vm_obj);

	/* Now bind it into the GTT if needed */
	ret = i915_gem_object_pin(obj, 0, true, false);
	if (ret)
		goto unlock;
	pinned = 1;

	ret = i915_gem_object_set_to_gtt_domain(obj, write);
	if (ret)
		goto unpin;

	ret = i915_gem_object_get_fence(obj);
	if (ret)
		goto unpin;

	obj->fault_mappable = true;

	VM_OBJECT_WLOCK(vm_obj);

	page = PHYS_TO_VM_PAGE(dev_priv->gtt.mappable_base + i915_gem_obj_ggtt_offset_view(obj, &view) + offset);
	KASSERT((page->flags & PG_FICTITIOUS) != 0,
	    ("physical address %#jx not fictitious",
	     (uintmax_t)(dev_priv->gtt.mappable_base + i915_gem_obj_ggtt_offset_view(obj, &view) + offset)));
	if (page == NULL) {
		VM_OBJECT_WUNLOCK(vm_obj);
		ret = -EFAULT;
		goto unpin;
	}
	KASSERT((page->flags & PG_FICTITIOUS) != 0,
	    ("not fictitious %p", page));
	KASSERT(page->wire_count == 1, ("wire_count not 1 %p", page));

	if (vm_page_busied(page)) {
		i915_gem_object_unpin_pages(obj);
		mutex_unlock(&dev->struct_mutex);
		vm_page_lock(page);
		VM_OBJECT_WUNLOCK(vm_obj);
		vm_page_busy_sleep(page, "915pbs");
		goto retry;
	}
	if (vm_page_insert(page, vm_obj, OFF_TO_IDX(offset))) {
		i915_gem_object_unpin_pages(obj);
		mutex_unlock(&dev->struct_mutex);
		VM_OBJECT_WUNLOCK(vm_obj);
		VM_WAIT;
		goto retry;
	}
	page->valid = VM_PAGE_BITS_ALL;
have_page:
	*mres = page;
	vm_page_xbusy(page);

	CTR4(KTR_DRM, "fault %p %jx %x phys %x", gem_obj, offset, prot,
	    page->phys_addr);
	if (pinned) {
		/*
		 * We may have not pinned the object if the page was
		 * found by the call to vm_page_lookup()
		 */
		i915_gem_object_unpin_pages(obj);
	}
	mutex_unlock(&dev->struct_mutex);
	if (oldpage != NULL) {
		vm_page_lock(oldpage);
		vm_page_free(oldpage);
		vm_page_unlock(oldpage);
	}
	vm_object_pip_wakeup(vm_obj);
	return (VM_PAGER_OK);

unpin:
	i915_gem_object_unpin_pages(obj);
unlock:
	mutex_unlock(&dev->struct_mutex);
out:
	KASSERT(ret != 0, ("i915_gem_pager_fault: wrong return"));
	CTR4(KTR_DRM, "fault_fail %p %jx %x err %d", gem_obj, offset, prot,
	    -ret);
	if (ret == -ERESTARTSYS) {
		/*
		 * NOTE Linux<->FreeBSD: Convert Linux' -ERESTARTSYS to
		 * the more common -EINTR, so the page fault is retried.
		 */
		ret = -EINTR;
	}
	if (ret == -EAGAIN || ret == -EIO || ret == -EINTR) {
		kern_yield(PRI_USER);
		goto retry;
	}
	VM_OBJECT_WLOCK(vm_obj);
	vm_object_pip_wakeup(vm_obj);
	return (VM_PAGER_ERROR);
}

static int
i915_gem_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color)
{

	/*
	 * NOTE Linux<->FreeBSD: drm_gem_mmap_single() takes care of
	 * calling drm_gem_object_reference(). That's why we don't
	 * do this here. i915_gem_pager_dtor(), below, will call
	 * drm_gem_object_unreference().
	 *
	 * On Linux, drm_gem_vm_open() references the object because
	 * it's called the mapping is copied. drm_gem_vm_open() is not
	 * called when the mapping is created. So the possible sequences
	 * are:
	 *     1. drm_gem_mmap():     ref++
	 *     2. drm_gem_vm_close(): ref--
	 *
	 *     1. drm_gem_mmap():     ref++
	 *     2. drm_gem_vm_open():  ref++ (for the copied vma)
	 *     3. drm_gem_vm_close(): ref-- (for the copied vma)
	 *     4. drm_gem_vm_close(): ref-- (for the initial vma)
	 *
	 * On FreeBSD, i915_gem_pager_ctor() is called once during the
	 * creation of the mapping. No callback is called when the
	 * mapping is shared during a fork(). i915_gem_pager_dtor() is
	 * called when the last reference to the mapping is dropped. So
	 * the only sequence is:
	 *     1. drm_gem_mmap_single(): ref++
	 *     2. i915_gem_pager_ctor(): <noop>
	 *     3. i915_gem_pager_dtor(): ref--
	 */

	*color = 0; /* XXXKIB */
	return (0);
}

static void
i915_gem_pager_dtor(void *handle)
{
	struct drm_gem_object *obj = handle;
	struct drm_device *dev = obj->dev;

	mutex_lock(&dev->struct_mutex);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
}

struct cdev_pager_ops i915_gem_pager_ops = {
	.cdev_pg_fault	= i915_gem_pager_fault,
	.cdev_pg_ctor	= i915_gem_pager_ctor,
	.cdev_pg_dtor	= i915_gem_pager_dtor
};

MODULE_DEPEND(i915kms, drmn, 1, 1, 1);
MODULE_DEPEND(i915kms, agp, 1, 1, 1);
MODULE_DEPEND(i915kms, linuxkpi, 1, 1, 1);
