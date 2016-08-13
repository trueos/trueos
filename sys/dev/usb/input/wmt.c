/*-
 * Copyright (c) 2014-2015 Vladimir Kondratyev <wulf@cicgroup.ru>
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
 *
 * $FreeBSD$
 */

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/conf.h>
#include <sys/fcntl.h>

#include "usbdevs.h"
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbhid.h>
#include <dev/usb/usb_ioctl.h>

#define	WMT_FIFO_ENABLE	1
#define	USB_DEBUG_VAR wmt_debug
#include <dev/usb/usb_debug.h>

#ifdef USB_DEBUG
static int wmt_debug = 1;

static SYSCTL_NODE(_hw_usb, OID_AUTO, wmt, CTLFLAG_RW, 0,
    "USB MSWindows 7/8 compatible multitouch Pointer Device");
SYSCTL_INT(_hw_usb_wmt, OID_AUTO, debug, CTLFLAG_RWTUN,
    &wmt_debug, 1, "Debug level");
#endif

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/tty.h>

#define	WMT_BSIZE	1024	/* bytes, buffer size */
#define	WMT_FRAME_NUM	50	/* bytes, frame number */

enum {
	WMT_INTR_DT,
	WMT_N_TRANSFER,
};

enum {
	WMT_TIP_SWITCH,
#define	WMT_SLOT	WMT_TIP_SWITCH
	WMT_WIDTH,
#define	WMT_MAJOR	WMT_WIDTH
	WMT_HEIGHT,
#define WMT_MINOR	WMT_HEIGHT
	WMT_TOOL_WIDTH,
#define	WMT_TOOL_MAJOR	WMT_TOOL_WIDTH
	WMT_TOOL_HEIGHT,
#define WMT_TOOL_MINOR	WMT_TOOL_HEIGHT
	WMT_ORIENTATION,
	WMT_X,
	WMT_Y,
	WMT_TOOL_TYPE,
	WMT_BLOB_ID,
	WMT_CONTACT_ID,
	WMT_PRESSURE,
	WMT_CONFIDENCE,
	WMT_TOOL_X,
	WMT_TOOL_Y,
	WMT_N_USAGES,
};

struct wmt_hid_map_item {
	int32_t usage;
	uint32_t code;
};

static const struct wmt_hid_map_item wmt_hid_map[WMT_N_USAGES] = {
	{	/* WMT_TIP_SWITCH, WMT_SLOT */
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_TIP_SWITCH),
		.code = ABS_MT_SLOT
	}, {	/* WMT_WIDTH, WMT_MAJOR */
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_WIDTH),
		.code = ABS_MT_TOUCH_MAJOR
	}, {	/* WMT_HEIGHT, WMT_MINOR */
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_HEIGHT),
		.code = ABS_MT_TOUCH_MINOR
	}, {	/* WMT_TOOL_WIDTH, WMT_TOOL_MAJOR */
		.usage = -1,
		.code = ABS_MT_WIDTH_MAJOR
	}, {	/* WMT_TOOL_HEIGHT, WMT_TOOL_MINOR */
		.usage = -1,
		.code = ABS_MT_WIDTH_MINOR
	}, {	/* WMT_ORIENTATION */
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_HEIGHT),
		.code = ABS_MT_ORIENTATION
	}, {	/* WMT_X */
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
		.code = ABS_MT_POSITION_X
	}, {	/* WMT_Y */
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
		.code = ABS_MT_POSITION_Y
	}, {	/* WMT_TOOL_TYPE */
		.usage = -1,
		.code = ABS_MT_TOOL_TYPE
	}, {	/* WMT_BLOB_ID */
		.usage = -1,
		.code = ABS_MT_BLOB_ID
	}, {	/* WMT_CONTACT_ID */
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACT_IDENTIFIER),
		.code = ABS_MT_TRACKING_ID
	}, {	/* WMT_PRESSURE */
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_TIP_PRESSURE),
		.code = ABS_MT_PRESSURE
	}, {	/* WMT_CONFIDENCE */
		.usage = HID_USAGE2(HUP_DIGITIZERS, HUD_CONFIDENCE),
		.code = ABS_MT_DISTANCE
	}, {	/* WMT_TOOL_X */
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
		.code = ABS_MT_TOOL_X
	}, {	/* WMT_TOOL_Y */
		.usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
		.code = ABS_MT_TOOL_Y
	}
};

