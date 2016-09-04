/*-
 * Copyright (c) 2006 IronPort Systems
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
#include <sys/pciio.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <net/if.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/swap_pager.h>

#include <machine/bus.h>

#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_util.h>
#include <fs/pseudofs/pseudofs.h>

#include <linux/kobject.h>
#include <linux/device.h>


#define SCSI_HOST_CLASS     0
#define GRAPHICS_CLASS      1
#define IEEE80211_CLASS     2
#define I2C_DEV_CLASS       3
#define I2C_ADAPTER_CLASS   4

#define SYS_CLASS_MAX (I2C_ADAPTER_CLASS+1)
struct pfs_node *classes[SYS_CLASS_MAX];
struct pfs_node *pfs_pci_devices;
struct pfs_node *sysfs_root;
struct pfs_node *pci_root;
extern struct kobject linux_class_root;
extern struct device linux_root_device;

struct scsi_host_queue {
	TAILQ_ENTRY(scsi_host_queue) scsi_host_next;
	char *path;
	char *name;
};

TAILQ_HEAD(,scsi_host_queue) scsi_host_q;

static int host_number = 0;

static int
atoi(const char *str)
{
	return (int)strtol(str, (char **)NULL, 10);
}

/*
 * Filler function for proc_name
 */
static int
linsysfs_scsiname(PFS_FILL_ARGS)
{
	struct scsi_host_queue *scsi_host;
	int index;

	if (strncmp(pn->pn_parent->pn_name, "host", 4) == 0) {
		index = atoi(&pn->pn_parent->pn_name[4]);
	} else {
		sbuf_printf(sb, "unknown\n");
		return (0);
	}
	TAILQ_FOREACH(scsi_host, &scsi_host_q, scsi_host_next) {
		if (index-- == 0) {
			sbuf_printf(sb, "%s\n", scsi_host->name);
			return (0);
		}
	}
	sbuf_printf(sb, "unknown\n");
	return (0);
}

/*
 * Filler function for device sym-link
 */
static int
linsysfs_link_scsi_host(PFS_FILL_ARGS)
{
	struct scsi_host_queue *scsi_host;
	int index;

	if (strncmp(pn->pn_parent->pn_name, "host", 4) == 0) {
		index = atoi(&pn->pn_parent->pn_name[4]);
	} else {
		sbuf_printf(sb, "unknown\n");
		return (0);
	}
	TAILQ_FOREACH(scsi_host, &scsi_host_q, scsi_host_next) {
		if (index-- == 0) {
			sbuf_printf(sb, "../../../devices%s", scsi_host->path);
			return(0);
		}
	}
	sbuf_printf(sb, "unknown\n");
	return (0);
}

static void
linsysfs_create_scsi_class_entry(device_t dev, struct pfs_node *dir, char *host, char *new_path)
{
	struct pfs_node *sub_dir, *scsi;
	struct scsi_host_queue *scsi_host;

	scsi = classes[SCSI_HOST_CLASS];
	sprintf(host, "host%d", host_number++);
	strcat(new_path, "/");
	strcat(new_path, host);
	pfs_create_dir(dir, host, NULL, NULL, NULL, 0);
	scsi_host = malloc(sizeof(struct scsi_host_queue),
			   M_DEVBUF, M_NOWAIT);
	scsi_host->path = malloc(
		strlen(new_path) + 1,
		M_DEVBUF, M_NOWAIT);
	scsi_host->path[0] = '\000';
	bcopy(new_path, scsi_host->path,
	      strlen(new_path) + 1);
	scsi_host->name = "unknown";

	sub_dir = pfs_create_dir(scsi, host, NULL, NULL, NULL, 0);
	pfs_create_link(sub_dir, "device",
			&linsysfs_link_scsi_host,
			NULL, NULL, NULL, 0);
	pfs_create_file(sub_dir, "proc_name", &linsysfs_scsiname,
			NULL, NULL, NULL, PFS_RD);
	scsi_host->name = linux_driver_get_name_dev(dev);
	TAILQ_INSERT_TAIL(&scsi_host_q, scsi_host, scsi_host_next);
}

static int
linsysfs_link_destroy(PFS_DESTROY_ARGS)
{

	free(pn->pn_data, M_TEMP);
	return (0);
}

static int
linsysfs_link_fill(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%s", (char *)pn->pn_data);
	return (0);
}

static void
linsysfs_create_pci_link(char *device, char *new_path)
{
	struct pfs_node *link_node;
	char *link_path;

	link_path = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	sprintf(link_path, "../../../devices%s", new_path);
	link_node = pfs_create_link(pfs_pci_devices, device, linsysfs_link_fill, NULL, NULL, linsysfs_link_destroy, PFS_RD);
	link_node->pn_data = link_path;
}

