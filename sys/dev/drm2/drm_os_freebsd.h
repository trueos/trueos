/**
 * \file drm_os_freebsd.h
 * OS abstraction macros.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef _DRM_OS_FREEBSD_H_
#define	_DRM_OS_FREEBSD_H_

#include <sys/fbio.h>
#include <sys/smp.h>

#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>

/*
 * Work around conflicting pci read/write wrappers in linuxkpi.  DRM doesn't
 * use Linux pci_dev at this time.
 */
#define	pci_read_config_byte	linux_pci_read_config_byte
#define	pci_read_config_word	linux_pci_read_config_word
#define	pci_read_config_dword	linux_pci_read_config_dword
#define	pci_write_config_byte	linux_pci_write_config_byte
#define	pci_write_config_word	linux_pci_write_config_word
#define	pci_write_config_dword	linux_pci_write_config_dword
#include <linux/pci.h>
#undef	pci_read_config_byte
#undef	pci_read_config_word
#undef	pci_read_config_dword
#undef	pci_write_config_byte
#undef	pci_write_config_word
#undef	pci_write_config_dword

#define	DRM_IRQ_ARGS		void *arg

#define	BUILD_BUG_ON_NOT_POWER_OF_2(x)

#define	KHZ2PICOS(a)	(1000000000UL/(a))

#define	DRM_HZ			hz
#define	DRM_CURRENTPID		curthread->td_proc->p_pid
#define	DRM_SUSER(p)		(priv_check(p, PRIV_DRIVER) == 0)
#define	DRM_UDELAY(udelay)	DELAY(udelay)
/* Ugly copy/paste from systm.h to work around conflicting Linux macro */
#define	bsd_msleep(chan, mtx, pri, wmesg, timo)				\
	_sleep((chan), &(mtx)->lock_object, (pri), (wmesg),		\
	    tick_sbt * (timo), 0, C_HARDCLOCK)
#define	drm_msleep(x, msg)	pause((msg), ((int64_t)(x)) * hz / 1000)
#define	DRM_MSLEEP(msecs)	drm_msleep((msecs), "drm_msleep")

#define	DRM_READ8(map, offset)						\
	*(volatile u_int8_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset))
#define	DRM_READ16(map, offset)						\
	le16toh(*(volatile u_int16_t *)(((vm_offset_t)(map)->handle) +	\
	    (vm_offset_t)(offset)))
#define	DRM_READ32(map, offset)						\
	le32toh(*(volatile u_int32_t *)(((vm_offset_t)(map)->handle) +	\
	    (vm_offset_t)(offset)))
#define	DRM_READ64(map, offset)						\
	le64toh(*(volatile u_int64_t *)(((vm_offset_t)(map)->handle) +	\
	    (vm_offset_t)(offset)))
