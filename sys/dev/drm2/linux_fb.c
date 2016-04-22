#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include <dev/drm2/drmP.h>
#include <dev/drm2/drm_crtc.h>
#include <dev/drm2/drm_fb_helper.h>
#include <dev/drm2/drm_crtc_helper.h>

#include <sys/kdb.h>
#include <sys/param.h>
#include <sys/systm.h>

#define FB_MAJOR		29   /* /dev/fb* framebuffers */
#define FBPIXMAPSIZE	(1024 * 8)


static struct sx linux_fb_mtx;
SX_SYSINIT(linux_fb_mtx, &linux_fb_mtx, "linux fb");

static struct linux_fb_info *registered_fb[FB_MAX];
static int num_registered_fb;
static struct class *fb_class;

static int __unregister_framebuffer(struct linux_fb_info *fb_info);

static int
fb_init(void)
{

	fb_class = class_create(THIS_MODULE, "graphics");

	return (fb_class == NULL ? ENOMEM : 0);
}
SYSINIT(fb_init, SI_SUB_KLD, SI_ORDER_MIDDLE, fb_init, NULL);


struct linux_fb_info *
linux_framebuffer_alloc(size_t size, struct device *dev)
{
#define BYTES_PER_LONG (BITS_PER_LONG/8)
#define PADDING (BYTES_PER_LONG - (sizeof(struct linux_fb_info) % BYTES_PER_LONG))
	int fb_info_size = sizeof(struct linux_fb_info);
	struct linux_fb_info *info;
	char *p;

	if (size)
		fb_info_size += PADDING;

	p = kzalloc(fb_info_size + size, GFP_KERNEL);

	if (!p)
		return NULL;

	info = (struct linux_fb_info *) p;

	if (size)
		info->par = p + fb_info_size;

	info->device = dev;

	return info;
#undef PADDING
#undef BYTES_PER_LONG
}


void
linux_framebuffer_release(struct linux_fb_info *info)
{
	if (info == NULL)
		return;
	kfree(info->apertures);
	kfree(info);
}

static void
put_fb_info(struct linux_fb_info *fb_info)
{
	if (!atomic_dec_and_test(&fb_info->count))
		return;
	if (fb_info->fbops->fb_destroy)
		fb_info->fbops->fb_destroy(fb_info);
}

static bool
apertures_overlap(struct aperture *gen, struct aperture *hw)
{
	/* is the generic aperture base the same as the HW one */
	if (gen->base == hw->base)
		return (true);
	/* is the generic aperture base inside the hw base->hw base+size */
	if (gen->base > hw->base && gen->base < hw->base + hw->size)
		return (true);
	return (false);
}

static bool
check_overlap(struct apertures_struct *a, struct apertures_struct *b)
{
	int i, j;

	if (a == NULL || b == NULL)
		return (false);

	for (i = 0; i < b->count; ++i)
		for (j = 0; j < a->count; ++j) 
			if (apertures_overlap(&a->ranges[j], &b->ranges[i]))
				return (true);
	return (false);
}



#define VGA_FB_PHYS 0xA0000
static void
__remove_conflicting(struct apertures_struct *a, const char *name, bool primary)
{
	struct apertures_struct *gen_aper;
	int i;

	for (i = 0 ; i < FB_MAX; i++) {

		if (registered_fb[i] == NULL)
			continue;

		if ((registered_fb[i]->flags & FBINFO_MISC_FIRMWARE) == 0)
			continue;

		gen_aper = registered_fb[i]->apertures;
		if (check_overlap(gen_aper, a) ||
			(primary && gen_aper && gen_aper->count &&
			 gen_aper->ranges[0].base == VGA_FB_PHYS)) {
			__unregister_framebuffer(registered_fb[i]);
		}
	}
}

void
linux_remove_conflicting_framebuffers(struct apertures_struct *a,
				const char *name, bool primary)
{
	sx_xlock(&linux_fb_mtx);
	__remove_conflicting(a, name, primary);
	sx_xunlock(&linux_fb_mtx);
}

static int
is_primary(struct linux_fb_info *info)
{
	struct device *device = info->device;
	struct pci_dev *pci_dev = NULL;
	struct linux_resource *res = NULL;

	if (device &&  (pci_dev = to_pci_dev(device)) == NULL)
		return (0);

	res = &pci_dev->resource[PCI_ROM_RESOURCE];
	if (res && res->flags & IORESOURCE_ROM_SHADOW)
		return (1);

	return (0);
}


static int
fb_mode_is_equal(const struct fb_videomode *mode1,
		     const struct fb_videomode *mode2)
{
	return (mode1->xres         == mode2->xres &&
		mode1->yres         == mode2->yres &&
		mode1->pixclock     == mode2->pixclock &&
		mode1->hsync_len    == mode2->hsync_len &&
		mode1->vsync_len    == mode2->vsync_len &&
		mode1->left_margin  == mode2->left_margin &&
		mode1->right_margin == mode2->right_margin &&
		mode1->upper_margin == mode2->upper_margin &&
		mode1->lower_margin == mode2->lower_margin &&
		mode1->sync         == mode2->sync &&
		mode1->vmode        == mode2->vmode);
}
static void
fb_destroy_modes(struct list_head *head)
{
	struct list_head *pos, *n;

	list_for_each_safe(pos, n, head) {
		list_del(pos);
		kfree(pos);
	}
}


