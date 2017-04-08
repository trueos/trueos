#ifndef __LINUX_DCACHE_H
#define __LINUX_DCACHE_H

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/cache.h>
#include <linux/rcupdate.h>

struct inode;
struct dentry;
struct path;
struct pfs_node;
#define HASH_LEN_DECLARE u32 hash; u32 len

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

#endif
