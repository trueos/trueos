#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include <dev/drm2/drmP.h>
#include <dev/drm2/drm_crtc.h>
#include <dev/drm2/drm_fb_helper.h>
#include <dev/drm2/drm_crtc_helper.h>

#include <sys/kdb.h>
#include <sys/param.h>
#include <sys/systm.h>

static u16 red2[] __read_mostly = {
    0x0000, 0xaaaa
};
static u16 green2[] __read_mostly = {
    0x0000, 0xaaaa
};
static u16 blue2[] __read_mostly = {
    0x0000, 0xaaaa
};

static u16 red4[] __read_mostly = {
    0x0000, 0xaaaa, 0x5555, 0xffff
};
static u16 green4[] __read_mostly = {
    0x0000, 0xaaaa, 0x5555, 0xffff
};
static u16 blue4[] __read_mostly = {
    0x0000, 0xaaaa, 0x5555, 0xffff
};

static u16 red8[] __read_mostly = {
    0x0000, 0x0000, 0x0000, 0x0000, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa
};
static u16 green8[] __read_mostly = {
    0x0000, 0x0000, 0xaaaa, 0xaaaa, 0x0000, 0x0000, 0x5555, 0xaaaa
};
static u16 blue8[] __read_mostly = {
    0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa
};

static u16 red16[] __read_mostly = {
    0x0000, 0x0000, 0x0000, 0x0000, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa,
    0x5555, 0x5555, 0x5555, 0x5555, 0xffff, 0xffff, 0xffff, 0xffff
};
static u16 green16[] __read_mostly = {
    0x0000, 0x0000, 0xaaaa, 0xaaaa, 0x0000, 0x0000, 0x5555, 0xaaaa,
    0x5555, 0x5555, 0xffff, 0xffff, 0x5555, 0x5555, 0xffff, 0xffff
};
static u16 blue16[] __read_mostly = {
    0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa,
    0x5555, 0xffff, 0x5555, 0xffff, 0x5555, 0xffff, 0x5555, 0xffff
};

static const struct fb_cmap default_2_colors = {
    .len=2, .red=red2, .green=green2, .blue=blue2
};
static const struct fb_cmap default_8_colors = {
    .len=8, .red=red8, .green=green8, .blue=blue8
};
static const struct fb_cmap default_4_colors = {
    .len=4, .red=red4, .green=green4, .blue=blue4
};
static const struct fb_cmap default_16_colors = {
    .len=16, .red=red16, .green=green16, .blue=blue16
};

struct linux_fb_info *
linux_framebuffer_alloc(size_t size, struct device *dev)
{
	return (NULL);
}

void
linux_framebuffer_release(struct linux_fb_info *info)
{
	panic("XXX");
}

int
linux_register_framebuffer(struct linux_fb_info *fb_info)
{

	panic("XXX");
	return (0);
}
int
linux_unregister_framebuffer(struct linux_fb_info *fb_info)
{

	panic("XXX");
	return (0);
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
	panic("XXX");
}

void
cfb_copyarea(struct linux_fb_info *p, const struct fb_copyarea *area)
{

	panic("XXX");
}

void
cfb_imageblit(struct linux_fb_info *p, const struct fb_image *image)
{
	panic("XXX");
}

static const struct fb_cmap *
fb_default_cmap(int len)
{
    if (len <= 2)
	return &default_2_colors;
    if (len <= 4)
	return &default_4_colors;
    if (len <= 8)
	return &default_8_colors;
    return &default_16_colors;
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
	ret = fb_copy_cmap(fb_default_cmap(len), cmap);
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