static int
fb_add_videomode(const struct fb_videomode *mode, struct list_head *head)
{
	struct list_head *pos;
	struct fb_modelist *modelist;
	struct fb_videomode *m;
	int found = 0;

	list_for_each(pos, head) {
		modelist = list_entry(pos, struct fb_modelist, list);
		m = &modelist->mode;
		if (fb_mode_is_equal(m, mode)) {
			found = 1;
			break;
		}
	}
	if (!found) {
		modelist = kmalloc(sizeof(struct fb_modelist),
						  GFP_KERNEL);

		if (!modelist)
			return -ENOMEM;
		modelist->mode = *mode;
		list_add(&modelist->list, head);
	}
	return 0;
}

static void
fb_var_to_videomode(struct fb_videomode *mode,
			 const struct fb_var_screeninfo *var)
{
	u32 pixclock, hfreq, htotal, vtotal;

	mode->name = NULL;
	mode->xres = var->xres;
	mode->yres = var->yres;
	mode->pixclock = var->pixclock;
	mode->hsync_len = var->hsync_len;
	mode->vsync_len = var->vsync_len;
	mode->left_margin = var->left_margin;
	mode->right_margin = var->right_margin;
	mode->upper_margin = var->upper_margin;
	mode->lower_margin = var->lower_margin;
	mode->sync = var->sync;
	mode->vmode = var->vmode & FB_VMODE_MASK;
	mode->flag = FB_MODE_IS_FROM_VAR;
	mode->refresh = 0;

	if (!var->pixclock)
		return;

	pixclock = PICOS2KHZ(var->pixclock) * 1000;

	htotal = var->xres + var->right_margin + var->hsync_len +
		var->left_margin;
	vtotal = var->yres + var->lower_margin + var->vsync_len +
		var->upper_margin;

	if (var->vmode & FB_VMODE_INTERLACED)
		vtotal /= 2;
	if (var->vmode & FB_VMODE_DOUBLE)
		vtotal *= 2;

	hfreq = pixclock/htotal;
	mode->refresh = hfreq/vtotal;
}

static int
__register_framebuffer(struct linux_fb_info *fb_info)
{
	int i;
	struct fb_event event;
	struct fb_videomode mode;

	__remove_conflicting(fb_info->apertures, fb_info->fix.id, is_primary(fb_info));

	if (num_registered_fb == FB_MAX)
		return -ENXIO;

	num_registered_fb++;
	for (i = 0 ; i < FB_MAX; i++)
		if (!registered_fb[i])
			break;
	fb_info->node = i;
	atomic_set(&fb_info->count, 1);
	mutex_init(&fb_info->lock);
	mutex_init(&fb_info->mm_lock);

	fb_info->dev = device_create(fb_class, fb_info->device,
				     MKDEV(FB_MAJOR, i), NULL, "fb%d", i);
	if (IS_ERR(fb_info->dev)) {
		/* Not fatal */
		printk(KERN_WARNING "Unable to create device for framebuffer %d; errno = %ld\n", i, PTR_ERR(fb_info->dev));
		fb_info->dev = NULL;
	} else
		dev_set_drvdata(fb_info->dev, fb_info);

	if (fb_info->pixmap.addr == NULL) {
		fb_info->pixmap.addr = kmalloc(FBPIXMAPSIZE, GFP_KERNEL);
		if (fb_info->pixmap.addr) {
			fb_info->pixmap.size = FBPIXMAPSIZE;
			fb_info->pixmap.buf_align = 1;
			fb_info->pixmap.scan_align = 1;
			fb_info->pixmap.access_align = 32;
			fb_info->pixmap.flags = FB_PIXMAP_DEFAULT;
		}
	}	
	fb_info->pixmap.offset = 0;

	if (!fb_info->pixmap.blit_x)
		fb_info->pixmap.blit_x = ~(u32)0;

	if (!fb_info->pixmap.blit_y)
		fb_info->pixmap.blit_y = ~(u32)0;

	if (!fb_info->modelist.prev || !fb_info->modelist.next)
		INIT_LIST_HEAD(&fb_info->modelist);

	fb_var_to_videomode(&mode, &fb_info->var);
	fb_add_videomode(&mode, &fb_info->modelist);
	registered_fb[i] = fb_info;

	event.info = fb_info;
#if 0	
	if (!lock_fb_info(fb_info))
		return -ENODEV;
	console_lock();
	fb_notifier_call_chain(FB_EVENT_FB_REGISTERED, &event);
	console_unlock();
	unlock_fb_info(fb_info);
#endif	
	return 0;
}

int
linux_register_framebuffer(struct linux_fb_info *fb_info)
{
	int rc;

	sx_xlock(&linux_fb_mtx);
	rc = __register_framebuffer(fb_info);
	sx_xunlock(&linux_fb_mtx);
	return (rc);
}

