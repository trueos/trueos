
#include <linux/kobject.h>
#include <linux/sysfs.h>


static int
sysctl_handle_attr(SYSCTL_HANDLER_ARGS)
{
	struct kobject *kobj;
	struct attribute *attr;
	const struct sysfs_ops *ops;
	char *buf;
	int error;
	ssize_t len;

	kobj = arg1;
	attr = (struct attribute *)(intptr_t)arg2;
	if (kobj->ktype == NULL || kobj->ktype->sysfs_ops == NULL)
		return (ENODEV);
	buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (buf == NULL)
		return (ENOMEM);
	ops = kobj->ktype->sysfs_ops;
	if (ops->show) {
		len = ops->show(kobj, attr, buf);
		/*
		 * It's valid to not have a 'show' so just return an
		 * empty string.
	 	 */
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
	}

	/* Leave one trailing byte to append a newline. */
	error = sysctl_handle_string(oidp, buf, PAGE_SIZE - 1, req);
	if (error != 0 || req->newptr == NULL || ops->store == NULL)
		goto out;
	len = strlcat(buf, "\n", PAGE_SIZE);
	KASSERT(len < PAGE_SIZE, ("new attribute truncated"));
	len = ops->store(kobj, attr, buf, len);
	if (len < 0)
		error = -len;
out:
	free_page((unsigned long)buf);

	return (error);
}

int
lkpi_sysfs_create_file(struct kobject *kobj, const struct attribute *attr)
{

	sysctl_add_oid(NULL, SYSCTL_CHILDREN(kobj->oidp), OID_AUTO,
	    attr->name, CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_MPSAFE, kobj,
		       (uintptr_t)attr, sysctl_handle_attr, "A", "", NULL);

	return (0);
}

void
lkpi_sysfs_remove_file(struct kobject *kobj, const struct attribute *attr)
{
	int refcount;

	refcount = 0;
	if (kobj->oidp) {
		refcount = kobj->oidp->oid_refcnt;
		sysctl_remove_name(kobj->oidp, attr->name, 1, 1);
	}
	if (refcount == 1)
		kobj->oidp = NULL;
}

void
lkpi_sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp)
{
	int refcount;

	refcount = 0;
	if (kobj->oidp) {
		refcount = kobj->oidp->oid_refcnt;
		sysctl_remove_name(kobj->oidp, grp->name, 1, 1);
	}
	if (refcount == 1)
		kobj->oidp = NULL;
}

int
lkpi_sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp)
{
	struct attribute **attr;
	struct sysctl_oid *oidp;

	oidp = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(kobj->oidp),
	    OID_AUTO, grp->name, CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, grp->name);
	for (attr = grp->attrs; *attr != NULL; attr++) {
		sysctl_add_oid(NULL, SYSCTL_CHILDREN(oidp), OID_AUTO,
		    (*attr)->name, CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_MPSAFE,
			       kobj, (uintptr_t)*attr, sysctl_handle_attr, "A", "", NULL);
	}

	return (0);
}

int
lkpi_sysfs_create_dir(struct kobject *kobj)
{

	kobj->oidp = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(kobj->parent->oidp),
	    OID_AUTO, kobj->name, CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, kobj->name);

        return (0);
}

void
lkpi_sysfs_remove_dir(struct kobject *kobj)
{
	int refcount;

	refcount = 0;
	if (kobj->oidp != NULL) {
		refcount = kobj->oidp->oid_refcnt;
		sysctl_remove_oid(kobj->oidp, 1, 1);
	}
	if (refcount == 1)
		kobj->oidp = NULL;
}
