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
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <asm/uaccess.h>


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
#include <vm/vm_map.h>



static MALLOC_DEFINE(M_LKFS, "lkfs", "lkpi fs");
uma_zone_t vnode_zone;

struct dentry *
mount_pseudo(struct file_system_type *fs_type, char *name,
	const struct super_operations *ops,
			    const struct dentry_operations *dops, unsigned long magic)
{
	UNIMPLEMENTED();
	return (NULL);
}
void
kill_anon_super(struct super_block *sb)
{
	UNIMPLEMENTED();
}


char *
simple_dname(struct dentry *dentry, char *buffer, int buflen)
{
	UNIMPLEMENTED();
	return (NULL);
}


int
simple_pin_fs(struct file_system_type *type, struct vfsmount **mount, int *count)
{
	struct vfsmount *mp;

	DODGY();
	if ((mp = malloc(sizeof(*mp), M_LKFS, M_WAITOK|M_ZERO)) == NULL)
		return (-ENOMEM);
	*mount = mp;
	return (0);
}

void
simple_release_fs(struct vfsmount **mount, int *count)
{
	free(*mount, M_LKFS);
	*mount = NULL;
}

int
simple_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	UNIMPLEMENTED();
	return (0);
}

long long
simple_strtoll(const char *cp, char **endp, unsigned int base)
{
	if (*cp == '-')
		return -strtouq(cp + 1, endp, base);

	return strtouq(cp, endp, base);
}

ssize_t
simple_read_from_buffer(void __user *to, size_t count,
			loff_t *ppos, const void *from, size_t available)
{
	loff_t pos = *ppos;
	size_t ret;

	if ((int64_t)pos < 0)
		return (-EINVAL);
	if (pos >= available || !count)
		return (0);
	if (count > available - pos)
		count = available - pos;
	ret = copyout(from, to + pos, count);
	if (ret != 0)
		return (-EFAULT);
	*ppos = pos + count;
	return (0);
}

ssize_t
simple_write_to_buffer(void *to, size_t available, loff_t *ppos,
		       const void __user *from, size_t count)
{
	loff_t pos = *ppos;
	size_t res;

	if ((int64_t)pos < 0)
		return (-EINVAL);
	if (pos >= available || !count)
		return (0);
	if (count > available - pos)
		count = available - pos;
	res = copyin(from, to + pos, count);
	if (res != 0)
		return (-EFAULT);
	*ppos = pos + count;
	return (0);
}

int
simple_attr_open(struct inode *inode, struct file *file,
		     int (*get)(void *, u64 *), int (*set)(void *, u64),
		     const char *fmt)
{
	struct simple_attr *attr;

	attr = kmalloc(sizeof(*attr), GFP_KERNEL);
	if (!attr)
		return -ENOMEM;

	attr->get = get;
	attr->set = set;
	attr->data = inode->i_private;
	attr->fmt = fmt;
	mutex_init(&attr->mutex);

	file->private_data = attr;

	return (nonseekable_open(inode, file));
}

int
simple_attr_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return (0);
}

ssize_t
simple_attr_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	struct sbuf *sb;
	struct simple_attr *attr;
	ssize_t ret;

	attr = file->private_data;

	if (!attr->get)
		return -EACCES;

	ret = mutex_lock_interruptible(&attr->mutex);
	if (ret)
		return ret;

	sb = attr->sb;
	if (*ppos == 0) {
		u64 val;
		ret = attr->get(attr->data, &val);
		if (ret)
			goto out;
		(void)sbuf_printf(sb, attr->fmt, (unsigned long long)val);
	}
out:
	mutex_unlock(&attr->mutex);
	return ret;
}

ssize_t
simple_attr_write(struct file *file, const char *buf, size_t len, loff_t *ppos)
{
	struct sbuf *sb;
	struct simple_attr *attr;
	u64 val;
	ssize_t ret;

	attr = file->private_data;
	if (!attr->set)
		return -EACCES;

	ret = mutex_lock_interruptible(&attr->mutex);
	if (ret)
		return ret;

	sb = attr->sb;
	(void)sbuf_finish(sb);
	val = simple_strtoll(sbuf_data(sb), NULL, 0);
	ret = attr->set(attr->data, val);
	mutex_unlock(&attr->mutex);
	return (ret);
}

