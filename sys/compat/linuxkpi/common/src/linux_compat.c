/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
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
#include <sys/proc.h>
#include <sys/sglist.h>
#include <sys/sleepqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/stdarg.h>

#if defined(__i386__) || defined(__amd64__)
#include <machine/md_var.h>
#endif

#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/sysfs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/netdevice.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/rcupdate.h>
#include <linux/interrupt.h>
#include <linux/async.h>
#include <linux/compat.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/compat.h>
#include <linux/poll.h>

#include <vm/vm_pager.h>
#include <vm/vm_pageout.h>
#include <vm/vm_map.h>
#include "linux_trace.h"

extern u_int cpu_clflush_line_size;
extern u_int cpu_id;
pteval_t __supported_pte_mask __read_mostly = ~0;


struct workqueue_struct *system_long_wq;
struct workqueue_struct *system_wq;
struct workqueue_struct *system_unbound_wq;
struct workqueue_struct *system_power_efficient_wq;

SYSCTL_NODE(_compat, OID_AUTO, linuxkpi, CTLFLAG_RW, 0, "LinuxKPI parameters");
int linux_db_trace;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, db_trace, CTLFLAG_RWTUN, &linux_db_trace, 0, "enable backtrace instrumentation");
int linux_skip_prefault;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, dev_fault_skip_prefault, CTLFLAG_RWTUN, &linux_skip_prefault, 0, "disable faultahead");
static int cdev_nopfn_count;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, cdev_nopfn_count, CTLFLAG_RW, &cdev_nopfn_count, 0, "cdev nopfn");
static int cdev_pfn_found_count;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, cdev_pfn_found_count, CTLFLAG_RW, &cdev_pfn_found_count, 0, "cdev found pfn");

MALLOC_DEFINE(M_KMALLOC, "linux", "Linux kmalloc compat");
MALLOC_DEFINE(M_LCINT, "linuxint", "Linux compat internal");


#undef file
#undef cdev


struct cpuinfo_x86 boot_cpu_data; 

struct kobject linux_class_root;
struct device linux_root_device;
struct class linux_class_misc;
struct list_head pci_drivers;
struct list_head pci_devices;
struct net init_net;
spinlock_t pci_lock;
struct sx linux_global_lock;

unsigned long linux_timer_hz_mask;
struct list_head cdev_list;
struct ida *hwmon_idap;
DEFINE_IDA(hwmon_ida);

/*
 * XXX need to define irq_idr 
 */

struct rendezvous_state {
	struct mtx rs_mtx;
	void *rs_data;
	smp_call_func_t *rs_func;
	int rs_count;
	bool rs_free;
};

static void
rendezvous_wait(void *arg)
{
	struct rendezvous_state *rs = arg;

	mtx_lock_spin(&rs->rs_mtx);
	rs->rs_count--;
	if (rs->rs_count == 0)
		wakeup(rs);
	mtx_unlock_spin(&rs->rs_mtx);
}

static void
rendezvous_callback(void *arg)
{
	struct rendezvous_state *rsp = arg;
	int needfree;

	rsp->rs_func(rsp->rs_data);
	if (rsp->rs_free) {
		mtx_lock_spin(&rsp->rs_mtx);
		rsp->rs_count--;
		needfree = (rsp->rs_count == 0);
		mtx_unlock_spin(&rsp->rs_mtx);
		if (needfree)
			lkpi_free(rsp, M_LCINT);
	}
}

int
on_each_cpu(void callback(void *data), void *data, int wait)
{
	struct rendezvous_state rs, *rsp;
	if (wait)
		rsp = &rs;
	else
		rsp = lkpi_malloc(sizeof(*rsp), M_LCINT, M_WAITOK);
	bzero(rsp, sizeof(*rsp));
	rsp->rs_data = data;
	rsp->rs_func = callback;
	rsp->rs_count = mp_ncpus;
	mtx_init(&rsp->rs_mtx, "rs lock", NULL, MTX_SPIN|MTX_RECURSE|MTX_NOWITNESS);

	if (wait) {
		rsp->rs_free = false;
		mtx_lock_spin(&rsp->rs_mtx);
		smp_rendezvous(NULL, rendezvous_callback, rendezvous_wait, rsp);
		msleep_spin(rsp, &rsp->rs_mtx, "rendezvous", 0);
	} else {
		rsp->rs_free = true;
		smp_rendezvous(NULL, callback, NULL, rsp);
	}
	return (0);
}

/*
 * XXX this leaks right now, we need to track
 * this memory so that it's freed on return from
 * the compatibility ioctl calls
 */
void *
compat_alloc_user_space(unsigned long len)
{

	return (malloc(len, M_LCINT, M_NOWAIT));
}

void *
memdup_user(const void *ubuf, size_t len)
{
	void *kbuf;
	int rc;

	kbuf = lkpi_malloc(len, M_KMALLOC, M_WAITOK);
	rc = copyin(ubuf, kbuf, len);
	if (rc) {
		lkpi_free(kbuf, M_KMALLOC);
		return ERR_PTR(-EFAULT);
	}
	return (kbuf);
}

unsigned long
clear_user(void *uptr, unsigned long len)
{
	int i, iter, rem;

	rem = len % 8;
	iter = len / 8;

	for (i = 0; i < iter; i++) {
		if (suword64(((uint64_t *)uptr) + iter, 0))
			return (len);
	}
	for (i = 0; i < rem; i++) {
		if (subyte(((uint8_t *)uptr) + iter*8 + i , 0))
			return (len);
	}
	return (0);
}


int
kobject_set_name_vargs(struct kobject *kobj, const char *fmt, va_list args)
{
	va_list tmp_va;
	int len;
	char *old;
	char *name;
	char dummy;

	old = kobj->name;

	if (old && fmt == NULL)
		return (0);

	/* compute length of string */
	va_copy(tmp_va, args);
	len = vsnprintf(&dummy, 0, fmt, tmp_va);
	va_end(tmp_va);

	/* account for zero termination */
	len++;

	/* check for error */
	if (len < 1)
		return (-EINVAL);

	/* allocate memory for string */
	name = kzalloc(len, GFP_KERNEL);
	if (name == NULL)
		return (-ENOMEM);
	vsnprintf(name, len, fmt, args);
	kobj->name = name;

	/* free old string */
	kfree(old);

	/* filter new string */
	for (; *name != '\0'; name++)
		if (*name == '/')
			*name = '!';
	return (0);
}

