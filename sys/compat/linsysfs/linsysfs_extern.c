/*-
 * Copyright (c) 2016 Matthew Macy
 * All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/blist.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/bus.h>

#include <net/if.h>

#include <fs/pseudofs/pseudofs.h>

#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>


extern struct pfs_node *sysfs_root;
MALLOC_DEFINE(M_SFSINT, "sysfsint", "Linux sysfs internal");

struct sysfsent {
	const struct attribute *s_attr;
	bool s_is_bin;
};

static char *
pfs_find_path(struct pfs_node *pnsource, struct pfs_node *targetpn)
{
	struct pfs_node *pni, *pnj;
	int depth, i;
	char *path, *new_path;

	path = malloc(MAXPATHLEN, M_SFSINT, M_WAITOK|M_ZERO);
	new_path = malloc(MAXPATHLEN, M_SFSINT, M_WAITOK|M_ZERO);
	sprintf(path, "%s", targetpn->pn_name);
	for (depth = 0, pni = pnsource; pni->pn_parent != sysfs_root; pni = pni->pn_parent, depth++)
		for (pnj = targetpn; pnj->pn_parent != sysfs_root; pnj = pnj->pn_parent) {
			if (pnj == pni)
				goto done;
		}
done:
	/*
	 * depth is the the number of .. entries needed
	 * pni and pnj point to the directory from which
	 * we need to descend to reach the target
	 */
	for (pni = targetpn; pni != pnj; pni = pni->pn_parent) {
		if (pni == targetpn) {
			sprintf(new_path, "%s", targetpn->pn_name);
			strcpy(path, new_path);
		} else {
			sprintf(new_path, "%s/%s", targetpn->pn_name, path);
			strcpy(path, new_path);
		}
	}
	for (i = 0; i < depth; i++) {
		sprintf(new_path, "../%s", path);
		strcpy(path, new_path);
	}
	free(new_path, M_SFSINT);
	return (path);
}

static void
pfs_remove_by_name(struct pfs_node *parent, const char *name)
{
	struct pfs_node *pn;

	pn = pfs_find_node(parent, name);
	if (pn)
		pfs_destroy(pn);
}
static struct pfs_node	*
pfs_find_and_get_node(struct pfs_node *parent, const char *name)
{
	return (pfs_find_node(parent, name));
}

static void
pfs_put(struct pfs_node *pn)
{
	/* NO-OP */
}

static int
sysfs_file_attr(PFS_ATTR_ARGS)
{
	struct sysfsent *se = pn->pn_data;

	vap->va_mode = se->s_attr->mode;
	return (0);
}

static int
sysfs_link_fill(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%s", (char *)pn->pn_data);
	return (0);
}

static int
sysfs_read(const struct sysfs_ops *ops, struct kobject *kobj,
	   const struct attribute *pattr, struct sbuf *sb)
{
	char *buf;
	int len, error;
	struct attribute *attr;

	attr = __DECONST(struct attribute *, pattr);
	buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (buf == NULL)
		return (ENOMEM);

	error = 0;
	len = ops->show(kobj, attr, buf);

	if (len < 0) {
		error = -len;
		if (error != EIO)
			goto out;
		buf[0] = '\0';
	} else if (len) {
		len--;
		if (len >= PAGE_SIZE)
			len = PAGE_SIZE - 1;
		/* Trim trailing newline. */
		buf[len] = '\0';
	}
	sbuf_cat(sb, buf);
out:
	free_page((unsigned long)buf);

	return (error);
}

static int
sysfs_write(const struct sysfs_ops *ops, struct kobject *kobj,
	    const struct attribute *pattr, struct sbuf *sb)
{
	const char *pp;
	int len, error;
	struct attribute *attr;

	attr = __DECONST(struct attribute *, pattr);
	sbuf_finish(sb);
	pp = sbuf_data(sb);
	len = sbuf_len(sb);
	error = 0;
	
	len = ops->store(kobj, attr, pp, len);
	if (len < 0)
		error = -len;
	return (error);
}


static int
sysfs_file_fill(PFS_FILL_ARGS)
{
	struct sysfsent *se = pn->pn_data;
	int rc;

	rc = 0;
	if (!se->s_is_bin) {
		struct kobject *kobj = pn->pn_parent->pn_data;
		const struct sysfs_ops *sysfs_ops = kobj->ktype->sysfs_ops;
		if (uio->uio_rw == UIO_READ) {
			rc = sysfs_read(sysfs_ops, kobj, se->s_attr, sb);
		} else {
			rc = sysfs_write(sysfs_ops, kobj, se->s_attr, sb);	
		}
	} else {
#if 0		
		struct bin_attribute *battr = (void *)se->s_attr;
#endif		

		if (uio->uio_rw == UIO_READ) {
			
		} else {

		}
	}
	return (rc);
}

static int
sysfs_file_destroy(PFS_DESTROY_ARGS)
{
	free(pn->pn_data, M_SFSINT);
	return (0);
}
	
static int
sysfs_add_file(struct pfs_node *parent, const struct attribute *attr,
    bool is_bin)
{
	struct sysfsent *se;
	struct pfs_node *pn;
	int flags;

	se = malloc(sizeof(*se), M_SFSINT, M_WAITOK);
	se->s_attr = attr;
	se->s_is_bin = is_bin;
	flags = 0;

	if (!is_bin) {
		struct kobject *pkobj = parent->pn_data;
		const struct sysfs_ops *sysfs_ops = pkobj->ktype->sysfs_ops;

		if (sysfs_ops->show)
			flags |= PFS_RD;
		if (sysfs_ops->store)
			flags |= PFS_WR;
	} else {
		struct bin_attribute *battr =
			 __DECONST(struct bin_attribute *, attr);

		if (battr->read)
			flags |= PFS_RD;
		if (battr->write)
			flags |= PFS_WR;
	}

	pn = pfs_create_file(parent, attr->name, sysfs_file_fill, sysfs_file_attr, NULL, sysfs_file_destroy, flags);
	if (pn == NULL)
		return (-ENOMEM);
	pn->pn_data = se;
	return (0);
}

