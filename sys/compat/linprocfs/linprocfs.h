#ifndef LINPROCFS_H_
#define LINPROCFS_H_

struct lp_fill_entry {
	char *fe_name;
	pfs_fill_t fe_fill;
	int fe_flags;
};

void linprocfs_vm_init(struct pfs_node *root);

#endif