#define WMT_ABSINFO_RESET	((struct input_absinfo) { .value = INT32_MIN })
#define WMT_ABSINFO_IS_SET(x)	((x)->value != INT32_MIN)

struct wmt_softc
{
	device_t		dev;
	struct evdev_dev	*evdev;

#ifdef WMT_FIFO_ENABLE
	struct usb_device	*udev;
	struct usb_fifo_sc	fifo;
#endif
	struct mtx		sc_mtx;
	struct usb_xfer		*xfer[WMT_N_TRANSFER];
	uint32_t		isize;
	uint32_t		fsize;
	void			*repdesc_ptr;
	uint16_t		repdesc_size;
	uint8_t			iid;
	uint8_t			fid;

#ifdef WMT_FIFO_ENABLE
	uint8_t			iface_no;
	uint8_t			iface_index;
#endif
	uint32_t		flags;
#define WMT_FLAG_TIP_SWITCH	0x0001
#define	WMT_FLAG_OPENED		0x0002
#define	WMT_FLAG_EV_OPENED	0x0004
#define	WMT_FLAG_RD_STARTED	0x0008

	uint8_t			report_id;
	struct hid_location	ncontacts;
	struct hid_location	hid_items[MAX_MT_SLOTS][WMT_N_USAGES];
	struct input_absinfo	ai[WMT_N_USAGES];

	uint8_t	buf[WMT_BSIZE] __aligned(4);
};

static usb_callback_t wmt_intr_callback;

static device_probe_t	wmt_probe;
static device_attach_t	wmt_attach;
static device_detach_t	wmt_detach;

static evdev_open_t wmt_ev_open;
static evdev_close_t wmt_ev_close;

static uint32_t wmt_hid_test(const void *, uint16_t);
static void wmt_hid_parse(struct wmt_softc *);

#ifdef WMT_FIFO_ENABLE
static usb_fifo_cmd_t	wmt_start_read;
static usb_fifo_cmd_t	wmt_stop_read;
static usb_fifo_open_t	wmt_open;
static usb_fifo_close_t	wmt_close;
static usb_fifo_ioctl_t	wmt_ioctl;

static void wmt_put_queue(struct wmt_softc *, uint8_t *, usb_size_t);

static struct usb_fifo_methods wmt_fifo_methods = {
	.f_open =	&wmt_open,
	.f_close =	&wmt_close,
	.f_ioctl =	&wmt_ioctl,
	.f_start_read =	&wmt_start_read,
	.f_stop_read =	&wmt_stop_read,
	.basename[0] =	"wmt",
};
#endif

static struct evdev_methods wmt_evdev_methods = {
	.ev_open = &wmt_ev_open,
	.ev_close = &wmt_ev_close,
};

static const struct usb_config wmt_config[WMT_N_TRANSFER] = {

	[WMT_INTR_DT] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = { .pipe_bof = 1, .short_xfer_ok = 1 },
		.bufsize = WMT_BSIZE,
		.callback = &wmt_intr_callback,
	},
};

static int
wmt_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	void *d_ptr;
	uint16_t d_len;
	int err;

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->info.bInterfaceClass != UICLASS_HID)
		return (ENXIO);

	err = usbd_req_get_hid_desc(uaa->device, NULL,
	    &d_ptr, &d_len, M_TEMP, uaa->info.bIfaceIndex);

	if (err)
		return (ENXIO);

	if (wmt_hid_test(d_ptr, d_len))
		err = BUS_PROBE_DEFAULT;
	else
		err = ENXIO;

	free(d_ptr, M_TEMP);
	return (err);
}

