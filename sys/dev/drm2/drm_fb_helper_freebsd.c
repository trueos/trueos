#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm_crtc.h>
#include <dev/drm2/drm_fb_helper.h>
#include <dev/drm2/drm_crtc_helper.h>

#include <sys/kdb.h>
#include <sys/param.h>
#include <sys/systm.h>

struct vt_kms_softc {
	struct drm_fb_helper	*fb_helper;
	struct task		 fb_mode_task;
};


/* Call restore out of vt(9) locks. */
static void
vt_restore_fbdev_mode(void *arg, int pending)
{
	struct drm_fb_helper *fb_helper;
	struct vt_kms_softc *sc;

	sc = (struct vt_kms_softc *)arg;
	fb_helper = sc->fb_helper;
	mutex_lock(&fb_helper->dev->mode_config.mutex);
	drm_fb_helper_restore_fbdev_mode(fb_helper);
	mutex_unlock(&fb_helper->dev->mode_config.mutex);
}

static int
vt_kms_postswitch(void *arg)
{
	struct vt_kms_softc *sc;

	sc = (struct vt_kms_softc *)arg;

	if (!kdb_active && panicstr == NULL)
		taskqueue_enqueue(taskqueue_thread, &sc->fb_mode_task);
	else
		drm_fb_helper_restore_fbdev_mode(sc->fb_helper);

	return (0);
}

struct fb_info *
framebuffer_alloc(size_t size, struct device *dev)
{
	struct fb_info *info;
	struct vt_kms_softc *sc;

	info = malloc(sizeof(*info), DRM_MEM_KMS, M_WAITOK | M_ZERO);

	sc = malloc(sizeof(*sc), DRM_MEM_KMS, M_WAITOK | M_ZERO);
	TASK_INIT(&sc->fb_mode_task, 0, vt_restore_fbdev_mode, sc);

	info->fb_priv = sc;
	info->enter = &vt_kms_postswitch;

	return (info);
}

void
framebuffer_release(struct fb_info *info)
{

	free(info->fb_priv, DRM_MEM_KMS);
	free(info, DRM_MEM_KMS);
}

int
fb_get_options(const char *connector_name, char **option)
{
	char tunable[64];

	/*
	 * A user may use loader tunables to set a specific mode for the
	 * console. Tunables are read in the following order:
	 *     1. kern.vt.fb.modes.$connector_name
	 *     2. kern.vt.fb.default_mode
	 *
	 * Example of a mode specific to the LVDS connector:
	 *     kern.vt.fb.modes.LVDS="1024x768"
	 *
	 * Example of a mode applied to all connectors not having a
	 * connector-specific mode:
	 *     kern.vt.fb.default_mode="640x480"
	 */
	snprintf(tunable, sizeof(tunable), "kern.vt.fb.modes.%s",
	    connector_name);
	DRM_INFO("Connector %s: get mode from tunables:\n", connector_name);
	DRM_INFO("  - %s\n", tunable);
	DRM_INFO("  - kern.vt.fb.default_mode\n");
	*option = kern_getenv(tunable);
	if (*option == NULL)
		*option = kern_getenv("kern.vt.fb.default_mode");

	return (*option != NULL ? 0 : -ENOENT);
}
