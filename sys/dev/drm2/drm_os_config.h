#ifndef DRM_OS_CONFIG_H_
#define DRM_OS_CONFIG_H_ 

#define CONFIG_DEBUG_FS
#ifdef notyet
#define CONFIG_COMPAT COMPAT_FREEBSD32
#endif
#ifdef notyet
#define CONFIG_MMU_NOTIFIER 1
#endif
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
#define CONFIG_PCI
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

#undef	CONFIG_VGA_CONSOLE

#endif