static int
wmt_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct wmt_softc *sc = device_get_softc(dev);
	size_t i;
	int err;

	device_set_usb_desc(dev);
	sc->dev = dev;
	sc->udev = uaa->device;
	sc->iface_no = uaa->info.bIfaceNum;
	sc->iface_index = uaa->info.bIfaceIndex;

	/* Get HID descriptor */
	if (usbd_req_get_hid_desc(uaa->device, NULL, &sc->repdesc_ptr,
	    &sc->repdesc_size, M_TEMP, uaa->info.bIfaceIndex) !=
	    USB_ERR_NORMAL_COMPLETION)
		return (ENXIO);

	mtx_init(&sc->sc_mtx, "wmt lock", NULL, MTX_DEF | MTX_RECURSE);

	/* Get HID report descriptor length */
	sc->isize = hid_report_size
	    (sc->repdesc_ptr, sc->repdesc_size, hid_input, &sc->iid);
	sc->fsize = hid_report_size
	    (sc->repdesc_ptr, sc->repdesc_size, hid_feature, &sc->fid);

	if (sc->isize <= 0 || sc->isize > WMT_BSIZE) {
		DPRINTF("wmt_attach: input size invalid or too large: %d\n",
		    sc->isize);
		goto detach;
	}
	if (sc->fsize <= 0 || sc->fsize > WMT_BSIZE) {
		DPRINTF("wmt_attach: feature size invalid or too large: %d\n",
		    sc->fsize);
		goto detach;
	}

	err = usbd_req_set_protocol(uaa->device, NULL,
	    uaa->info.bIfaceIndex, 1);

	err = usbd_transfer_setup(uaa->device, &uaa->info.bIfaceIndex,
	    sc->xfer, wmt_config, WMT_N_TRANSFER, sc, &sc->sc_mtx);
	if (err) {
		DPRINTF("usbd_transfer_setup error=%s\n", usbd_errstr(err));
		goto detach;
	}
#ifdef WMT_FIFO_ENABLE
	err = usb_fifo_attach(uaa->device, sc, &sc->sc_mtx,
	    &wmt_fifo_methods, &sc->fifo, device_get_unit(dev), -1,
	    uaa->info.bIfaceIndex, UID_ROOT, GID_OPERATOR, 0644);
	if (err) {
		DPRINTF("usb_fifo_attach error=%s\n", usbd_errstr(err));
		goto detach;
	}
#endif

	wmt_hid_parse(sc);

	sc->evdev = evdev_alloc();

	evdev_set_name(sc->evdev, device_get_desc(dev));
	evdev_set_serial(sc->evdev, "0");
	evdev_set_methods(sc->evdev, sc, &wmt_evdev_methods);
	evdev_support_prop(sc->evdev, INPUT_PROP_DIRECT);
	evdev_support_event(sc->evdev, EV_SYN);
	evdev_support_event(sc->evdev, EV_ABS);
	evdev_set_flag(sc->evdev, EVDEV_FLAG_MT_STCOMPAT);

	/* Report absolute contacts and axes information */
	for (i = 0; i < WMT_N_USAGES; i++)
		if (WMT_ABSINFO_IS_SET(&sc->ai[i]))
			evdev_support_abs(sc->evdev, wmt_hid_map[i].code,
			    (struct input_absinfo *)&sc->ai[i]);

	err = evdev_register(dev, sc->evdev);
	if (err)
		goto detach;

	return (0);

detach:
	if (sc->repdesc_ptr)
		free(sc->repdesc_ptr, M_TEMP);

	wmt_detach(dev);
	return (ENXIO);
}

static int
wmt_detach(device_t dev)
{
	struct wmt_softc *sc = device_get_softc(dev);

	/* Stop intr transfer if running */
	wmt_ev_close(sc->evdev, sc);

	evdev_unregister(dev, sc->evdev);
	evdev_free(sc->evdev);
#ifdef WMT_FIFO_ENABLE
	usb_fifo_detach(&sc->fifo);
#endif
	usbd_transfer_unsetup(sc->xfer, WMT_N_TRANSFER);
	mtx_destroy(&sc->sc_mtx);
	return (0);
}