int
kobject_set_name(struct kobject *kobj, const char *fmt, ...)
{
	va_list args;
	int error;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);

	return (error);
}

static int
kobject_add_complete(struct kobject *kobj, struct kobject *parent)
{
	const struct kobj_type *t;
	int error;

	kobj->parent = kobject_get(parent);
	error = sysfs_create_dir_ns(kobj, NULL);
	if (error == 0 && kobj->ktype && kobj->ktype->default_attrs) {
		struct attribute **attr;
		t = kobj->ktype;

		for (attr = t->default_attrs; *attr != NULL; attr++) {
			error = sysfs_create_file(kobj, *attr);
			if (error)
				break;
		}
		if (error)
			sysfs_remove_dir(kobj);
		
	}
	if (error == 0)
		kobj->state_in_sysfs = 1;
	return (error);
}

int
kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int error;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);
	if (error)
		return (error);

	return kobject_add_complete(kobj, parent);
}

void
linux_kobject_release(struct kref *kref)
{
	struct kobject *kobj;
	char *name;

	kobj = container_of(kref, struct kobject, kref);
	/* we need to work out how to do this in a way that it works */
	if (kobj->state_in_sysfs) {
		kobject_del(kobj);
	}
	name = kobj->name;
	if (kobj->ktype && kobj->ktype->release)
		kobj->ktype->release(kobj);
	kfree(name);
}

static void
linux_kobject_kfree(struct kobject *kobj)
{
	kfree(kobj);
}

static void
linux_kobject_kfree_name(struct kobject *kobj)
{
	if (kobj) {
		kfree(kobj->name);
	}
}

const struct kobj_type linux_kfree_type = {
	.release = linux_kobject_kfree
};

static void
linux_device_release(struct device *dev)
{
	pr_debug("linux_device_release: %s\n", dev_name(dev));
	kfree(dev);
}

static ssize_t
linux_class_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct class_attribute *dattr;
	ssize_t error;

	dattr = container_of(attr, struct class_attribute, attr);
	error = -EIO;
	if (dattr->show)
		error = dattr->show(container_of(kobj, struct class, kobj),
		    dattr, buf);
	return (error);
}

static ssize_t
linux_class_store(struct kobject *kobj, struct attribute *attr, const char *buf,
    size_t count)
{
	struct class_attribute *dattr;
	ssize_t error;

	dattr = container_of(attr, struct class_attribute, attr);
	error = -EIO;
	if (dattr->store)
		error = dattr->store(container_of(kobj, struct class, kobj),
		    dattr, buf, count);
	return (error);
}

static void
linux_class_release(struct kobject *kobj)
{
	struct class *class;

	class = container_of(kobj, struct class, kobj);
	if (class->class_release)
		class->class_release(class);
}

static const struct sysfs_ops linux_class_sysfs = {
	.show  = linux_class_show,
	.store = linux_class_store,
};

const struct kobj_type linux_class_ktype = {
	.release = linux_class_release,
	.sysfs_ops = &linux_class_sysfs
};

static void
linux_dev_release(struct kobject *kobj)
{
	struct device *dev;

	dev = container_of(kobj, struct device, kobj);
	/* This is the precedence defined by linux. */
	if (dev->release)
		dev->release(dev);
	else if (dev->class && dev->class->dev_release)
		dev->class->dev_release(dev);
}

static ssize_t
linux_dev_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct device_attribute *dattr;
	ssize_t error;

	dattr = container_of(attr, struct device_attribute, attr);
	error = -EIO;
	if (dattr->show)
		error = dattr->show(container_of(kobj, struct device, kobj),
		    dattr, buf);
	return (error);
}

static ssize_t
linux_dev_store(struct kobject *kobj, struct attribute *attr, const char *buf,
    size_t count)
{
	struct device_attribute *dattr;
	ssize_t error;

	dattr = container_of(attr, struct device_attribute, attr);
	error = -EIO;
	if (dattr->store)
		error = dattr->store(container_of(kobj, struct device, kobj),
		    dattr, buf, count);
	return (error);
}

static const struct sysfs_ops linux_dev_sysfs = {
	.show  = linux_dev_show,
	.store = linux_dev_store,
};

const struct kobj_type linux_dev_ktype = {
	.release = linux_dev_release,
	.sysfs_ops = &linux_dev_sysfs
};

struct device *
device_create(struct class *class, struct device *parent, dev_t devt,
    void *drvdata, const char *fmt, ...)
{
	struct device *dev;
	va_list args;

	dev = kzalloc(sizeof(*dev), M_WAITOK);
	dev->parent = parent;
	dev->class = class;
	dev->devt = devt;
	dev->driver_data = drvdata;
	dev->release = linux_device_release;
	va_start(args, fmt);
	kobject_set_name_vargs(&dev->kobj, fmt, args);
	va_end(args);
	device_register(dev);

	return (dev);
}

int
kobject_init_and_add(struct kobject *kobj, const struct kobj_type *ktype,
    struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int error;

	kobject_init(kobj, ktype);
	kobj->ktype = ktype;
	kobj->parent = parent;
	kobj->name = NULL;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);
	if (error)
		return (error);
	return kobject_add_complete(kobj, parent);
}

int
linux_alloc_current(int flags)
{
	struct mm_struct *mm;
	struct task_struct *t;
	struct thread *td;

	td = curthread;
	MPASS(__predict_true(td->td_lkpi_task == NULL));

	if ((t = malloc(sizeof(*t), M_LCINT, flags|M_ZERO)) == NULL)
		return (ENOMEM);
	task_struct_fill(td, t);
	mm = t->mm;
	init_rwsem(&mm->mmap_sem);
	mm->mm_count.counter = 1;
	mm->mm_users.counter = 1;
	curthread->td_lkpi_task = t;
	return (0);
}

