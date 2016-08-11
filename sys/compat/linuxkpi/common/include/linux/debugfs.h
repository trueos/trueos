
#ifndef _DEBUGFS_H_
#define _DEBUGFS_H_

#include <linux/fs.h>
#include <linux/seq_file.h>

#include <linux/types.h>


void debugfs_remove(struct dentry *dentry);

struct dentry *debugfs_create_file(const char *name, umode_t mode,
				   struct dentry *parent, void *data,
				   const struct file_operations *fops);

struct dentry *debugfs_create_dir(const char *name, struct dentry *parent);

void debugfs_remove_recursive(struct dentry *dentry);


#endif