int
unlink_framebuffer(struct linux_fb_info *fb_info)
{
	int i;

	i = fb_info->node;
	if (i < 0 || i >= FB_MAX || registered_fb[i] != fb_info)
		return -EINVAL;

	if (fb_info->dev) {
		device_destroy(fb_class, MKDEV(FB_MAJOR, i));
		fb_info->dev = NULL;
	}
	return 0;
}

static int
__unregister_framebuffer(struct linux_fb_info *fb_info)
{
	struct fb_event event;
	int i, ret = 0;

	i = fb_info->node;
	if (i < 0 || i >= FB_MAX || registered_fb[i] != fb_info)
		return -EINVAL;

#if 0	
	if (!lock_fb_info(fb_info))
		return -ENODEV;
	console_lock();
	event.info = fb_info;
	ret = fb_notifier_call_chain(FB_EVENT_FB_UNBIND, &event);
	console_unlock();
	unlock_fb_info(fb_info);

	if (ret)
		return -EINVAL;
#endif
	
	unlink_framebuffer(fb_info);
	if (fb_info->pixmap.addr &&
	    (fb_info->pixmap.flags & FB_PIXMAP_DEFAULT))
		kfree(fb_info->pixmap.addr);
	fb_destroy_modes(&fb_info->modelist);
	registered_fb[i] = NULL;
	num_registered_fb--;
	event.info = fb_info;
#if 0	
	console_lock();
	fb_notifier_call_chain(FB_EVENT_FB_UNREGISTERED, &event);
	console_unlock();
#endif
	/* this may free fb info */
	put_fb_info(fb_info);
	return 0;
}

int
linux_unregister_framebuffer(struct linux_fb_info *fb_info)
{
	int rc;

	sx_xlock(&linux_fb_mtx);
	rc = __unregister_framebuffer(fb_info);
	sx_xunlock(&linux_fb_mtx);
	return (rc);
}

void
fb_set_suspend(struct linux_fb_info *info, int state)
{
#if 0	
	struct fb_event event;

	event.info = info;
	if (state) {
		fb_notifier_call_chain(FB_EVENT_SUSPEND, &event);
		info->state = FBINFO_STATE_SUSPENDED;
	} else {
		info->state = FBINFO_STATE_RUNNING;
		fb_notifier_call_chain(FB_EVENT_RESUME, &event);
	}
#endif	
}


void
cfb_fillrect(struct linux_fb_info *p, const struct fb_fillrect *rect)
{
	tainted_cfb_fillrect(p, rect);
}

void
cfb_copyarea(struct linux_fb_info *p, const struct fb_copyarea *area)
{

	tainted_cfb_copyarea(p, area);
}

void
cfb_imageblit(struct linux_fb_info *p, const struct fb_image *image)
{
	tainted_cfb_imageblit(p, image);
}

static int
fb_copy_cmap(const struct fb_cmap *from, struct fb_cmap *to)
{
	int tooff = 0, fromoff = 0;
	int size;

	if (to->start > from->start)
		fromoff = to->start - from->start;
	else
		tooff = from->start - to->start;
	size = to->len - tooff;
	if (size > (int) (from->len - fromoff))
		size = from->len - fromoff;
	if (size <= 0)
		return -EINVAL;
	size *= sizeof(u16);

	memcpy(to->red+tooff, from->red+fromoff, size);
	memcpy(to->green+tooff, from->green+fromoff, size);
	memcpy(to->blue+tooff, from->blue+fromoff, size);
	if (from->transp && to->transp)
		memcpy(to->transp+tooff, from->transp+fromoff, size);
	return 0;
}

static int
fb_alloc_cmap_gfp(struct fb_cmap *cmap, int len, int transp, gfp_t flags)
{
	int size = len * sizeof(u16);
	int ret = -ENOMEM;

	if (cmap->len != len) {
		fb_dealloc_cmap(cmap);
		if (!len)
			return 0;

		cmap->red = kmalloc(size, flags);
		if (!cmap->red)
			goto fail;
		cmap->green = kmalloc(size, flags);
		if (!cmap->green)
			goto fail;
		cmap->blue = kmalloc(size, flags);
		if (!cmap->blue)
			goto fail;
		if (transp) {
			cmap->transp = kmalloc(size, flags);
			if (!cmap->transp)
				goto fail;
		} else {
			cmap->transp = NULL;
		}
	}
	cmap->start = 0;
	cmap->len = len;
	ret = fb_copy_cmap(tainted_fb_default_cmap(len), cmap);
	if (ret)
		goto fail;
	return 0;

fail:
	fb_dealloc_cmap(cmap);
	return ret;
}

int
fb_alloc_cmap(struct fb_cmap *cmap, int len, int transp)
{
	return fb_alloc_cmap_gfp(cmap, len, transp, GFP_ATOMIC);
}

void
fb_dealloc_cmap(struct fb_cmap *cmap)
{
	kfree(cmap->red);
	kfree(cmap->green);
	kfree(cmap->blue);
	kfree(cmap->transp);

	cmap->red = cmap->green = cmap->blue = cmap->transp = NULL;
	cmap->len = 0;
}