static void
linux_file_dtor(void *cdp)
{
	struct linux_file *filp;

	linux_set_current();
	filp = cdp;
	filp->f_op->release(filp->f_vnode, filp);
	vdrop(filp->f_vnode);
	kfree(filp);
}


#define PFN_TO_VM_PAGE(pfn) PHYS_TO_VM_PAGE((pfn) << PAGE_SHIFT)

static inline vm_map_entry_t
vm_map_find_object_entry(vm_map_t map, vm_object_t obj)
{
	vm_map_entry_t entry;

	MPASS(map->root != NULL);

	entry = &map->header;
	do {
		if (entry->object.vm_object == obj)
			return (entry);
		entry = entry->next;
	} while (entry != NULL);
	MPASS(entry != NULL);

	return (NULL);
}

static inline void
vm_area_set_object_bounds(vm_map_t map, vm_object_t obj, struct vm_area_struct *vmap)
{
	vm_map_entry_t entry;
	int needunlock = 0;
	
	if (__predict_true(vmap->vm_cached_map == map))
		return;
	if (!sx_xlocked(&map->lock)) {
		vm_map_lock_read(map);
		needunlock = 1;
	}
	entry = vm_map_find_object_entry(map, obj);
	if (needunlock)
		vm_map_unlock_read(map);

	MPASS(entry != NULL);
	vmap->vm_cached_map = map;
	vmap->vm_start = entry->start & ~(PAGE_SIZE-1);
	vmap->vm_end = entry->end & ~(PAGE_SIZE-1);
}

static int
linux_cdev_pager_fault(vm_object_t vm_obj, vm_ooffset_t offset, int prot, vm_page_t *mres)
{
	struct vm_fault vmf;
	struct vm_area_struct *vmap, cvma;
	int rc, err;
	vm_map_t map;

	linux_set_current();

	vmap  = vm_obj->handle;
	/*
	 * We can be fairly certain that these aren't 
	 * the pages we're looking for.
	 */
	if (mres) {
		vm_page_lock(*mres);
		vm_page_remove(*mres);
		vm_page_unlock(*mres);
	}

	trace_compat_cdev_pager_fault(vm_obj, offset, prot, mres);
	vm_object_pip_add(vm_obj, 1);
	VM_OBJECT_WUNLOCK(vm_obj);
	map = &curproc->p_vmspace->vm_map;
	vm_area_set_object_bounds(map, vm_obj, vmap);
retry:
	memcpy(&cvma, vmap, sizeof(cvma));
	vmf.virtual_address = (void *)(vmap->vm_start + offset);
	vmf.flags = (prot & VM_PROT_WRITE) ? FAULT_FLAG_WRITE : 0;
	cvma.vm_pfn_count = 0;
	cvma.vm_pfn_pcount = &cvma.vm_pfn_count;
	err = vmap->vm_ops->fault(&cvma, &vmf);
	if (cvma.vm_pfn_count == 0) {
		kern_yield(0);
		goto retry;
	}

	VM_OBJECT_WLOCK(vm_obj);
	if (__predict_false(err != VM_FAULT_NOPAGE))
		goto err;
	vm_object_pip_wakeup(vm_obj);
	VM_OBJECT_WUNLOCK(vm_obj);

	atomic_add_int(&cdev_pfn_found_count, 1);

	/*
	 * The VM has helpfully given us pages, but device memory
	 * is not fungible. Thus we need to remove them from the object
	 * in order to replace them with device addresses that we can 
	 * actually use. We don't free them unless we succeed, so that 
	 * there is still a valid result page on failure.
	 */
	vm_page_lock(*mres);
	vm_page_free(*mres);
	vm_page_unlock(*mres);
	return (VM_PAGER_NOPAGE);
err:
	switch (err) {
	case VM_FAULT_OOM:
		rc = VM_PAGER_AGAIN;
		break;
	case VM_FAULT_SIGBUS:
		rc = VM_PAGER_BAD;
		break;
	case VM_FAULT_NOPAGE:
		rc = VM_PAGER_ERROR;
		break;
	default:
		panic("unexpected error %d\n", err);
		rc = VM_PAGER_ERROR;
	}
	vm_object_pip_wakeup(vm_obj);
	return (rc);
}


static int
linux_cdev_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
		      vm_ooffset_t foff, struct ucred *cred, u_short *color)
{
	struct vm_area_struct *vmap = handle;

	vmap->vm_ops->open(vmap);
	*color = 0;
	return (0);
}

static void
linux_cdev_pager_dtor(void *handle)
{
	struct vm_area_struct *vmap = handle;

	vmap->vm_ops->close(vmap);
	free(vmap, M_LCINT);
}

static struct cdev_pager_ops linux_cdev_pager_ops = {
	.cdev_pg_fault	= linux_cdev_pager_fault,
	.cdev_pg_ctor	= linux_cdev_pager_ctor,
	.cdev_pg_dtor	= linux_cdev_pager_dtor
};

static void
linux_dev_deferred_note(unsigned long arg)
{
	struct linux_file *filp = (struct linux_file *)arg;

	spin_lock(&filp->f_lock);
	KNOTE_LOCKED(&filp->f_selinfo.si_note, 1);
	spin_unlock(&filp->f_lock);
}

static void
kq_lock(void *arg)
{
	spinlock_t *s = arg;

	spin_lock(s);
}
static void
kq_unlock(void *arg)
{
	spinlock_t *s = arg;

	spin_unlock(s);
}

static void
kq_lock_owned(void *arg)
{
#ifdef INVARIANTS
	spinlock_t *s = arg;

	mtx_assert(&s->m, MA_OWNED);
#endif
}

static void
kq_lock_unowned(void *arg)
{
#ifdef INVARIANTS
	spinlock_t *s = arg;

	mtx_assert(&s->m, MA_NOTOWNED);
#endif
}