static void
wmt_process_frame(struct wmt_softc *sc, uint8_t *buf, int len)
{
	int32_t slot_data[WMT_N_USAGES];
	int32_t slot, contact, contacts_count, width, height;
	uint8_t id;
	size_t i;

	if (sc->iid) {
		id = *buf;
		len--;
		buf++;
	} else {
		id = 0;
	}

	if (id != sc->report_id)
		return;

	contacts_count = hid_get_data(buf, len, &sc->ncontacts);

	/* Use protocol Type B for reporting events */
	for (contact = 0; contact < contacts_count; contact++) {

		memset(slot_data, 0, sizeof(slot_data));
		for (i = 0; i < WMT_N_USAGES; i++)
			if (WMT_ABSINFO_IS_SET(&sc->ai[i]))
				slot_data[i] = hid_get_data(buf, len,
				    &sc->hid_items[contact][i]);

		slot = evdev_get_mt_slot_by_tracking_id(sc->evdev,
		    slot_data[WMT_CONTACT_ID]);
		if (slot == -1) {
			printf("Slot overflow for contact_id %d",
			    (int)slot_data[WMT_CONTACT_ID]);
			continue;
		}

		if (slot_data[WMT_TIP_SWITCH] == 0) {
			evdev_push_event(sc->evdev, EV_ABS, ABS_MT_SLOT, slot);
			evdev_push_event(sc->evdev, EV_ABS, ABS_MT_TRACKING_ID,
			    -1);
		} else {
			/* this finger is in proximity of the sensor */
			slot_data[WMT_SLOT] = slot;

			/* divided by two to match visual scale of touch */
			width = slot_data[WMT_WIDTH] >> 1;
			height = slot_data[WMT_HEIGHT] >> 1;
			slot_data[WMT_ORIENTATION] = width > height;
			slot_data[WMT_MAJOR] = width > height ? width : height;
			slot_data[WMT_MINOR] = width < height ? width : height;
			for (i = 0; i < WMT_N_USAGES; i++)
				if (WMT_ABSINFO_IS_SET(&sc->ai[i]))
					evdev_push_event(sc->evdev, EV_ABS,
					    wmt_hid_map[i].code, slot_data[i]);
		}
	}
	evdev_sync(sc->evdev);
}

static void
wmt_intr_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct wmt_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint8_t *buf = sc->buf;
	int len;

	usbd_xfer_status(xfer, &len, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		pc = usbd_xfer_get_frame(xfer, 0);

		if (len >= (int)sc->isize || (len > 0 && sc->iid == 0)) {
			/* limit report length to the maximum */
			if (len > (int)sc->isize)
				len = sc->isize;

			usbd_copy_out(pc, 0, buf, len);
			if (len < sc->isize) {
				/* make sure we don't process old data */
				memset(buf + len, 0, sc->isize - len);
			}

#ifdef WMT_FIFO_ENABLE
			wmt_put_queue(sc, buf, sc->isize);
#endif
			wmt_process_frame(sc, buf, len);
		} else {
			/* ignore it */
			DPRINTF("ignored transfer, %d bytes\n", len);
		}

	case USB_ST_SETUP:
tr_setup:
#if WMT_FIFO_ENABLE
		/* check if we can put more data into the FIFO */
		if (usb_fifo_put_bytes_max(sc->fifo.fp[USB_FIFO_RX]) == 0)
#endif
			if ((sc->flags & WMT_FLAG_EV_OPENED) == 0)
				break;

		usbd_xfer_set_frame_len(xfer, 0, sc->isize);
		usbd_transfer_submit(xfer);
		break;
	default:
		if (error != USB_ERR_CANCELLED) {
			/* try clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

#if WMT_FIFO_ENABLE
static void
wmt_start_read(struct usb_fifo *fifo)
{
	struct wmt_softc *sc = usb_fifo_softc(fifo);

	if (!(sc->flags & WMT_FLAG_EV_OPENED))
		usbd_transfer_start(sc->xfer[WMT_INTR_DT]);
	sc->flags |= WMT_FLAG_RD_STARTED;
}

static void
wmt_stop_read(struct usb_fifo *fifo)
{
	struct wmt_softc *sc = usb_fifo_softc(fifo);

	if (!(sc->flags & WMT_FLAG_EV_OPENED))
		usbd_transfer_stop(sc->xfer[WMT_INTR_DT]);
	sc->flags &= ~WMT_FLAG_RD_STARTED;
}

static void
wmt_put_queue(struct wmt_softc *sc, uint8_t *buf, usb_size_t len)
{

	usb_fifo_put_data_linear(sc->fifo.fp[USB_FIFO_RX], buf, len, 1);
}

static int
wmt_get_report(struct wmt_softc *sc, uint8_t type, uint8_t id,
    void *kern_data, void *user_data, uint16_t len)
{
	int err;
	uint8_t free_data = 0;

	if (kern_data == NULL) {
		kern_data = malloc(len, M_USBDEV, M_WAITOK);
		if (kern_data == NULL) {
			err = ENOMEM;
			goto done;
		}
		free_data = 1;
	}
	err = usbd_req_get_report(sc->udev, NULL, kern_data, len,
	    sc->iface_index, type, id);
	if (err) {
		err = ENXIO;
		goto done;
	}
	if (user_data) {
		/* dummy buffer */
		err = copyout(kern_data, user_data, len);
		if (err)
			goto done;
	}
done:
	if (free_data)
		free(kern_data, M_USBDEV);

	return (err);
}



