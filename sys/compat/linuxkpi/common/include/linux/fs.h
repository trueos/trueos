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
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_FS_H_
#define	_LINUX_FS_H_

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/filedesc.h>

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/dcache.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>

struct module;
struct kiocb;
struct iovec;
struct dentry;
struct page;
struct file_lock;
struct pipe_inode_info;
struct vm_area_struct;
struct poll_table_struct;
struct files_struct;
struct super_block;

#define	inode	vnode
#define	i_cdev	v_rdev

#define	S_IRUGO	(S_IRUSR | S_IRGRP | S_IROTH)
#define	S_IWUGO	(S_IWUSR | S_IWGRP | S_IWOTH)

typedef struct files_struct *fl_owner_t;

struct file_operations;

#define address_space vm_object
#define i_mapping v_bufobj.bo_object
#define i_private v_data
#define file_inode(f) ((f)->f_vnode)
/* this value isn't needed by the compat layer */
static inline void i_size_write(void *inode, off_t i_size) { ; }

struct linux_file {
	struct file	*_file;
	const struct file_operations	*f_op;
	void 		*private_data;
	int		f_flags;
	int		f_mode;	/* Just starting mode. */
	struct dentry	*f_dentry;
	struct dentry	f_dentry_store;
	struct selinfo	f_selinfo;
	struct sigio	*f_sigio;
	struct vnode	*f_vnode;
	atomic_long_t		f_count;
	vm_object_t	f_mapping;

	/* kqfilter support */
	struct list_head f_entry;
	struct filterops *f_kqfiltops;
	/* protects f_sigio.si_note and f_entry */
	spinlock_t	f_lock;
};
#define f_inode		f_vnode
#define	file		linux_file
#define	fasync_struct	sigio *

#define	fasync_helper(fd, filp, on, queue)				\
({									\
	if ((on))							\
		*(queue) = &(filp)->f_sigio;				\
	else								\
		*(queue) = NULL;					\
	0;								\
})

#define	kill_fasync(queue, sig, pollstat)				\
do {									\
	if (*(queue) != NULL)						\
		pgsigio(*(queue), (sig), 0);				\
} while (0)

typedef int (*filldir_t)(void *, const char *, int, loff_t, u64, unsigned);

struct file_operations {
	struct module *owner;
	ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
	int (*mmap)(struct file *, struct vm_area_struct *);
	int (*open)(struct inode *, struct file *);
	int (*release)(struct inode *, struct file *);
	int (*fasync)(int, struct file *, int);

/* Although not supported in FreeBSD, to align with Linux code
 * we are adding llseek() only when it is mapped to no_llseek which returns 
 * an illegal seek error
 */
	loff_t (*llseek)(struct file *, loff_t, int);
#if 0
	/* We do not support these methods.  Don't permit them to compile. */
	loff_t (*llseek)(struct file *, loff_t, int);
	ssize_t (*aio_read)(struct kiocb *, const struct iovec *,
	    unsigned long, loff_t);
	ssize_t (*aio_write)(struct kiocb *, const struct iovec *,
	    unsigned long, loff_t);
	int (*readdir)(struct file *, void *, filldir_t);
	int (*ioctl)(struct inode *, struct file *, unsigned int,
	    unsigned long);
	long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
	int (*flush)(struct file *, fl_owner_t id);
	int (*fsync)(struct file *, struct dentry *, int datasync);
	int (*aio_fsync)(struct kiocb *, int datasync);
	int (*lock)(struct file *, int, struct file_lock *);
	ssize_t (*sendpage)(struct file *, struct page *, int, size_t,
	    loff_t *, int);
	unsigned long (*get_unmapped_area)(struct file *, unsigned long,
	    unsigned long, unsigned long, unsigned long);
	int (*check_flags)(int);
	int (*flock)(struct file *, int, struct file_lock *);
	ssize_t (*splice_write)(struct pipe_inode_info *, struct file *,
	    loff_t *, size_t, unsigned int);
	ssize_t (*splice_read)(struct file *, loff_t *,
	    struct pipe_inode_info *, size_t, unsigned int);
	int (*setlease)(struct file *, long, struct file_lock **);
#endif
};
#define	fops_get(fops)	(fops)

#define	FMODE_READ	FREAD
#define	FMODE_WRITE	FWRITE
#define	FMODE_EXEC	FEXEC

/* Alas, no aliases. Too much hassle with bringing module.h everywhere */
#define fops_put(fops) \
	do { if (fops) module_put((fops)->owner); } while(0)
/*
 * This one is to be used *ONLY* from ->open() instances.
 * fops must be non-NULL, pinned down *and* module dependencies
 * should be sufficient to pin the caller down as well.
 */
#define replace_fops(f, fops) \
	do {	\
		struct file *__file = (f); \
		fops_put(__file->f_op); \
		BUG_ON(!(__file->f_op = (fops))); \
	} while(0)

int __register_chrdev(unsigned int major, unsigned int baseminor,
    unsigned int count, const char *name,
    const struct file_operations *fops);
int __register_chrdev_p(unsigned int major, unsigned int baseminor,
    unsigned int count, const char *name,
    const struct file_operations *fops, uid_t uid,
    gid_t gid, int mode);
void __unregister_chrdev(unsigned int major, unsigned int baseminor,
    unsigned int count, const char *name);

static inline void
unregister_chrdev(unsigned int major, const char *name)
{

	__unregister_chrdev(major, 0, 256, name);
}

static inline int
register_chrdev(unsigned int major, const char *name,
    const struct file_operations *fops)
{

	return (__register_chrdev(major, 0, 256, name, fops));
}

static inline int
register_chrdev_p(unsigned int major, const char *name,
    const struct file_operations *fops, uid_t uid, gid_t gid, int mode)
{

	return (__register_chrdev_p(major, 0, 256, name, fops, uid, gid, mode));
}

static inline int
register_chrdev_region(dev_t dev, unsigned range, const char *name)
{

	return 0;
}

static inline void
unregister_chrdev_region(dev_t dev, unsigned range)
{

	return;
}

static inline int
alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count,
			const char *name)
{

	return 0;
}

/* No current support for seek op in FreeBSD */
static inline int
nonseekable_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static inline dev_t
iminor(struct inode *inode)
{

	return (minor(dev2unit(inode->v_rdev)));
}

static inline struct linux_file *
get_file(struct linux_file *f)
{
	fhold(f->_file);
	return (f);
}

extern loff_t default_llseek(struct file *file, loff_t offset, int whence);

static inline loff_t 
no_llseek(struct file *file, loff_t offset, int whence)
{
        return -ESPIPE;
}

static inline loff_t 
noop_llseek(struct file *file, loff_t offset, int whence)
{
        return file->_file->f_offset;
}


unsigned long invalidate_mapping_pages(struct address_space *mapping,
					pgoff_t start, pgoff_t end);

struct page *shmem_read_mapping_page_gfp(struct address_space *as, int idx, gfp_t gfp);

static inline struct page *
shmem_read_mapping_page(struct address_space *as, int idx)
{

	return (shmem_read_mapping_page_gfp(as, idx, 0));
}

extern struct linux_file *shmem_file_setup(char *name, loff_t size, unsigned long flags);

static inline void mapping_set_gfp_mask(struct address_space *m, gfp_t mask) {}
static inline gfp_t mapping_gfp_mask(struct address_space *m)
{
	return (0);
}
void shmem_truncate_range(struct vnode *, loff_t, loff_t);

extern struct address_space *alloc_anon_mapping(size_t);
extern void free_anon_mapping(struct address_space *);

#endif /* _LINUX_FS_H_ */