static int
linux_dev_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	struct tasklet_struct *t;
	int error;

	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (ENODEV);
	filp = kzalloc(sizeof(*filp), GFP_KERNEL);
	filp->f_dentry = &filp->f_dentry_store;
	filp->f_op = ldev->ops;
	filp->f_flags = file->f_flag;
	vhold(file->f_vnode);
	filp->f_vnode = file->f_vnode;
	linux_set_current();
	INIT_LIST_HEAD(&filp->f_entry);
	t = &filp->f_kevent_tasklet;
	tasklet_init(t, linux_dev_deferred_note, (u_long)filp);
	spin_lock_init(&filp->f_lock);
	knlist_init(&filp->f_selinfo.si_note, &filp->f_lock, kq_lock, kq_unlock,
		    kq_lock_owned, kq_lock_unowned);

	if (filp->f_op->open) {
		error = -filp->f_op->open(file->f_vnode, filp);
		if (error) {
			kfree(filp);
			goto done;
		}
	}
	error = devfs_set_cdevpriv(filp, linux_file_dtor);
	if (error) {
		filp->f_op->release(file->f_vnode, filp);
		kfree(filp);
	}
done:
	return (error);
}

static int
linux_dev_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	int error;

	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
        devfs_clear_cdevpriv();
        

	return (0);
}

#define	LINUX_IOCTL_MIN_PTR 0x10000UL
#define	LINUX_IOCTL_MAX_PTR (LINUX_IOCTL_MIN_PTR + IOCPARM_MAX)

static inline int
linux_remap_address(void **uaddr, size_t len)
{
	uintptr_t uaddr_val = (uintptr_t)(*uaddr);

	if (unlikely(uaddr_val >= LINUX_IOCTL_MIN_PTR &&
	    uaddr_val < LINUX_IOCTL_MAX_PTR)) {
		struct task_struct *pts = current;
		if (pts == NULL) {
			*uaddr = NULL;
			return (1);
		}

		/* compute data offset */
		uaddr_val -= LINUX_IOCTL_MIN_PTR;

		/* check that length is within bounds */
		if ((len > IOCPARM_MAX) ||
		    (uaddr_val + len) > pts->bsd_ioctl_len) {
			*uaddr = NULL;
			return (1);
		}

		/* re-add kernel buffer address */
		uaddr_val += (uintptr_t)pts->bsd_ioctl_data;

		/* update address location */
		*uaddr = (void *)uaddr_val;
		return (1);
	}
	return (0);
}

int
linux_copyin(const void *uaddr, void *kaddr, size_t len)
{
	if (linux_remap_address(__DECONST(void **, &uaddr), len)) {
		if (uaddr == NULL)
			return (-EFAULT);
		memcpy(kaddr, uaddr, len);
		return (0);
	}
	return (-copyin(uaddr, kaddr, len));
}

int
linux_copyout(const void *kaddr, void *uaddr, size_t len)
{
	if (linux_remap_address(&uaddr, len)) {
		if (uaddr == NULL)
			return (-EFAULT);
		memcpy(uaddr, kaddr, len);
		return (0);
	}
	return (-copyout(kaddr, uaddr, len));
}

static int
linux_dev_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	unsigned size;
	int error;

	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;

	linux_set_current();
	size = IOCPARM_LEN(cmd);
	/* refer to logic in sys_ioctl() */
	if (size > 0) {
		/*
		 * Setup hint for linux_copyin() and linux_copyout().
		 *
		 * Background: Linux code expects a user-space address
		 * while FreeBSD supplies a kernel-space address.
		 */
		current->bsd_ioctl_data = data;
		current->bsd_ioctl_len = size;
		data = (void *)LINUX_IOCTL_MIN_PTR;
	} else {
		/* fetch user-space pointer */
		data = *(void **)data;
	}
	if (filp->f_op->unlocked_ioctl)
		error = -filp->f_op->unlocked_ioctl(filp, cmd, (u_long)data);
	else
		error = ENOTTY;
	if (size > 0) {
		current->bsd_ioctl_data = NULL;
		current->bsd_ioctl_len = -1;
	}
	
	return (error);
}

static int
linux_dev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct thread *td;
	struct file *file;
	ssize_t bytes;
	int error;

	td = curthread;
	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	/* XXX no support for I/O vectors currently */
	if (uio->uio_iovcnt != 1)
		return (EOPNOTSUPP);
	linux_set_current();
	if (filp->f_op->read) {
		bytes = filp->f_op->read(filp, uio->uio_iov->iov_base,
					 uio->uio_iov->iov_len, &uio->uio_offset);
		if (bytes >= 0) {
			uio->uio_iov->iov_base =
			    ((uint8_t *)uio->uio_iov->iov_base) + bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
		} else
			error = -bytes;
	} else
		error = ENXIO;

	return (error);
}

static int
linux_dev_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct thread *td;
	struct file *file;
	ssize_t bytes;
	int error;

	td = curthread;
	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	/* XXX no support for I/O vectors currently */
	if (uio->uio_iovcnt != 1)
		return (EOPNOTSUPP);
	linux_set_current();
	if (filp->f_op->write) {
		bytes = filp->f_op->write(filp, uio->uio_iov->iov_base,
					  uio->uio_iov->iov_len, &uio->uio_offset);
		if (bytes >= 0) {
			uio->uio_iov->iov_base =
			    ((uint8_t *)uio->uio_iov->iov_base) + bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
		} else
			error = -bytes;
	} else
		error = ENXIO;

	return (error);
}

static int
linux_dev_poll(struct cdev *dev, int events, struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct poll_wqueues table;
	struct file *file;
	int revents;
	int error;

	file = td->td_fpop;
	ldev = dev->si_drv1;
	revents = 0;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	if (filp->_file == NULL)
		filp->_file = td->td_fpop;

	linux_set_current();
	if (filp->f_op->poll) {
		/* XXX need to add support for bounded wait */
		poll_initwait(&table);
		revents = filp->f_op->poll(filp, &table.pt) & events;
		poll_freewait(&table);
	}

	return (revents);
}