loff_t
generic_file_llseek(struct file *file, loff_t offset, int whence)
{

	panic("%s not supported/implemented \n", __FUNCTION__);
	return (0);
}

loff_t
default_llseek(struct file *file, loff_t offset, int whence)
{

	panic("%s not supported/implemented \n", __FUNCTION__);
	return (0);
}

struct page *
shmem_read_mapping_page_gfp(struct address_space *as, int pindex, gfp_t gfp)
{
	vm_page_t page;
	vm_object_t object;
	int rv;

	object = as;
	VM_OBJECT_WLOCK(object);
	/* XXXMJ should handle ALLOC_NOWAIT? */
	page = vm_page_grab(object, pindex, VM_ALLOC_NORMAL | VM_ALLOC_NOBUSY |
	    VM_ALLOC_WIRED);
	if (page->valid != VM_PAGE_BITS_ALL) {
		vm_page_xbusy(page);
		if (vm_pager_has_page(object, pindex, NULL, NULL)) {
			rv = vm_pager_get_pages(object, &page, 1, NULL, NULL);
			if (rv != VM_PAGER_OK) {
				vm_page_lock(page);
				vm_page_unwire(page, PQ_NONE);
				vm_page_free(page);
				vm_page_unlock(page);
				VM_OBJECT_WUNLOCK(object);
				return (ERR_PTR(-EINVAL));
			}
			MPASS(page->valid == VM_PAGE_BITS_ALL);
		} else {
			pmap_zero_page(page);
			page->valid = VM_PAGE_BITS_ALL;
			page->dirty = 0;
		}
		vm_page_xunbusy(page);
	}
	vm_page_lock(page);
	vm_page_hold(page);
	vm_page_unlock(page);
	VM_OBJECT_WUNLOCK(object);
	return (page);
}

static struct vnode *
linux_get_new_vnode(void)
{
	struct vnode *vp;
	int error;

	error = getnewvnode("LINUX", NULL, &dead_vnodeops, &vp);
	if (error != 0)
		return (NULL);
	return (vp);
}

struct linux_file *
shmem_file_setup(char *name, loff_t size, unsigned long flags)
{
	struct linux_file *filp;
	struct vnode *vp;
	int error;

	filp = kzalloc(sizeof(*filp), GFP_KERNEL);
	if (filp == NULL) {
		error = -ENOMEM;
		goto err_0;
	}

	vp = linux_get_new_vnode();
	if (vp == NULL) {
		error = -EINVAL;
		goto err_1;
	}

	filp->f_dentry = &filp->f_dentry_store;
	filp->f_vnode = vp;
	filp->f_mapping = file_inode(filp)->i_mapping =
	    vm_pager_allocate(OBJT_DEFAULT, NULL, size,
	    VM_PROT_READ | VM_PROT_WRITE, 0, curthread->td_ucred);

	if (file_inode(filp)->i_mapping == NULL) {
		error = -ENOMEM;
		goto err_2;
	}

	return (filp);
err_2:
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	vgone(vp);
	vput(vp);
err_1:
	kfree(filp);
err_0:
	return (ERR_PTR(error));
}

struct inode *
alloc_anon_inode(struct super_block *s)
{
	struct vnode *vp;

	vp = linux_get_new_vnode();
	if (vp == NULL)
		return (ERR_PTR(-EINVAL));
	return (vp);
}

static vm_ooffset_t
_invalidate_mapping_pages(vm_object_t obj, vm_pindex_t start, vm_pindex_t end,
    int flags)
{
	int start_count, end_count;

	VM_OBJECT_WLOCK(obj);
	start_count = obj->resident_page_count;
	vm_object_page_remove(obj, start, end, flags);
	end_count = obj->resident_page_count;
	VM_OBJECT_WUNLOCK(obj);
	return (start_count - end_count);
}

unsigned long
invalidate_mapping_pages(vm_object_t obj, pgoff_t start, pgoff_t end)
{

	return (_invalidate_mapping_pages(obj, start, end, OBJPR_CLEANONLY));
}