static int
wmt_open(struct usb_fifo *fifo, int fflags)
{
	/*
	 * The buffers are one byte larger than maximum so that one
	 * can detect too large read/writes and short transfers:
	 */
	if (fflags & FREAD) {
		struct wmt_softc *sc = usb_fifo_softc(fifo);
		if (sc->flags & WMT_FLAG_OPENED)
			return (EBUSY);
		if (usb_fifo_alloc_buffer(fifo,
		    sc->isize + 1, WMT_FRAME_NUM))
			return (ENOMEM);

		sc->flags |= WMT_FLAG_OPENED;
	}
	return (0);
}

static void
wmt_close(struct usb_fifo *fifo, int fflags)
{
	if (fflags & (FREAD)) {
		struct wmt_softc *sc = usb_fifo_softc(fifo);
		sc->flags &= ~(WMT_FLAG_OPENED);
		usb_fifo_free_buffer(fifo);
	}
}

static int
wmt_ioctl(struct usb_fifo *fifo, u_long cmd, void *addr, int fflags)
{
	struct wmt_softc *sc = usb_fifo_softc(fifo);
	struct usb_gen_descriptor *ugd;
	uint32_t size;
	int error = 0;
	uint8_t id;

	switch (cmd) {
	case USB_GET_REPORT_DESC:
		ugd = addr;
		if (sc->repdesc_size > ugd->ugd_maxlen) {
			size = ugd->ugd_maxlen;
		} else {
			size = sc->repdesc_size;
		}
		ugd->ugd_actlen = size;
		if (ugd->ugd_data == NULL)
			break;		/* descriptor length only */
		error = copyout(sc->repdesc_ptr, ugd->ugd_data, size);
		break;

	case USB_SET_IMMED:
		if (!(fflags & FREAD)) {
			error = EPERM;
			break;
		}
		error = EINVAL;
		break;

	case USB_GET_REPORT:
		if (!(fflags & FREAD)) {
			error = EPERM;
			break;
		}
		ugd = addr;
		switch (ugd->ugd_report_type) {
		case UHID_INPUT_REPORT:
			size = sc->isize;
			id = sc->iid;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->fsize;
			id = sc->fid;
			break;
		case UHID_OUTPUT_REPORT:
		default:
			return (EINVAL);
		}
		if (id != 0)
			copyin(ugd->ugd_data, &id, 1);
		error = wmt_get_report(sc, ugd->ugd_report_type, id,
		    sc->buf, ugd->ugd_data, imin(ugd->ugd_maxlen, size));
		break;

	case USB_SET_REPORT:
		if (!(fflags & FWRITE)) {
			error = EPERM;
			break;
		}
		error = EINVAL;
		break;

	case USB_GET_REPORT_ID:
		*(int *)addr = 0;	/* XXX: we only support reportid 0? */
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}
#endif

static void
wmt_ev_close(struct evdev_dev *evdev, void *ev_softc)
{
	struct wmt_softc *sc = (struct wmt_softc *)ev_softc;

	mtx_lock(&sc->sc_mtx);
	if (!(sc->flags & WMT_FLAG_RD_STARTED))
		usbd_transfer_stop(sc->xfer[WMT_INTR_DT]);
	sc->flags &= ~(WMT_FLAG_EV_OPENED);
	mtx_unlock(&sc->sc_mtx);
}

static int
wmt_ev_open(struct evdev_dev *evdev, void *ev_softc)
{
	struct wmt_softc *sc = (struct wmt_softc *)ev_softc;

	mtx_lock(&sc->sc_mtx);
	if (!(sc->flags & WMT_FLAG_RD_STARTED))
		usbd_transfer_start(sc->xfer[WMT_INTR_DT]);
	sc->flags |= WMT_FLAG_EV_OPENED;
	mtx_unlock(&sc->sc_mtx);

	return (0);
}


static uint32_t
wmt_hid_test(const void *d_ptr, uint16_t d_len)
{
	struct hid_data *hd;
	struct hid_item hi;
	int mdepth;
	uint32_t found;

	hd = hid_start_parse(d_ptr, d_len, 1 << hid_feature);
	if (hd == NULL)
		return (0);

	mdepth = 0;
	found = 0;

	while (hid_get_item(hd, &hi)) {
		switch (hi.kind) {
		case hid_collection:
			if (mdepth != 0)
				mdepth++;
			else if (hi.collection == 1 &&
			     hi.usage ==
			      HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHSCREEN))
				mdepth++;
			break;
		case hid_endcollection:
			if (mdepth != 0)
				mdepth--;
			break;
		case hid_feature:
			if (mdepth == 0)
				break;
			if (hi.usage == HID_USAGE2(HUP_DIGITIZERS, HUD_CONTACT_COUNT_MAX) &&
			    (hi.flags & (HIO_CONST|HIO_VARIABLE|HIO_RELATIVE)) == HIO_VARIABLE)
				found = hi.logical_maximum;
			break;
		default:
			break;
		}
	}
	hid_end_parse(hd);
	return (found);
}