static int
linux_dev_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct linux_file *filp;
	struct file *file;
	struct poll_wqueues table;
	struct thread *td;
	int error, revents;
	struct linux_cdev *ldev;

	ldev = dev->si_drv1;
	td = curthread;
	file = td->td_fpop;
	revents = 0;
	if (ldev == NULL)
		return (ENXIO);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	if (filp->_file == NULL)
		filp->_file = td->td_fpop;
	if (filp->f_op->poll == NULL || kn->kn_filter != EVFILT_READ || filp->f_kqfiltops == NULL)
		return (EINVAL);

	if (kn->kn_filter == EVFILT_READ) {
		kn->kn_fop = filp->f_kqfiltops;
		kn->kn_hook = filp;
		spin_lock(&filp->f_lock);
		knlist_add(&filp->f_selinfo.si_note, kn, 1);
		spin_unlock(&filp->f_lock);
	} else
		return (EINVAL);

	linux_set_current();
	kevent_initwait(&table);
	revents = filp->f_op->poll(filp, &table.pt);

	if (revents) {
		spin_lock(&filp->f_lock);
		KNOTE_LOCKED(&filp->f_selinfo.si_note, 0);
		spin_unlock(&filp->f_lock);
	}
	return (0);
}

static int
linux_dev_mmap_single(struct cdev *dev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **object, int nprot)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct thread *td;
	struct file *file;
	struct vm_area_struct vma, *vmap;
	vm_memattr_t attr;
	int error;

	td = curthread;
	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (ENODEV);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	linux_set_current();
	vma.vm_start = 0;
	vma.vm_end = size;
	vma.vm_pgoff = *offset / PAGE_SIZE;
	vma.vm_pfn = 0;
	vma.vm_flags = vma.vm_page_prot = nprot;
	vma.vm_ops = NULL;
	vma.vm_file = filp;
	if (filp->f_op->mmap) {
		error = -filp->f_op->mmap(filp, &vma);
		if (error == 0) {
			struct sglist *sg;

			attr = pgprot2cachemode(vma.vm_page_prot);
			if (vma.vm_ops != NULL && vma.vm_ops->fault != NULL) {
				MPASS(vma.vm_ops->open != NULL);
				MPASS(vma.vm_ops->close != NULL);
				vmap = malloc(sizeof(*vmap), M_LCINT, M_WAITOK);
				memcpy(vmap, &vma, sizeof(*vmap));
				*object = cdev_pager_allocate(vmap, OBJT_MGTDEVICE, &linux_cdev_pager_ops,
							      size, nprot,
							      0, curthread->td_ucred);

				VM_OBJECT_WLOCK(*object);
				(*object)->flags2 |= OBJ2_GRAPHICS;
				VM_OBJECT_WUNLOCK(*object);
				if (*object != NULL)
					vmap->vm_obj = *object;
			} else {
				sg = sglist_alloc(1, M_WAITOK);
				sglist_append_phys(sg,
						   (vm_paddr_t)vma.vm_pfn << PAGE_SHIFT, vma.vm_len);
				*object = vm_pager_allocate(OBJT_SG, sg, vma.vm_len,
							    nprot, 0, curthread->td_ucred);
				if (*object == NULL) {
					sglist_free(sg);
					return (EINVAL);
				}
			}
			if (attr != VM_MEMATTR_DEFAULT) {
				VM_OBJECT_WLOCK(*object);
				vm_object_set_memattr(*object, attr);
				VM_OBJECT_WUNLOCK(*object);
			}
			*offset = 0;
		}
	} else
		error = ENODEV;
	return (error);
}

struct cdevsw linuxcdevsw = {
	.d_version = D_VERSION,
	.d_flags = D_TRACKCLOSE,
	.d_open = linux_dev_open,
	.d_close = linux_dev_close,
	.d_read = linux_dev_read,
	.d_write = linux_dev_write,
	.d_ioctl = linux_dev_ioctl,
	.d_mmap_single = linux_dev_mmap_single,
	.d_poll = linux_dev_poll,
	.d_kqfilter = linux_dev_kqfilter,
	.d_name = "lkpidev",
};

static int
linux_file_read(struct file *file, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct linux_file *filp;
	ssize_t bytes;
	int error;

	error = 0;
	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	/* XXX no support for I/O vectors currently */
	if (uio->uio_iovcnt != 1)
		return (EOPNOTSUPP);
	linux_set_current();
	if (filp->f_op->read) {
		bytes = filp->f_op->read(filp, uio->uio_iov->iov_base,
		    uio->uio_iov->iov_len, &uio->uio_offset);
		if (bytes >= 0) {
			uio->uio_iov->iov_base =
			    ((uint8_t *)uio->uio_iov->iov_base) + bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
		} else
			error = -bytes;
	} else
		error = ENXIO;

	return (error);
}

static int
linux_file_poll(struct file *file, int events, struct ucred *active_cred,
    struct thread *td)
{
	struct linux_file *filp;
	struct poll_wqueues table;
	int revents;

	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	if (filp->_file == NULL)
		filp->_file = td->td_fpop;
	linux_set_current();
	if (filp->f_op->poll) {
		poll_initwait(&table);
		revents = filp->f_op->poll(filp, &table.pt) & events;
		poll_freewait(&table);
	} else
		revents = 0;

	return (revents);
}

static int
linux_file_close(struct file *file, struct thread *td)
{
	struct linux_file *filp;
	int error;

	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	linux_set_current();
	error = -filp->f_op->release(NULL, filp);
	funsetown(&filp->f_sigio);
	kfree(filp);

	return (error);
}

static int
linux_file_ioctl(struct file *fp, u_long cmd, void *data, struct ucred *cred,
    struct thread *td)
{
	struct linux_file *filp;
	int error;

	filp = (struct linux_file *)fp->f_data;
	filp->f_flags = fp->f_flag;
	error = 0;