int
sysfs_create_bin_file(struct kobject *kobj, const struct bin_attribute *attr)
{

	return (sysfs_add_file(kobj->sd, &attr->attr, true));
}

void
sysfs_remove_bin_file(struct kobject *kobj, const struct bin_attribute *attr)
{
	struct pfs_node *pn;

	pn = pfs_find_node(kobj->sd, attr->attr.name);
	if (pn)
		pfs_destroy(pn);
}

int
sysfs_create_file(struct kobject *kobj, const struct attribute *attr)
{
	lkpi_sysfs_create_file(kobj, attr);

	return (sysfs_add_file(kobj->sd, attr, false));
}

int
sysfs_create_files(struct kobject *kobj, const struct attribute **ptr)
{
	int rc = 0;
	int i;

	for (i = 0; ptr[i] && rc == 0; i++)
		rc = sysfs_create_file(kobj, ptr[i]);
	if (rc)
		while (--i >= 0)
			sysfs_remove_file(kobj, ptr[i]);
	return (rc);
}

void
sysfs_remove_file(struct kobject *kobj, const struct attribute *attr)
{
	struct pfs_node *pn;

	lkpi_sysfs_remove_file(kobj, attr);

	pn = pfs_find_node(kobj->sd, attr->name);
	if (pn)
		pfs_destroy(pn);
}

void
sysfs_remove_files(struct kobject *kobj, const struct attribute **ptr)
{
	int i;
	for (i = 0; ptr[i]; i++)
		sysfs_remove_file(kobj, ptr[i]);
}

int
sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp)
{
	struct attribute **attr;
	struct pfs_node *pn;

	lkpi_sysfs_create_group(kobj, grp);

	if (grp->name) {
		pn = pfs_create_dir(kobj->sd, grp->name, NULL, NULL, NULL, 0);
		pn->pn_data = kobj;
	} else
		pn = kobj->sd;

	/* XXX check failure handling :-\ */
	for (attr = grp->attrs; *attr != NULL; attr++)
		sysfs_add_file(pn, *attr, 0);
	return (0);
}

void
sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp)
{
	struct pfs_node *pn;
	struct attribute **attr;

	lkpi_sysfs_remove_group(kobj, grp);
	for (attr = grp->attrs; *attr != NULL; attr++) {
		pn = pfs_find_node(kobj->sd, (*attr)->name);
		if (pn)
			pfs_destroy(pn);
	}
}

/*
 * Temporary hack to warkaround kobj->sd not having been set yet
 *
 */
static void
kobj_fixup(struct kobject *kobj)
{
	if (kobj->sd == NULL)
		sysfs_create_dir_ns(kobj, NULL);
}

int
sysfs_merge_group(struct kobject *kobj,
		  const struct attribute_group *grp)
{
	struct pfs_node *parent;
	int rc = 0;
	struct attribute *const *attr;
	int i;

	kobj_fixup(kobj);
	parent = pfs_find_and_get_node(kobj->sd, grp->name);
	if (!parent)
		return (-ENOENT);

	for ((i = 0, attr = grp->attrs); *attr && rc == 0; (++i, ++attr))
		rc = sysfs_add_file(parent, *attr, false);
	if (rc) {
		while (--i >= 0)
			pfs_remove_by_name(parent, (*--attr)->name);
	}
	pfs_put(parent);
	return (rc);
}

void
sysfs_unmerge_group(struct kobject *kobj,
		       const struct attribute_group *grp)
{
	struct pfs_node *parent;
	struct attribute *const *attr;

	parent = pfs_find_and_get_node(kobj->sd, grp->name);
	if (parent) {
		for (attr = grp->attrs; *attr; ++attr)
			pfs_remove_by_name(parent, (*attr)->name);
		pfs_put(parent);
	}
}

int
sysfs_create_dir_ns(struct kobject *kobj, const void *ns)
{
	struct pfs_node *parent, *pn;

	lkpi_sysfs_create_dir(kobj);
	if (kobj->parent)
		parent = kobj->parent->sd;
	else
		parent = sysfs_root;

	if (!parent)
		return (-ENOENT);
	pn = pfs_create_dir(parent, kobject_name(kobj), NULL, NULL, NULL, 0);
	pn->pn_data = kobj;
	kobj->sd = pn;
	return (0);
}

void
sysfs_remove_dir(struct kobject *kobj)
{
	struct pfs_node *pn = kobj->sd;

	lkpi_sysfs_remove_dir(kobj);
	if (pn) {
		pfs_destroy(pn);
		kobj->sd = NULL;
	}
}

int
sysfs_create_link(struct kobject *kobj, struct kobject *target, const char *name)
{
	char *link_path;
	struct pfs_node *pn;

	link_path = pfs_find_path(kobj->sd, target->sd);
	pn = pfs_create_link(kobj->sd, name, sysfs_link_fill, NULL, NULL, sysfs_file_destroy, 0);
	pn->pn_data = link_path;
	return (0);
}

void
sysfs_remove_link(struct kobject *kobj, const char *name)
{
	struct pfs_node *pn;
	
	pn = pfs_find_node(kobj->sd, name);
	if (pn)
		pfs_destroy(pn);
}

const char power_group_name[] = "power";
