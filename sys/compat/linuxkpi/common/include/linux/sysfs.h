/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
#ifndef	_LINUX_SYSFS_H_
#define	_LINUX_SYSFS_H_

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/errno.h>

#include <linux/compiler.h>
#include <linux/types.h>

struct vm_area_struct;

struct linux_file;
struct kobject;
struct module;
struct bin_attribute;
enum kobj_ns_type;

struct attribute {
	const char 	*name;
	struct module	*owner;
	mode_t		mode;
};


struct sysfs_ops {
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, struct attribute *, const char *,
	    size_t);
};

struct attribute_group {
	const char		*name;
	umode_t			(*is_visible)(struct kobject *,
					      struct attribute *, int);
	struct attribute	**attrs;
};


#define	__ATTR(_name, _mode, _show, _store) {				\
	.attr = { .name = __stringify(_name), .mode = _mode },		\
        .show = _show, .store  = _store,				\
}

#define	__ATTR_RO(_name) {						\
	.attr = { .name = __stringify(_name), .mode = 0444 },		\
	.show   = _name##_show,						\
}


#define __ATTR_WO(_name) {						\
	.attr	= { .name = __stringify(_name), .mode = S_IWUSR },	\
	.store	= _name##_store,					\
}

#define __ATTR_RW(_name) __ATTR(_name, (S_IWUSR | S_IRUGO),		\
			 _name##_show, _name##_store)


#define	__ATTR_NULL	{ .attr = { .name = NULL } }

#define __ATTRIBUTE_GROUPS(_name)				\
static const struct attribute_group *_name##_groups[] = {	\
	&_name##_group,						\
	NULL,							\
}

#define ATTRIBUTE_GROUPS(_name)					\
static const struct attribute_group _name##_group = {		\
	.attrs = _name##_attrs,					\
};								\
__ATTRIBUTE_GROUPS(_name)

struct bin_attribute {
	struct attribute	attr;
	size_t			size;
	void			*private;
	ssize_t (*read)(struct linux_file *, struct kobject *, struct bin_attribute *,
			char *, loff_t, size_t);
	ssize_t (*write)(struct linux_file *, struct kobject *, struct bin_attribute *,
			 char *, loff_t, size_t);
	int (*mmap)(struct linux_file *, struct kobject *, struct bin_attribute *attr,
		    struct vm_area_struct *vma);
};
extern int sysfs_create_bin_file(struct kobject *kobj, const struct bin_attribute *attr);
extern void sysfs_remove_bin_file(struct kobject *kobj, const struct bin_attribute *attr);

extern int sysfs_create_file(struct kobject *kobj, const struct attribute *attr);
extern void sysfs_remove_file(struct kobject *kobj, const struct attribute *attr);

extern int __must_check sysfs_create_files(struct kobject *kobj, const struct attribute **attr);
extern void sysfs_remove_files(struct kobject *kobj, const struct attribute **attr);

extern int sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp);
extern void sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp);
extern int sysfs_create_dir_ns(struct kobject *kobj, const void *ns);
extern void sysfs_remove_dir(struct kobject *kobj);
extern int __must_check sysfs_create_link(struct kobject *kobj, struct kobject *target,
				   const char *name);
extern void sysfs_remove_link(struct kobject *kobj, const char *name);


extern int lkpi_sysfs_create_file(struct kobject *kobj, const struct attribute *attr);
extern void lkpi_sysfs_remove_file(struct kobject *kobj, const struct attribute *attr);

extern int lkpi_sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp);
extern void lkpi_sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp);
extern int sysfs_merge_group(struct kobject *kobj, const struct attribute_group *grp);
extern void sysfs_unmerge_group(struct kobject *kobj, const struct attribute_group *grp);

extern int lkpi_sysfs_create_dir(struct kobject *kobj);
extern void lkpi_sysfs_remove_dir(struct kobject *kobj);

struct class;
struct pfs_node *linsysfs_create_class_dir(struct class *class, const char *name);
void linsysfs_destroy_class_dir(struct class *class);

/*
 * Handle our generic '\0' terminated 'C' string.
 * Two cases:
 *      a variable string:  point arg1 at it, arg2 is max length.
 *      a constant string:  point arg1 at it, arg2 is zero.
 */

static inline bool
sysfs_streq(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}

	if (*s1 == *s2)
		return true;
	if (!*s1 && *s2 == '\n' && !s2[1])
		return true;
	if (*s1 == '\n' && !s1[1] && !*s2)
		return true;
	return false;
}

#define sysfs_attr_init(attr) do {} while(0)

#endif	/* _LINUX_SYSFS_H_ */