	linux_set_current();
	switch (cmd) {
	case FIONBIO:
		break;
	case FIOASYNC:
		if (filp->f_op->fasync == NULL)
			break;
		error = filp->f_op->fasync(0, filp, fp->f_flag & FASYNC);
		break;
	case FIOSETOWN:
		error = fsetown(*(int *)data, &filp->f_sigio);
		if (error == 0)
			error = filp->f_op->fasync(0, filp,
			    fp->f_flag & FASYNC);
		break;
	case FIOGETOWN:
		*(int *)data = fgetown(&filp->f_sigio);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static int
linux_file_stat(struct file *fp, struct stat *sb, struct ucred *active_cred,
    struct thread *td)
{

	return (EOPNOTSUPP);
}

static int
linux_file_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{

	return (0);
}

struct fileops linuxfileops = {
	.fo_read = linux_file_read,
	.fo_write = invfo_rdwr,
	.fo_truncate = invfo_truncate,
	.fo_kqfilter = invfo_kqfilter,
	.fo_stat = linux_file_stat,
	.fo_fill_kinfo = linux_file_fill_kinfo,
	.fo_poll = linux_file_poll,
	.fo_close = linux_file_close,
	.fo_ioctl = linux_file_ioctl,
	.fo_chmod = invfo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
};


char *
kvasprintf(gfp_t gfp, const char *fmt, va_list ap)
{
	unsigned int len;
	char *p;
	va_list aq;

	va_copy(aq, ap);
	len = vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);

	p = kmalloc(len + 1, gfp);
	if (p != NULL)
		vsnprintf(p, len + 1, fmt, ap);

	return (p);
}

char *
kasprintf(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf(gfp, fmt, ap);
	va_end(ap);

	return (p);
}

static void
linux_timer_callback_wrapper(void *context)
{
	struct timer_list *timer;

	timer = context;
	timer->function(timer->data);
}

void
mod_timer(struct timer_list *timer, unsigned long expires)
{

	timer->expires = expires;
	callout_reset(&timer->timer_callout,		      
	    linux_timer_jiffies_until(expires),
	    &linux_timer_callback_wrapper, timer);
}

void
add_timer(struct timer_list *timer)
{

	callout_reset(&timer->timer_callout,
	    linux_timer_jiffies_until(timer->expires),
	    &linux_timer_callback_wrapper, timer);
}

void
add_timer_on(struct timer_list *timer, int cpu)
{

	callout_reset_on(&timer->timer_callout,
	    linux_timer_jiffies_until(timer->expires),
	    &linux_timer_callback_wrapper, timer, cpu);
}

static void
linux_timer_init(void *arg)
{

	/*
	 * Compute an internal HZ value which can divide 2**32 to
	 * avoid timer rounding problems when the tick value wraps
	 * around 2**32:
	 */
	linux_timer_hz_mask = 1;
	while (linux_timer_hz_mask < (unsigned long)hz)
		linux_timer_hz_mask *= 2;
	linux_timer_hz_mask--;
}
SYSINIT(linux_timer, SI_SUB_DRIVERS, SI_ORDER_FIRST, linux_timer_init, NULL);

void
linux_complete_common(struct completion *c, int all)
{
	int wakeup_swapper;

	sleepq_lock(c);
	c->done++;
	if (all)
		wakeup_swapper = sleepq_broadcast(c, SLEEPQ_SLEEP, 0, 0);
	else
		wakeup_swapper = sleepq_signal(c, SLEEPQ_SLEEP, 0, 0);
	sleepq_release(c);
	if (wakeup_swapper)
		kick_proc0();
}

/*
 * Indefinite wait for done != 0 with or without signals.
 */
long
linux_wait_for_common(struct completion *c, int flags)
{

	if (unlikely(SKIP_SLEEP()))
		return (0);

	if (flags != 0)
		flags = SLEEPQ_INTERRUPTIBLE | SLEEPQ_SLEEP;
	else
		flags = SLEEPQ_SLEEP;
	for (;;) {
		sleepq_lock(c);
		if (c->done)
			break;
		sleepq_add(c, NULL, "completion", flags, 0);
		if (flags & SLEEPQ_INTERRUPTIBLE) {
			if (sleepq_wait_sig(c, 0) != 0)
				return (-ERESTARTSYS);
		} else
			sleepq_wait(c, 0);
	}
	c->done--;
	sleepq_release(c);

	return (0);
}

/*
 * Time limited wait for done != 0 with or without signals.
 */
long
linux_wait_for_timeout_common(struct completion *c, long timeout, int flags)
{
	long end = jiffies + timeout;

	if (SKIP_SLEEP())
		return (0);

	if (flags != 0)
		flags = SLEEPQ_INTERRUPTIBLE | SLEEPQ_SLEEP;
	else
		flags = SLEEPQ_SLEEP;
	for (;;) {
		int ret;

		sleepq_lock(c);
		if (c->done)
			break;
		sleepq_add(c, NULL, "completion", flags, 0);
		sleepq_set_timeout(c, linux_timer_jiffies_until(end));
		if (flags & SLEEPQ_INTERRUPTIBLE)
			ret = sleepq_timedwait_sig(c, 0);
		else
			ret = sleepq_timedwait(c, 0);
		if (ret != 0) {
			/* check for timeout or signal */
			if (ret == EWOULDBLOCK)
				return (0);
			else
				return (-ERESTARTSYS);
		}
	}
	c->done--;
	sleepq_release(c);

	/* return how many jiffies are left */
	return (linux_timer_jiffies_until(end));
}

int
linux_try_wait_for_completion(struct completion *c)
{
	int isdone;

	isdone = 1;
	sleepq_lock(c);
	if (c->done)
		c->done--;
	else
		isdone = 0;
	sleepq_release(c);
	return (isdone);
}

int
linux_completion_done(struct completion *c)
{
	int isdone;

	isdone = 1;
	sleepq_lock(c);
	if (c->done == 0)
		isdone = 0;
	sleepq_release(c);
	return (isdone);
}

static void
linux_cdev_release(struct kobject *kobj)
{
	struct linux_cdev *cdev;
	struct kobject *parent;

	cdev = container_of(kobj, struct linux_cdev, kobj);
	parent = kobj->parent;
	if (cdev->cdev)
		destroy_dev(cdev->cdev);
	kfree(cdev);
	kobject_put(parent);
}

static void
linux_cdev_static_release(struct kobject *kobj)
{
	struct linux_cdev *cdev;
	struct kobject *parent;

	cdev = container_of(kobj, struct linux_cdev, kobj);
	parent = kobj->parent;
	if (cdev->cdev)
		destroy_dev(cdev->cdev);
	kobject_put(parent);
}

const struct kobj_type linux_cdev_ktype = {
	.release = linux_cdev_release,
};

const struct kobj_type linux_cdev_static_ktype = {
	.release = linux_cdev_static_release,
};

static void
linux_handle_ifnet_link_event(void *arg, struct ifnet *ifp, int linkstate)
{
	struct notifier_block *nb;

	nb = arg;
	if (linkstate == LINK_STATE_UP)
		nb->notifier_call(nb, NETDEV_UP, ifp);
	else
		nb->notifier_call(nb, NETDEV_DOWN, ifp);
}

static void
linux_handle_ifnet_arrival_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_REGISTER, ifp);
}

