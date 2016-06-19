#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <linux/device.h>
#include <linux/acpi.h>
#include <drm/drmP.h>

#include <drm/drm_crtc_helper.h>

MODULE_DEPEND(i915kms, drmn, 1, 1, 1);
MODULE_DEPEND(i915kms, agp, 1, 1, 1);
MODULE_DEPEND(i915kms, linuxkpi, 1, 1, 1);
MODULE_DEPEND(i915kms, firmware, 1, 1, 1);