static void
hid_item_to_absinfo(struct hid_item *hi, struct input_absinfo *ia)
{
	/*
	 * hid unit scaling table according to HID Usage Table Review
	 * Request 39 Tbl 17 http://www.usb.org/developers/hidpage/HUTRR39b.pdf
	 * Modified to do cm to 1/mm conversion.
	 */
	static const int64_t scale[][2] = {
	    { 2, 20 },		/* 0x00 */
	    { 2, 200 },		/* 0x01 */
	    { 2, 2000 },	/* 0x02 */
	    { 2, 20000 },	/* 0x03 */
	    { 2, 200000 },	/* 0x04 */
	    { 2, 2000000 },	/* 0x05 */
	    { 2, 20000000 },	/* 0x06 */
	    { 2, 200000000 },	/* 0x07 */
	    { 10000000, 1 },	/* 0x08 */
	    { 1000000, 1 },	/* 0x09 */
	    { 100000, 1 },	/* 0x0A */
	    { 10000, 1 },	/* 0x0B */
	    { 1000, 1 },	/* 0x0C */
	    { 100, 1 },		/* 0x0D */
	    { 10, 1 },		/* 0x0E */
	    { 2, 2 },		/* 0x0F */
	};
	int64_t logical_size, physical_size, resolution;

	if (WMT_ABSINFO_IS_SET(ia))
		return;

	memset(ia, 0, sizeof(*ia));
	ia->maximum = hi->logical_maximum;
	ia->minimum = hi->logical_minimum;
#define HIUU_CENTIMETER 0x11
	if (hi->unit == HIUU_CENTIMETER &&
	    hi->logical_maximum > hi->logical_minimum &&
	    hi->physical_maximum > hi->physical_minimum) {
		logical_size = (int64_t)hi->logical_maximum -
		    (int64_t)hi->logical_minimum + 1;
		physical_size = (int64_t)hi->physical_maximum -
		    (int64_t)hi->physical_minimum + 1;

		if (hi->unit_exponent >= 0 &&
		    hi->unit_exponent < nitems(scale)) {
			resolution = ((scale[hi->unit_exponent][0] / 2) +
			    logical_size * scale[hi->unit_exponent][0]) /
			    (physical_size * scale[hi->unit_exponent][1]);
			if (resolution <= INT32_MAX)
				ia->resolution = resolution;
		}
	}
}

