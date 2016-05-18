#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <linux/device.h>
#include <linux/acpi.h>
#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include "i915_drv.h"
#include "i915_trace.h"
#include "intel_drv.h"

#include <linux/apple-gmux.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/vgaarb.h>
#include <linux/vga_switcheroo.h>
#include <drm/drm_crtc_helper.h>


#define WARN_UN() log(LOG_WARNING, "%s unimplemented", __FUNCTION__)

bool
intel_enable_gtt(void)
{
	WARN_UN();
	return (false);
}

int
intel_gmch_probe(struct pci_dev *bridge_pdev, struct pci_dev *gpu_pdev,
		 struct agp_bridge_data *bridge)
{
	WARN_UN();
	return (0);
}

void
intel_gmch_remove(void)
{
	WARN_UN();
}

void
i915_locks_destroy(struct drm_i915_private *dev_priv)
{
	spin_lock_destroy(&dev_priv->irq_lock);
	spin_lock_destroy(&dev_priv->gpu_error.lock);
	mutex_destroy(&dev_priv->backlight_lock);
	spin_lock_destroy(&dev_priv->uncore.lock);
	spin_lock_destroy(&dev_priv->mm.object_stat_lock);
	spin_lock_destroy(&dev_priv->mmio_flip_lock);
	mutex_destroy(&dev_priv->sb_lock);
	mutex_destroy(&dev_priv->modeset_restore_lock);
	mutex_destroy(&dev_priv->av_mutex);
}


MODULE_DEPEND(i915kms, drmn, 1, 1, 1);
MODULE_DEPEND(i915kms, agp, 1, 1, 1);
MODULE_DEPEND(i915kms, linuxkpi, 1, 1, 1);
MODULE_DEPEND(i915kms, firmware, 1, 1, 1);