static void
linux_handle_ifnet_departure_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_UNREGISTER, ifp);
}

static void
linux_handle_iflladdr_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_CHANGEADDR, ifp);
}

static void
linux_handle_ifaddr_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_CHANGEIFADDR, ifp);
}

int
register_netdevice_notifier(struct notifier_block *nb)
{

	nb->tags[NETDEV_UP] = EVENTHANDLER_REGISTER(
	    ifnet_link_event, linux_handle_ifnet_link_event, nb, 0);
	nb->tags[NETDEV_REGISTER] = EVENTHANDLER_REGISTER(
	    ifnet_arrival_event, linux_handle_ifnet_arrival_event, nb, 0);
	nb->tags[NETDEV_UNREGISTER] = EVENTHANDLER_REGISTER(
	    ifnet_departure_event, linux_handle_ifnet_departure_event, nb, 0);
	nb->tags[NETDEV_CHANGEADDR] = EVENTHANDLER_REGISTER(
	    iflladdr_event, linux_handle_iflladdr_event, nb, 0);

	return (0);
}

int
register_inetaddr_notifier(struct notifier_block *nb)
{

        nb->tags[NETDEV_CHANGEIFADDR] = EVENTHANDLER_REGISTER(
            ifaddr_event, linux_handle_ifaddr_event, nb, 0);
        return (0);
}

int
unregister_netdevice_notifier(struct notifier_block *nb)
{

        EVENTHANDLER_DEREGISTER(ifnet_link_event,
	    nb->tags[NETDEV_UP]);
        EVENTHANDLER_DEREGISTER(ifnet_arrival_event,
	    nb->tags[NETDEV_REGISTER]);
        EVENTHANDLER_DEREGISTER(ifnet_departure_event,
	    nb->tags[NETDEV_UNREGISTER]);
        EVENTHANDLER_DEREGISTER(iflladdr_event,
	    nb->tags[NETDEV_CHANGEADDR]);

	return (0);
}

int
unregister_inetaddr_notifier(struct notifier_block *nb)
{

        EVENTHANDLER_DEREGISTER(ifaddr_event,
            nb->tags[NETDEV_CHANGEIFADDR]);

        return (0);
}

struct list_sort_thunk {
	int (*cmp)(void *, struct list_head *, struct list_head *);
	void *priv;
};

static inline int
linux_le_cmp(void *priv, const void *d1, const void *d2)
{
	struct list_head *le1, *le2;
	struct list_sort_thunk *thunk;

	thunk = priv;
	le1 = *(__DECONST(struct list_head **, d1));
	le2 = *(__DECONST(struct list_head **, d2));
	return ((thunk->cmp)(thunk->priv, le1, le2));
}

void
list_sort(void *priv, struct list_head *head, int (*cmp)(void *priv,
    struct list_head *a, struct list_head *b))
{
	struct list_sort_thunk thunk;
	struct list_head **ar, *le;
	size_t count, i;

	count = 0;
	list_for_each(le, head)
		count++;
	ar = lkpi_malloc(sizeof(struct list_head *) * count, M_KMALLOC, M_WAITOK);
	i = 0;
	list_for_each(le, head)
		ar[i++] = le;
	thunk.cmp = cmp;
	thunk.priv = priv;
	qsort_r(ar, count, sizeof(struct list_head *), &thunk, linux_le_cmp);
	INIT_LIST_HEAD(head);
	for (i = 0; i < count; i++)
		list_add_tail(ar[i], head);
	lkpi_free(ar, M_KMALLOC);
}

int
linux_irq_handler(void *ent)
{
	struct irq_ent *irqe;

	irqe = ent;
	irqe->handler(irqe->irq, irqe->arg);
	return (FILTER_HANDLED);
}

int
in_atomic(void)
{

	return ((curthread->td_pflags & TDP_NOFAULTING) != 0);
}

struct linux_cdev*
find_cdev(const char *name, unsigned int major, int minor, int remove)
{
	struct linux_cdev *cdev;
	struct list_head *h;
	
	sx_xlock(&linux_global_lock);
	list_for_each(h, &cdev_list) {
		cdev = __containerof(h, struct linux_cdev, list);
		if ((strcmp(kobject_name(&cdev->kobj), name) == 0) &&
		    cdev->baseminor == minor &&
		    cdev->major == major) {
			if (remove)
				list_del(&cdev->list);
			sx_xunlock(&linux_global_lock);
			return (cdev);
		}
	}
	sx_xunlock(&linux_global_lock);
	return (NULL);
}


int
__register_chrdev(unsigned int major, unsigned int baseminor,
		  unsigned int count, const char *name,
		  const struct file_operations *fops)
{
	struct linux_cdev *cdev;
	int i, ret;

	for (i = baseminor; i < baseminor + count; i++) {
		cdev = cdev_alloc();
		cdev_init(cdev, fops);
		kobject_set_name(&cdev->kobj, name);

		ret = cdev_add(cdev, i, 1);
		cdev->major = major;
		cdev->baseminor = i;	
		sx_xlock(&linux_global_lock);
		list_add(&cdev->list, &cdev_list);
		sx_xunlock(&linux_global_lock);
	}
	return (ret);
}