#define PCI_DEV "pci"
static int
linsysfs_run_bus(device_t dev, struct pfs_node *dir, char *path, char *prefix)
{
	int i, nchildren;
	device_t *children, parent;
	devclass_t devclass;
	const char *name = NULL;
	struct pci_devinfo *dinfo;
	char *device, *host, *new_path = path;

	parent = device_get_parent(dev);
	if (parent) {
		devclass = device_get_devclass(parent);
		if (devclass != NULL)
			name = devclass_get_name(devclass);
		if (name && strcmp(name, PCI_DEV) == 0) {
			dinfo = device_get_ivars(dev);
			if (dinfo) {
				device = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
				new_path = malloc(MAXPATHLEN, M_TEMP,
				    M_WAITOK);
				new_path[0] = '\000';
				strcpy(new_path, path);
				host = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
				device[0] = '\000';
				sprintf(device, "%s:%02x:%02x.%x",
				    prefix,
				    dinfo->cfg.bus,
				    dinfo->cfg.slot,
				    dinfo->cfg.func);
				strcat(new_path, "/");
				strcat(new_path, device);
				dir = pfs_create_dir(dir, device, NULL, NULL, NULL, 0);
				linsysfs_create_pci_link(device, new_path);
				if (dinfo->cfg.baseclass == PCIC_STORAGE) {
					linsysfs_create_scsi_class_entry(dev, dir, host, new_path);
				}
				free(device, M_TEMP);
				free(host, M_TEMP);
			}
		}
	}

	device_get_children(dev, &children, &nchildren);
	for (i = 0; i < nchildren; i++) {
		if (children[i])
			linsysfs_run_bus(children[i], dir, new_path, prefix);
	}
	if (new_path != path)
		free(new_path, M_TEMP);

	return (1);
}

static int
debugfs_attr(PFS_ATTR_ARGS)
{
	vap->va_mode = 0700;
	return (0);
}

static struct pfs_node *classdir;
struct pfs_node *
linsysfs_create_class_dir(struct class *class, const char *name)
{
	struct pfs_node *pn;

	pn = pfs_create_dir(classdir, name, NULL, NULL, NULL, 0);
	pn->pn_data = class;
	return (pn);
}

void
linsysfs_destroy_class_dir(struct class *class)
{
	if (class->sd != NULL) {
		pfs_destroy(class->sd);
		class->sd = NULL;
	}
}

static void
linsysfs_populate_pci(struct pfs_node *root)
{
	struct pfs_node *driversdir, *slotsdir;

	pfs_pci_devices = pfs_create_dir(root, "devices", NULL, NULL, NULL, 0);
	driversdir = pfs_create_dir(root, "drivers", NULL, NULL, NULL, 0);
	slotsdir = pfs_create_dir(root, "slots", NULL, NULL, NULL, 0);

}

static void
linsysfs_populate_udev(struct pfs_node *root)
{
	struct pfs_node *blockdir, *chardir;

	blockdir = pfs_create_dir(root, "block", NULL, NULL, NULL, 0);
	chardir = pfs_create_dir(root, "char", NULL, NULL, NULL, 0);
}

/*
 * Constructor
 */
static int
linsysfs_init(PFS_INIT_ARGS)
{
	struct pfs_node *root;
	struct pfs_node *dir;
	struct pfs_node *pci00;
	devclass_t devclass;
	device_t dev;

	TAILQ_INIT(&scsi_host_q);

	sysfs_root = root = pi->pi_root;

	/* /sys/class/... */
	classdir = dir = pfs_create_dir(root, "class", NULL, NULL, NULL, 0);
	classes[SCSI_HOST_CLASS] = pfs_create_dir(dir, "scsi_host", NULL, NULL, NULL, 0);
	classes[IEEE80211_CLASS] = pfs_create_dir(dir, "ieee80211", NULL, NULL, NULL, 0);
	classes[I2C_DEV_CLASS] = pfs_create_dir(dir, "i2c-dev", NULL, NULL, NULL, 0);
	classes[I2C_ADAPTER_CLASS] = pfs_create_dir(dir, "i2c-adapter", NULL, NULL, NULL, 0);


	/* /sys/kernel/... */
	dir = pfs_create_dir(root, "kernel", NULL, NULL, NULL, 0);
	pfs_create_dir(dir, "debug", debugfs_attr, NULL, NULL, 0);
	
	/* /sys/device */
	dir = pfs_create_dir(root, "devices", NULL, NULL, NULL, 0);

	/* /sys/device/pci0000:00 */
	linux_root_device.kobj.sd = linux_class_root.sd = pci00 = pfs_create_dir(dir, "pci0000:00", NULL, NULL, NULL, 0);

	/* /sys/dev/... */
	dir = pfs_create_dir(root, "dev", NULL, NULL, NULL, 0);
	linsysfs_populate_udev(dir);

	/* /sys/bus/pci/... */
	dir = pfs_create_dir(root, "bus", NULL, NULL, NULL, 0);
	dir = pfs_create_dir(dir, "pci", NULL, NULL, NULL, 0);
	linsysfs_populate_pci(dir);

	devclass = devclass_find("root");
	if (devclass == NULL) {
		return (0);
	}

	dev = devclass_get_device(devclass, 0);
	linsysfs_run_bus(dev, pci00, "/pci0000:00", "0000");
	return (0);
}

/*
 * Destructor
 */
static int
linsysfs_uninit(PFS_INIT_ARGS)
{
	struct scsi_host_queue *scsi_host, *scsi_host_tmp;

	TAILQ_FOREACH_SAFE(scsi_host, &scsi_host_q, scsi_host_next,
	    scsi_host_tmp) {
		TAILQ_REMOVE(&scsi_host_q, scsi_host, scsi_host_next);
		free(scsi_host->path, M_TEMP);
		free(scsi_host, M_TEMP);
	}

	return (0);
}

PSEUDOFS(linsysfs, 1, PR_ALLOW_MOUNT_LINSYSFS);
#if defined(__amd64__)
MODULE_DEPEND(linsysfs, linux_common, 1, 1, 1);
#else
MODULE_DEPEND(linsysfs, linux, 1, 1, 1);
#endif
