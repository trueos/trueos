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

	if (pos < 0)
		return (-EINVAL);
	if (pos >= available || !count)
		return 0;
	if (count > available - pos)
		count = available - pos;
	ret = copy_to_user(to, from + pos, count);
	if (ret == count)
		return (-EFAULT);
	count -= ret;
	*ppos = pos + count;
	return (count);
}

ssize_t
simple_write_to_buffer(void *to, size_t available, loff_t *ppos,
		       const void __user *from, size_t count)
{
	loff_t pos = *ppos;
	size_t res;

	if (pos < 0)
		return (-EINVAL);
	if (pos >= available || !count)
		return 0;
	if (count > available - pos)
		count = available - pos;
	res = copy_from_user(to + pos, from, count);
	if (res == count)
		return (-EFAULT);
	count -= res;
	*ppos = pos + count;
	return (count);	
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
simple_attr_read(struct file *file, char __user *buf,
			 size_t len, loff_t *ppos)
{
	struct simple_attr *attr;
	size_t size;
	ssize_t ret;

	attr = file->private_data;

	if (!attr->get)
		return -EACCES;

	ret = mutex_lock_interruptible(&attr->mutex);
	if (ret)
		return ret;

	if (*ppos)
		size = strlen(attr->get_buf);
	else {
		u64 val;
		ret = attr->get(attr->data, &val);
		if (ret)
			goto out;

		size = scnprintf(attr->get_buf, sizeof(attr->get_buf),
				 attr->fmt, (unsigned long long)val);
	}

	ret = simple_read_from_buffer(buf, len, ppos, attr->get_buf, size);
out:
	mutex_unlock(&attr->mutex);
	return ret;
}


ssize_t
simple_attr_write(struct file *file, const char __user *buf,
		  size_t len, loff_t *ppos)
{
	struct simple_attr *attr;
	u64 val;
	size_t size;
	ssize_t ret;

	attr = file->private_data;
	if (!attr->set)
		return -EACCES;

	ret = mutex_lock_interruptible(&attr->mutex);
	if (ret)
		return ret;

	ret = -EFAULT;
	size = min(sizeof(attr->set_buf) - 1, len);
	if (copy_from_user(attr->set_buf, buf, size))
		goto out;

	attr->set_buf[size] = '\0';
	val = simple_strtoll(attr->set_buf, NULL, 0);
	ret = attr->set(attr->data, val);
	if (ret == 0)
		ret = len; /* on success, claim we got the whole input */
out:
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
	page = vm_page_grab(object, pindex, VM_ALLOC_NORMAL);
	if (page->valid != VM_PAGE_BITS_ALL) {
		if (vm_pager_has_page(object, pindex, NULL, NULL)) {
			rv = vm_pager_get_pages(object, &page, 1, NULL, NULL, VM_PROT_READ|VM_PROT_WRITE);
			if (rv != VM_PAGER_OK) {
				vm_page_lock(page);
				vm_page_free(page);
				vm_page_unlock(page);
				return (NULL);
			}
			MPASS(page->valid == VM_PAGE_BITS_ALL);
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

struct linux_file *
shmem_file_setup(char *name, int size, int flags)
{
	struct linux_file *filp;
	struct vnode *vp;

	if ((filp = malloc(sizeof(*filp), M_LKFS, M_NOWAIT|M_ZERO)) == NULL)
		return (NULL);

	if (getnewvnode("LINUX", NULL, &dead_vnodeops, &vp))
		goto err_1;

	filp->f_dentry = &filp->f_dentry_store;
	filp->f_vnode = vp;
	file_inode(filp)->i_mapping = vm_pager_allocate(OBJT_DEFAULT, NULL, size,
	    VM_PROT_READ | VM_PROT_WRITE, 0, curthread->td_ucred);

	if (file_inode(filp)->i_mapping == NULL)
		goto err_2;

	return (filp);
err_2:
	_vdrop(vp, 0);
err_1:
	free(filp, M_LKFS);
	return (NULL);
}


struct inode *
alloc_anon_inode(struct super_block *s)
{
	struct vnode *vp;

	if (getnewvnode("LINUX", NULL, &dead_vnodeops, &vp))
		return (NULL);
	return (vp);
}

unsigned long
invalidate_mapping_pages(vm_object_t obj, pgoff_t start, pgoff_t end)
{
	int start_count, end_count;

	VM_OBJECT_WLOCK(obj);
	start_count = obj->resident_page_count;
	vm_object_page_remove(obj, start, end, false);
	end_count = obj->resident_page_count;
	VM_OBJECT_WUNLOCK(obj);

	return (start_count - end_count);
}

void
shmem_truncate_range(struct vnode *vp, loff_t lstart, loff_t lend)
{
	vm_object_t vm_obj;
	vm_pindex_t start = (lstart + PAGE_SIZE - 1) >> PAGE_SHIFT;
	vm_pindex_t end = (lend + 1) >> PAGE_SHIFT;

	vm_obj = vp->i_mapping;
	(void)invalidate_mapping_pages(vm_obj, start, end);
}

static int
__get_user_pages_internal(vm_map_t map, unsigned long start, int nr_pages, int write,
			  struct page **pages)
{
	int count, len;
	vm_prot_t prot;

	prot = VM_PROT_READ;
	if (write)
		prot |= VM_PROT_WRITE;
	len = nr_pages << PAGE_SHIFT;
	count = vm_fault_quick_hold_pages(map, start, len, prot, pages, nr_pages);
	return (count == -1 ? -EFAULT : count);
}

int
__get_user_pages_fast(unsigned long start, int nr_pages, int write,
			  struct page **pages)

{
	vm_map_t map;
	vm_page_t *mp;
	vm_offset_t va, end;
	int count;
	vm_prot_t prot;

	if (nr_pages == 0)
		return (0);

	MPASS(pages != NULL);
	va = start;
	map = &curthread->td_proc->p_vmspace->vm_map;
	end = start + (nr_pages << PAGE_SHIFT);
	if (start < vm_map_min(map) ||  end > vm_map_max(map))
		return (-EINVAL);
	prot = VM_PROT_READ;
	if (write)
		prot |= VM_PROT_WRITE;
	for (count= 0, mp = pages, va = start; va < end; mp++, va += PAGE_SIZE, count++) {
		*mp = pmap_extract_and_hold(map->pmap, va, prot);
		if (*mp == NULL)
			break;
		else if ((prot & VM_PROT_WRITE) != 0 &&
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
		      unsigned long start, unsigned long nr_pages,
		      int write, int force, struct page **pages,
		      struct vm_area_struct **vmas)
{
	vm_map_t map;

	map = &tsk->task_thread->td_proc->p_vmspace->vm_map;
	return (__get_user_pages_internal(map, start, nr_pages, write, pages));
}

long
get_user_pages(unsigned long start, unsigned long nr_pages,
		int write, int force, struct page **pages,
		    struct vm_area_struct **vmas)
{
	vm_map_t map;

	map = &curthread->td_proc->p_vmspace->vm_map;
	return (__get_user_pages_internal(map, start, nr_pages, write, pages));
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


