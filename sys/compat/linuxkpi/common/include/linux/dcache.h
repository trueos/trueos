#ifndef __LINUX_DCACHE_H
#define __LINUX_DCACHE_H

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/seqlock.h>
#include <linux/cache.h>
#include <linux/rcupdate.h>

struct inode;
struct dentry;
struct path;
struct pfs_node;
#define HASH_LEN_DECLARE u32 hash; u32 len

extern char *simple_dname(struct dentry *, char *, int);
struct qstr {
	union {
		struct {
			HASH_LEN_DECLARE;
		};
		u64 hash_len;
	};
	const unsigned char *name;
};
struct dentry {
	struct inode	*d_inode;
	struct dentry *d_parent;	/* parent directory */
	struct qstr d_name;
	/* FreeBSD */
	struct pfs_node *d_pfs_node;
};



struct dentry_operations {
	int (*d_revalidate)(struct dentry *, unsigned int);
	int (*d_weak_revalidate)(struct dentry *, unsigned int);
	int (*d_hash)(const struct dentry *, struct qstr *);
	int (*d_compare)(const struct dentry *, const struct dentry *,
			unsigned int, const char *, const struct qstr *);
	int (*d_delete)(const struct dentry *);
	void (*d_release)(struct dentry *);
	void (*d_prune)(struct dentry *);
	void (*d_iput)(struct dentry *, struct inode *);
	char *(*d_dname)(struct dentry *, char *, int);
	struct vfsmount *(*d_automount)(struct path *);
	int (*d_manage)(struct dentry *, bool);
	struct inode *(*d_select_inode)(struct dentry *, unsigned);
	struct dentry *(*d_real)(struct dentry *, struct inode *);
} ____cacheline_aligned;

#endif