static void
wmt_hid_parse(struct wmt_softc *sc)
{
	struct hid_data *hd;
	struct hid_item hi;
	int mdepth, touch_coll, finger_coll;
	size_t i;
	int32_t finger_idx;

	finger_idx = mdepth = touch_coll = finger_coll = 0;

	hd = hid_start_parse
	    (sc->repdesc_ptr, sc->repdesc_size, 1 << hid_input);
	if (hd == NULL)
		return;

	for (i = 0; i < WMT_N_USAGES; i++)
		sc->ai[i] = WMT_ABSINFO_RESET;

	while (hid_get_item(hd, &hi)) {
		switch (hi.kind) {
		case hid_collection:
			if (hi.collection == 1 && mdepth == 0 &&
			    hi.usage ==
			      HID_USAGE2(HUP_DIGITIZERS, HUD_TOUCHSCREEN))
				touch_coll = 1;
			if (hi.collection == 2 && mdepth == 1 &&
			    hi.usage == HID_USAGE2(HUP_DIGITIZERS, HUD_FINGER)) {
				finger_coll = 1;
				sc->report_id = hi.report_ID;
			}
			mdepth++;
			break;
		case hid_endcollection:
			if (mdepth != 0)
				mdepth--;
			if (mdepth == 1 && finger_coll == 1) {
				finger_coll = 0;
				++finger_idx;
			}
			if (mdepth == 0 && touch_coll == 1)
				touch_coll = 0;
			break;
		case hid_input:
			if ((hi.flags & (HIO_CONST|HIO_VARIABLE|HIO_RELATIVE))
			    != HIO_VARIABLE)
				break;
			if (touch_coll == 1 && mdepth == 1) {
				sc->ncontacts = hi.loc;
				break;
			}

			if (finger_coll == 0 || mdepth != 2)
				break;
			if (finger_idx >= MAX_MT_SLOTS) {
				DPRINTF("Finger %d ignored\n", finger_idx);
				break;
			}

			if (hi.usage ==
			    HID_USAGE2(HUP_DIGITIZERS, HUD_TIP_SWITCH))
				sc->flags |= WMT_FLAG_TIP_SWITCH;

			for (i = 0; i < WMT_N_USAGES; i++) {
				if (hi.usage == wmt_hid_map[i].usage &&
				    sc->hid_items[finger_idx][i].size == 0) {
					sc->hid_items[finger_idx][i] = hi.loc;
					hid_item_to_absinfo(&hi, &sc->ai[i]);
					break;
				}
			}
			break;
		default:
			break;
		}
	}
	hid_end_parse(hd);

	sc->ai[WMT_SLOT] =
	    (struct input_absinfo) { .maximum = finger_idx - 1 };
	if (WMT_ABSINFO_IS_SET(&sc->ai[WMT_WIDTH]) &&
	    WMT_ABSINFO_IS_SET(&sc->ai[WMT_HEIGHT]))
		sc->ai[WMT_ORIENTATION] =
		    (struct input_absinfo) { .maximum = 1 };

	/* Announce information about the pointer device */
	device_printf(sc->dev,
	    "%d contacts and [%s%s%s%s]. Report range [%d:%d] - [%d:%d]\n",
	    (int)finger_idx,
	    WMT_ABSINFO_IS_SET(&sc->ai[WMT_CONFIDENCE]) ? "C" : "",
	    WMT_ABSINFO_IS_SET(&sc->ai[WMT_WIDTH]) ? "W" : "",
	    WMT_ABSINFO_IS_SET(&sc->ai[WMT_HEIGHT]) ? "H" : "",
	    WMT_ABSINFO_IS_SET(&sc->ai[WMT_PRESSURE]) ? "P" : "",
	    (int)sc->ai[WMT_X].minimum, (int)sc->ai[WMT_Y].minimum,
	    (int)sc->ai[WMT_X].maximum, (int)sc->ai[WMT_Y].maximum);
}

static devclass_t wmt_devclass;

static device_method_t wmt_methods[] = {
	DEVMETHOD(device_probe, wmt_probe),
	DEVMETHOD(device_attach, wmt_attach),
	DEVMETHOD(device_detach, wmt_detach),

	DEVMETHOD_END
};

static driver_t wmt_driver = {
	.name = "wmt",
	.methods = wmt_methods,
	.size = sizeof(struct wmt_softc),
};

DRIVER_MODULE(wmt, uhub, wmt_driver, wmt_devclass, NULL, 0);
MODULE_DEPEND(wmt, usb, 1, 1, 1);
MODULE_VERSION(wmt, 1);