int
__register_chrdev_p(unsigned int major, unsigned int baseminor,
		    unsigned int count, const char *name,
		    const struct file_operations *fops, uid_t uid,
		    gid_t gid, int mode)
{
	struct linux_cdev *cdev;
	int i, ret;

	for (i = baseminor; i < baseminor + count; i++) {
		cdev = cdev_alloc();
		cdev_init(cdev, fops);
		kobject_set_name(&cdev->kobj, name);

		ret = cdev_add_ext(cdev, makedev(major, i), uid, gid, mode);
		cdev->major = major;
		cdev->baseminor = i;	
		sx_xlock(&linux_global_lock);
		list_add(&cdev->list, &cdev_list);
		sx_xunlock(&linux_global_lock);
	}
	return (ret);
}

void
__unregister_chrdev(unsigned int major, unsigned int baseminor,
		    unsigned int count, const char *name)
{
	int i;
	struct linux_cdev *cdevp;

	for (i = baseminor; i < count; i++) {
		cdevp = find_cdev(name, major, i, true);
		MPASS(cdevp != NULL);
		if (cdevp != NULL)
			cdev_del(cdevp);
	}
}

static DECLARE_WAIT_QUEUE_HEAD(async_done);
static async_cookie_t nextcookie;
static atomic_t entry_count;

static void
async_run_entry_fn(struct work_struct *work)
{
	struct async_entry *entry; 	

	linux_set_current();
	entry  = container_of(work, struct async_entry, work);
	entry->func(entry->data, entry->cookie);
	kfree(entry);
	atomic_dec(&entry_count);
	wake_up(&async_done);

}

async_cookie_t
async_schedule(async_func_t func, void *data)
{
	struct async_entry *entry;
	async_cookie_t newcookie;

	DODGY();
	entry = kzalloc(sizeof(struct async_entry), GFP_ATOMIC);

	if (entry == NULL) {
		sx_xlock(&linux_global_lock);
		nextcookie++;
		sx_xunlock(&linux_global_lock);
		newcookie = nextcookie;
		func(data, newcookie);
		return (newcookie);
	}

	INIT_WORK(&entry->work, async_run_entry_fn);
	entry->func = func;
	entry->data = data;
	sx_xlock(&linux_global_lock);
	atomic_inc(&entry_count);
	newcookie = entry->cookie = nextcookie++;
	sx_xunlock(&linux_global_lock);
	curthread->td_pflags |= PF_USED_ASYNC;
	queue_work(system_unbound_wq, &entry->work);
	return (newcookie);
}

#ifdef __notyet__
/*
 * XXX
 * The rather broken taskqueue API doesn't allow us to serialize 
 * on a particular thread's queue if we use more than 1 thread
 */
#define MAX_WQ_CPUS mp_ncpus
#else
#define MAX_WQ_CPUS 1
#endif

static void
linux_compat_init(void *arg)
{
	struct sysctl_oid *rootoid;

#if defined(__i386__) || defined(__amd64__)
	if (cpu_feature & CPUID_CLFSH)
		set_bit(X86_FEATURE_CLFLUSH, &boot_cpu_data.x86_capability);
	if (cpu_feature & CPUID_PAT)
		set_bit(X86_FEATURE_PAT, &boot_cpu_data.x86_capability);
#endif
	hwmon_idap = &hwmon_ida;
	sx_init(&linux_global_lock, "LinuxBKL");
	boot_cpu_data.x86_clflush_size = cpu_clflush_line_size;
	boot_cpu_data.x86 = ((cpu_id & 0xF0000) >> 12) | ((cpu_id & 0xF0) >> 4);

	system_long_wq = alloc_workqueue("events_long", 0, MAX_WQ_CPUS);
	system_wq = alloc_workqueue("events", 0, MAX_WQ_CPUS);
	system_power_efficient_wq = alloc_workqueue("power efficient", 0, MAX_WQ_CPUS);
	system_unbound_wq = alloc_workqueue("events_unbound", WQ_UNBOUND, MAX_WQ_CPUS);
	INIT_LIST_HEAD(&cdev_list);
	rootoid = SYSCTL_ADD_ROOT_NODE(NULL,
	    OID_AUTO, "sys", CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, "sys");
	kobject_init(&linux_class_root, &linux_class_ktype);
	kobject_set_name(&linux_class_root, "class");
	linux_class_root.oidp = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(rootoid),
	    OID_AUTO, "class", CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, "class");
	kobject_init(&linux_root_device.kobj, &linux_dev_ktype);
	kobject_set_name(&linux_root_device.kobj, "device");
	linux_root_device.kobj.oidp = SYSCTL_ADD_NODE(NULL,
	    SYSCTL_CHILDREN(rootoid), OID_AUTO, "device", CTLFLAG_RD, NULL,
	    "device");
	linux_root_device.bsddev = root_bus;
	linux_class_misc.name = "misc";
	class_register(&linux_class_misc);
	INIT_LIST_HEAD(&pci_drivers);
	INIT_LIST_HEAD(&pci_devices);
	spin_lock_init(&pci_lock);
}

SYSINIT(linux_compat, SI_SUB_VFS, SI_ORDER_ANY, linux_compat_init, NULL);

static void
linux_compat_uninit(void *arg)
{
	linux_kobject_kfree_name(&linux_class_root);
	linux_kobject_kfree_name(&linux_root_device.kobj);
	linux_kobject_kfree_name(&linux_class_misc.kobj);

	destroy_workqueue(system_long_wq);
	destroy_workqueue(system_wq);
	destroy_workqueue(system_unbound_wq);

	spin_lock_destroy(&pci_lock);
}
SYSUNINIT(linux_compat, SI_SUB_VFS, SI_ORDER_ANY, linux_compat_uninit, NULL);

/*
 * NOTE: Linux frequently uses "unsigned long" for pointer to integer
 * conversion and vice versa, where in FreeBSD "uintptr_t" would be
 * used. Assert these types have the same size, else some parts of the
 * LinuxKPI may not work like expected:
 */
CTASSERT(sizeof(unsigned long) == sizeof(uintptr_t));