#define	DRM_WRITE8(map, offset, val)					\
	*(volatile u_int8_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = val
#define	DRM_WRITE16(map, offset, val)					\
	*(volatile u_int16_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = htole16(val)
#define	DRM_WRITE32(map, offset, val)					\
	*(volatile u_int32_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = htole32(val)
#define	DRM_WRITE64(map, offset, val)					\
	*(volatile u_int64_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = htole64(val)

/* DRM_READMEMORYBARRIER() prevents reordering of reads.
 * DRM_WRITEMEMORYBARRIER() prevents reordering of writes.
 * DRM_MEMORYBARRIER() prevents reordering of reads and writes.
 */
#define	DRM_READMEMORYBARRIER()		rmb()
#define	DRM_WRITEMEMORYBARRIER()	wmb()
#define	DRM_MEMORYBARRIER()		mb()
#define	smp_rmb()			rmb()
#define	smp_wmb()			wmb()
#define	smp_mb__before_atomic_inc()	mb()
#define	smp_mb__after_atomic_inc()	mb()

#define	lower_32_bits(n)	((u32)(n))
#define	upper_32_bits(n)	((u32)(((n) >> 16) >> 16))

#define	VERIFY_READ	VM_PROT_READ
#define	VERIFY_WRITE	VM_PROT_WRITE
#define	access_ok(prot, p, l)	useracc((p), (l), (prot))

/* XXXKIB what is the right code for the FreeBSD ? */
/* kib@ used ENXIO here -- dumbbell@ */
#define	EREMOTEIO	EIO

#define	KTR_DRM		KTR_DEV
#define	KTR_DRM_REG	KTR_SPARE3

#define	DRM_AGP_KERN	struct agp_info
#define	DRM_AGP_MEM	void

#define	hweight32(i)		bitcount32(i)

#define	IS_ALIGNED(x, y)	(((x) & ((y) - 1)) == 0)
#define	get_unaligned(ptr)                                              \
	({ __typeof__(*(ptr)) __tmp;                                    \
	  memcpy(&__tmp, (ptr), sizeof(*(ptr))); __tmp; })

#if _BYTE_ORDER == _LITTLE_ENDIAN
/* Taken from linux/include/linux/unaligned/le_struct.h. */
struct __una_u32 { u32 x; } __packed;

static inline u32
__get_unaligned_cpu32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;

	return (ptr->x);
}

static inline u32
get_unaligned_le32(const void *p)
{

	return (__get_unaligned_cpu32((const u8 *)p));
}
#else
/* Taken from linux/include/linux/unaligned/le_byteshift.h. */
static inline u32
__get_unaligned_le32(const u8 *p)
{

	return (p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24);
}

static inline u32
get_unaligned_le32(const void *p)
{

	return (__get_unaligned_le32((const u8 *)p));
}
#endif

#define	page_to_phys(x) VM_PAGE_TO_PHYS(x)

#define	drm_get_device_from_kdev(_kdev)	(((struct drm_minor *)(_kdev)->si_drv1)->dev)

#define DRM_IOC_VOID		IOC_VOID
#define DRM_IOC_READ		IOC_OUT
#define DRM_IOC_WRITE		IOC_IN
#define DRM_IOC_READWRITE	IOC_INOUT
#define DRM_IOC(dir, group, nr, size) _IOC(dir, group, nr, size)

static inline int
__copy_to_user_inatomic(void __user *to, const void *from, unsigned n)
{

	return (copyout_nofault(from, to, n) != 0 ? n : 0);
}
#define	__copy_to_user_inatomic_nocache(to, from, n) \
    __copy_to_user_inatomic((to), (from), (n))

static inline unsigned long
__copy_from_user_inatomic(void *to, const void __user *from,
    unsigned long n)
{

	/*
	 * XXXKIB.  Equivalent Linux function is implemented using
	 * MOVNTI for aligned moves.  For unaligned head and tail,
	 * normal move is performed.  As such, it is not incorrect, if
	 * only somewhat slower, to use normal copyin.  All uses
	 * except shmem_pwrite_fast() have the destination mapped WC.
	 */
	return ((copyin_nofault(__DECONST(void *, from), to, n) != 0 ? n : 0));
}
#define	__copy_from_user_inatomic_nocache(to, from, n) \
    __copy_from_user_inatomic((to), (from), (n))

static inline int
fault_in_multipages_readable(const char __user *uaddr, int size)
{
	char c;
	int ret = 0;
	const char __user *end = uaddr + size - 1;

	if (unlikely(size == 0))
		return ret;

	while (uaddr <= end) {
		ret = -copyin(uaddr, &c, 1);
		if (ret != 0)
			return -EFAULT;
		uaddr += PAGE_SIZE;
	}

	/* Check whether the range spilled into the next page. */
	if (((unsigned long)uaddr & ~(PAGE_SIZE - 1)) ==
			((unsigned long)end & ~(PAGE_SIZE - 1))) {
		ret = -copyin(end, &c, 1);
	}

	return ret;
}

static inline int
fault_in_multipages_writeable(char __user *uaddr, int size)
{
	int ret = 0;
	char __user *end = uaddr + size - 1;

	if (unlikely(size == 0))
		return ret;

	/*
	 * Writing zeroes into userspace here is OK, because we know that if
	 * the zero gets there, we'll be overwriting it.
	 */
	while (uaddr <= end) {
		ret = subyte(uaddr, 0);
		if (ret != 0)
			return -EFAULT;
		uaddr += PAGE_SIZE;
	}

	/* Check whether the range spilled into the next page. */
	if (((unsigned long)uaddr & ~(PAGE_SIZE - 1)) ==
			((unsigned long)end & ~(PAGE_SIZE - 1)))
		ret = subyte(end, 0);

	return ret;
}

enum __drm_capabilities {
	CAP_SYS_ADMIN
};

static inline bool
capable(enum __drm_capabilities cap)
{

	switch (cap) {
	case CAP_SYS_ADMIN:
		return DRM_SUSER(curthread);
	}
}

#define	to_user_ptr(x)		((void *)(uintptr_t)(x))
#define	sigemptyset(set)	SIGEMPTYSET(set)
#define	sigaddset(set, sig)	SIGADDSET(set, sig)

#define DRM_LOCK(dev)		sx_xlock(&(dev)->dev_struct_lock)
#define DRM_UNLOCK(dev) 	sx_xunlock(&(dev)->dev_struct_lock)

MALLOC_DECLARE(DRM_MEM_DMA);
MALLOC_DECLARE(DRM_MEM_SAREA);
MALLOC_DECLARE(DRM_MEM_DRIVER);
MALLOC_DECLARE(DRM_MEM_FILES);
MALLOC_DECLARE(DRM_MEM_BUFLISTS);
MALLOC_DECLARE(DRM_MEM_AGPLISTS);
MALLOC_DECLARE(DRM_MEM_CTXBITMAP);
MALLOC_DECLARE(DRM_MEM_HASHTAB);
MALLOC_DECLARE(DRM_MEM_KMS);

typedef struct drm_pci_id_list
{
	int vendor;
	int device;
	long driver_private;
	char *name;
} drm_pci_id_list_t;

#ifdef __i386__
#define	CONFIG_X86	1
#endif
#ifdef __amd64__
#define	CONFIG_X86	1
#define	CONFIG_X86_64	1
#endif
#ifdef __ia64__
#define	CONFIG_IA64	1
#endif

#if defined(__i386__) || defined(__amd64__)
#define	CONFIG_ACPI
#define	CONFIG_DRM_I915_KMS
#undef	CONFIG_INTEL_IOMMU
#endif

#ifdef COMPAT_FREEBSD32
#define	CONFIG_COMPAT
#endif

#define	CONFIG_AGP	1
#define	CONFIG_MTRR	1

#define	CONFIG_FB	1
extern const char *fb_mode_option;

#undef	CONFIG_DEBUG_FS
#undef	CONFIG_VGA_CONSOLE

/* I2C compatibility. */
#define	I2C_M_RD	IIC_M_RD
#define	I2C_M_WR	IIC_M_WR
#define	I2C_M_NOSTART	IIC_M_NOSTART

struct fb_info *	framebuffer_alloc(void);
void			framebuffer_release(struct fb_info *info);

#define	console_lock()
#define	console_unlock()
#define	console_trylock()	true

#define	PM_EVENT_SUSPEND	0x0002
#define	PM_EVENT_QUIESCE	0x0008
#define	PM_EVENT_PRETHAW	PM_EVENT_QUIESCE

static inline int
pci_read_config_byte(device_t kdev, int where, u8 *val)
{

	*val = (u8)pci_read_config(kdev, where, 1);
	return (0);
}

static inline int
pci_write_config_byte(device_t kdev, int where, u8 val)
{

	pci_write_config(kdev, where, val, 1);
	return (0);
}

static inline int
pci_read_config_word(device_t kdev, int where, uint16_t *val)
{

	*val = (uint16_t)pci_read_config(kdev, where, 2);
	return (0);
}

static inline int
pci_write_config_word(device_t kdev, int where, uint16_t val)
{

	pci_write_config(kdev, where, val, 2);
	return (0);
}

static inline int
pci_read_config_dword(device_t kdev, int where, uint32_t *val)
{

	*val = (uint32_t)pci_read_config(kdev, where, 4);
	return (0);
}

static inline int
pci_write_config_dword(device_t kdev, int where, uint32_t val)
{

	pci_write_config(kdev, where, val, 4);
	return (0);
}

static inline void
on_each_cpu(void callback(void *data), void *data, int wait)
{

	smp_rendezvous(NULL, callback, NULL, data);
}

void	hex_dump_to_buffer(const void *buf, size_t len, int rowsize,
	    int groupsize, char *linebuf, size_t linebuflen, bool ascii);

#define KIB_NOTYET()							\
do {									\
	if (drm_debug && drm_notyet)					\
		printf("NOTYET: %s at %s:%d\n", __func__, __FILE__, __LINE__); \
} while (0)

#endif /* _DRM_OS_FREEBSD_H_ */