void
shmem_truncate_range(struct vnode *vp, loff_t lstart, loff_t lend)
{
	vm_object_t vm_obj = vp->i_mapping;
	vm_pindex_t start = OFF_TO_IDX(lstart + PAGE_SIZE - 1);
	vm_pindex_t end = OFF_TO_IDX(lend + 1);

	(void)_invalidate_mapping_pages(vm_obj, start, end, 0);
}

static int
__get_user_pages_internal(vm_map_t map, unsigned long start, int nr_pages, int write,
			  struct page **pages)
{
	vm_prot_t prot;
	size_t len;
	int count;
	int i;

	prot = write ? (VM_PROT_READ | VM_PROT_WRITE) : VM_PROT_READ;
	len = ((size_t)nr_pages) << PAGE_SHIFT;
	count = vm_fault_quick_hold_pages(map, start, len, prot, pages, nr_pages);
	if (count == -1)
		return (-EFAULT);

	for (i = 0; i != nr_pages; i++) {
		struct page *pg = pages[i];

		vm_page_lock(pg);
		vm_page_wire(pg);
		vm_page_unlock(pg);
	}
	return (nr_pages);
}

int
__get_user_pages_fast(unsigned long start, int nr_pages, int write,
    struct page **pages)
{
	vm_map_t map;
	vm_page_t *mp;
	vm_offset_t va;
	vm_offset_t end;
	vm_prot_t prot;
	int count;

	if (nr_pages == 0 || in_interrupt())
		return (0);

	MPASS(pages != NULL);
	va = start;
	map = &curthread->td_proc->p_vmspace->vm_map;
	end = start + (((size_t)nr_pages) << PAGE_SHIFT);
	if (start < vm_map_min(map) ||  end > vm_map_max(map))
		return (-EINVAL);
	prot = write ? (VM_PROT_READ | VM_PROT_WRITE) : VM_PROT_READ;
	for (count = 0, mp = pages, va = start; va < end;
	    mp++, va += PAGE_SIZE, count++) {
		*mp = pmap_extract_and_hold(map->pmap, va, prot);
		if (*mp == NULL)
			break;

		vm_page_lock(*mp);
		vm_page_wire(*mp);
		vm_page_unlock(*mp);

		if ((prot & VM_PROT_WRITE) != 0 &&
		    (*mp)->dirty != VM_PAGE_BITS_ALL) {
			/*
			 * Explicitly dirty the physical page.  Otherwise, the
			 * caller's changes may go unnoticed because they are
			 * performed through an unmanaged mapping or by a DMA
			 * operation.
			 *
			 * The object lock is not held here.
			 * See vm_page_clear_dirty_mask().
			 */
			vm_page_dirty(*mp);
		}
	}
	return (count);
}

long
get_user_pages_remote(struct task_struct *tsk, struct mm_struct *mm,
    unsigned long start, unsigned long nr_pages, int gup_flags,
    struct page **pages, struct vm_area_struct **vmas)
{
	vm_map_t map;

	map = &((struct vmspace *)mm->vmspace)->vm_map;
	return (__get_user_pages_internal(map, start, nr_pages,
	    !!(gup_flags & FOLL_WRITE), pages));
}

long
get_user_pages(unsigned long start, unsigned long nr_pages, int gup_flags,
    struct page **pages, struct vm_area_struct **vmas)
{
	vm_map_t map;

	map = &curthread->td_proc->p_vmspace->vm_map;
	return (__get_user_pages_internal(map, start, nr_pages,
	    !!(gup_flags & FOLL_WRITE), pages));
}

void
linux_file_free(struct linux_file *filp)
{

	if (filp->_file == NULL) {
		struct vnode *vp = filp->f_vnode;
		if (vp == NULL)
			goto done;
		if (vp->i_mapping != NULL) {
			vm_object_deallocate(vp->i_mapping);
			vp->i_mapping = NULL;
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		vgone(vp);
		vput(vp);
	} else {
		_fdrop(filp->_file, curthread);
	}
done:
	kfree(filp);
}

#include <sys/mount.h>
#include <fs/pseudofs/pseudofs.h>

static int
linpseudofs_init(PFS_INIT_ARGS)
{
	return (0);
}

static int
linpseudofs_uninit(PFS_INIT_ARGS)
{
	return (0);
}

PSEUDOFS(linpseudofs, 1, PR_ALLOW_MOUNT);
