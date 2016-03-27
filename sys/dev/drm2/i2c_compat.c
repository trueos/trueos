#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/sglist.h>
#include <sys/stat.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/filio.h>
#include <sys/rwlock.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <sys/signalvar.h>
#include <sys/pciio.h>
#include <sys/poll.h>
#include <sys/sbuf.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>
#include <machine/bus.h>
#include <machine/resource.h>
#if defined(__i386__) || defined(__amd64__)
#include <machine/specialreg.h>
#endif
#include <machine/sysarch.h>
#include <sys/endian.h>
#include <sys/mman.h>
#include <sys/rman.h>
#include <sys/memrange.h>
#include <dev/agp/agpvar.h>
#include <sys/agpio.h>
#include <sys/mutex.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <sys/selinfo.h>
#include <sys/bus.h>

#include <compat/linuxkpi/common/include/linux/idr.h>

/*
 * CEM: drm.h brings in drm_os_freebsd.h, which brings in linuxkpi list.h.
 * drm_gem_names and drm_hashtab use FreeBSD queue(9) macros.  Include them
 * first so they get the BSD macros.
 *
 * We probably can (and should) drop drm_hashtab.h entirely and diff-reduce
 * drm_hashtab.c to use the Linux list implementation.
 */

#include <dev/drm2/drm_hashtab.h>

#include <dev/drm2/drm.h>
#include <dev/drm2/drm_sarea.h>

#include <linux/device.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

#include <dev/drm2/i2c_compat.h>

int
i2c_add_adapter(struct i2c_adapter *adapter)
{
	panic("IMPLEMENT ME!!!");
	return (EINVAL);
}
