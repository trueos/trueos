/*-
 * Copyright (c) 2014 Jakub Wojciech Klama <jceel@FreeBSD.org>
 * Copyright (c) 2015-2016 Vladimir Kondratyev <wulf@cicgroup.ru>
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/malloc.h>

#include <dev/evdev/input.h>
#include <dev/evdev/uinput.h>
#include <dev/evdev/evdev.h>

#undef	DEBUG
#ifdef DEBUG
#define	debugf(fmt, args...)	printf("evdev: " fmt "\n", ##args);
#else
#define	debugf(fmt, args...)
#endif

#define	UINPUT_BUFFER_SIZE	16

#define	UINPUT_LOCKQ(state)		mtx_lock(&(state)->ucs_mtx)
#define	UINPUT_UNLOCKQ(state)		mtx_unlock(&(state)->ucs_mtx)
#define	UINPUT_LOCKQ_ASSERT(state)	mtx_assert(&(state)->ucs_mtx, MA_OWNED)
#define UINPUT_EMPTYQ(state) \
    ((state)->ucs_buffer_head == (state)->ucs_buffer_tail)

enum uinput_state
{
	UINPUT_NEW = 0,
	UINPUT_CONFIGURED,
	UINPUT_RUNNING
};

static evdev_event_t	uinput_ev_event;

static d_open_t		uinput_open;
static d_read_t		uinput_read;
static d_write_t	uinput_write;
static d_ioctl_t	uinput_ioctl;
static d_poll_t		uinput_poll;
static d_kqfilter_t	uinput_kqfilter;
static void uinput_dtor(void *);

static int uinput_kqread(struct knote *kn, long hint);
static void uinput_kqdetach(struct knote *kn);

static struct cdevsw uinput_cdevsw = {
	.d_version = D_VERSION,
	.d_open = uinput_open,
	.d_read = uinput_read,
	.d_write = uinput_write,
	.d_ioctl = uinput_ioctl,
	.d_poll = uinput_poll,
	.d_kqfilter = uinput_kqfilter,
	.d_name = "uinput",
};

static struct evdev_methods uinput_ev_methods = {
	.ev_open = NULL,
	.ev_close = NULL,
	.ev_event = uinput_ev_event,
};

static struct filterops uinput_filterops = {
	.f_isfd = 1,
	.f_attach = NULL,
	.f_detach = uinput_kqdetach,
	.f_event = uinput_kqread,
};

struct uinput_cdev_state
{
	enum uinput_state	ucs_state;
	struct evdev_dev *	ucs_evdev;
	struct mtx		ucs_mtx;
	size_t			ucs_buffer_head;
	size_t			ucs_buffer_tail;
	struct selinfo		ucs_selp;
	bool			ucs_blocked;
	bool			ucs_selected;
	struct input_event      ucs_buffer[UINPUT_BUFFER_SIZE];
};

static void uinput_enqueue_event(struct uinput_cdev_state *, uint16_t,
    uint16_t, int32_t);
static int uinput_setup_provider(struct uinput_cdev_state *,
    struct uinput_user_dev *);
static int uinput_cdev_create(void);
static void uinput_notify(struct uinput_cdev_state *);

static void
uinput_ev_event(struct evdev_dev *evdev, void *softc, uint16_t type,
    uint16_t code, int32_t value)
{
	struct uinput_cdev_state *state = softc;

	if (type == EV_LED)
		evdev_push_event(evdev, type, code, value);

	UINPUT_LOCKQ(state);
	uinput_enqueue_event(state, type, code, value);
	uinput_notify(state);
	UINPUT_UNLOCKQ(state);
}

static void
uinput_enqueue_event(struct uinput_cdev_state *state, uint16_t type,
    uint16_t code, int32_t value)
{
	size_t head, tail;

	UINPUT_LOCKQ_ASSERT(state);

	head = state->ucs_buffer_head;
	tail = (state->ucs_buffer_tail + 1) % UINPUT_BUFFER_SIZE;

	microtime(&state->ucs_buffer[tail].time);
	state->ucs_buffer[tail].type = type;
	state->ucs_buffer[tail].code = code;
	state->ucs_buffer[tail].value = value;
	state->ucs_buffer_tail = tail;

	/* If queue is full remove oldest event */
	if (tail == head) {
		debugf("state %p: buffer overflow", state);

		head = (head + 1) % UINPUT_BUFFER_SIZE;
		state->ucs_buffer_head = head;
	}
}

static int
uinput_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct uinput_cdev_state *state;

	state = malloc(sizeof(struct uinput_cdev_state), M_EVDEV,
	    M_WAITOK | M_ZERO);
	state->ucs_evdev = evdev_alloc();

	mtx_init(&state->ucs_mtx, "uinput", NULL, MTX_DEF);
	knlist_init_mtx(&state->ucs_selp.si_note, &state->ucs_mtx);

	devfs_set_cdevpriv(state, uinput_dtor);
	return (0);
}

static void
uinput_dtor(void *data)
{
	struct uinput_cdev_state *state = (struct uinput_cdev_state *)data;

	if (state->ucs_state == UINPUT_RUNNING)
		evdev_unregister(NULL, state->ucs_evdev);

	evdev_free(state->ucs_evdev);

	knlist_clear(&state->ucs_selp.si_note, 0);
	seldrain(&state->ucs_selp);
	knlist_destroy(&state->ucs_selp.si_note);
	mtx_destroy(&state->ucs_mtx);
	free(data, M_EVDEV);
}

static int
uinput_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct uinput_cdev_state *state;
	struct input_event *event;
	int remaining, ret;

	debugf("uinput: read %zd bytes by thread %d", uio->uio_resid,
	    uio->uio_td->td_tid);

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	/* Zero-sized reads are allowed for error checking */
	if (uio->uio_resid != 0 && uio->uio_resid < sizeof(struct input_event))
		return (EINVAL);

	remaining = uio->uio_resid / sizeof(struct input_event);

	UINPUT_LOCKQ(state);

	if (UINPUT_EMPTYQ(state)) {
		if (ioflag & O_NONBLOCK)
			ret = EWOULDBLOCK;
		else {
			if (remaining != 0) {
				state->ucs_blocked = true;
				ret = mtx_sleep(state, &state->ucs_mtx,
				    PCATCH, "uiread", 0);
			}
		}
	}

	while (ret == 0 && !UINPUT_EMPTYQ(state) && remaining > 0) {
		event = &state->ucs_buffer[state->ucs_buffer_head];
		state->ucs_buffer_head = (state->ucs_buffer_head + 1) %
		    UINPUT_BUFFER_SIZE;
		remaining--;

		UINPUT_UNLOCKQ(state);
		ret = uiomove(event, sizeof(struct input_event), uio);
		UINPUT_LOCKQ(state);
	}

	UINPUT_UNLOCKQ(state);

	return (ret);
}

static int
uinput_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct uinput_cdev_state *state;
	struct uinput_user_dev userdev;
	struct input_event event;
	int ret = 0;

	debugf("uinput: write %zd bytes by thread %d", uio->uio_resid,
	    uio->uio_td->td_tid);

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	if (state->ucs_state != UINPUT_RUNNING) {
		/* Process written struct uinput_user_dev */
		if (uio->uio_resid != sizeof(struct uinput_user_dev)) {
			debugf("write size not multiple of struct uinput_user_dev size");
			return (EINVAL);
		}

		uiomove(&userdev, sizeof(struct uinput_user_dev), uio);
		uinput_setup_provider(state, &userdev);
	} else {
		/* Process written event */
		if (uio->uio_resid % sizeof(struct input_event) != 0) {
			debugf("write size not multiple of struct input_event size");
			return (EINVAL);
		}

		while (uio->uio_resid > 0) {
			uiomove(&event, sizeof(struct input_event), uio);
			ret = evdev_inject_event(state->ucs_evdev, event.type,
			    event.code, event.value);

			if (ret != 0)
				return (ret);
		}
	}

	return (0);
}

static int
uinput_setup_dev(struct uinput_cdev_state *state, struct input_id *id,
    char *name, uint32_t ff_effects_max)
{

	if (name[0] == 0)
		return (EINVAL);

	evdev_set_name(state->ucs_evdev, name);
	memcpy(&state->ucs_evdev->ev_id, id, sizeof(struct input_id));
	state->ucs_state = UINPUT_CONFIGURED;

	return (0);
}

static int
uinput_setup_provider(struct uinput_cdev_state *state,
    struct uinput_user_dev *udev)
{
	struct input_absinfo absinfo;
	int i, ret;

	debugf("uinput: setup_provider called, udev=%p", udev);

	ret = uinput_setup_dev(state, &udev->id, udev->name,
	    udev->ff_effects_max);
	if (ret)
		return (ret);

	bzero(&absinfo, sizeof(struct input_absinfo));
	for (i = 0; i < ABS_CNT; i++) {
		if (!bit_test(state->ucs_evdev->ev_abs_flags, i))
			continue;

		absinfo.minimum = udev->absmin[i];
		absinfo.maximum = udev->absmax[i];
		absinfo.fuzz = udev->absfuzz[i];
		absinfo.flat = udev->absflat[i];
		evdev_set_absinfo(state->ucs_evdev, i, &absinfo);
	}

	return (0);
}

static int
uinput_poll(struct cdev *dev, int events, struct thread *td)
{
	struct uinput_cdev_state *state;
	int revents = 0;

	debugf("uinput: poll by thread %d", td->td_tid);

	if (devfs_get_cdevpriv((void **)&state) != 0)
		return (POLLNVAL);

	/* Always allow write */
	if (events & (POLLOUT | POLLWRNORM))
		revents |= (events & (POLLOUT | POLLWRNORM));

	if (events & (POLLIN | POLLRDNORM)) {
		UINPUT_LOCKQ(state);
		if (!UINPUT_EMPTYQ(state))
			revents = events & (POLLIN | POLLRDNORM);
		else {
			state->ucs_selected = true;
			selrecord(td, &state->ucs_selp);
		}
		UINPUT_UNLOCKQ(state);
	}

	return (revents);
}

static int
uinput_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct uinput_cdev_state *state;
	int ret;

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	switch(kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &uinput_filterops;
		break;
	default:
		return(EINVAL);
	}
	kn->kn_hook = (caddr_t)state;

	knlist_add(&state->ucs_selp.si_note, kn, 0);
	return (0);
}

static int
uinput_kqread(struct knote *kn, long hint)
{
	struct uinput_cdev_state *state;
	int ret;

	state = (struct uinput_cdev_state *)kn->kn_hook;

	UINPUT_LOCKQ_ASSERT(state);

	ret = !UINPUT_EMPTYQ(state);
	return (ret);
}

static void
uinput_kqdetach(struct knote *kn)
{
	struct uinput_cdev_state *state;

	state = (struct uinput_cdev_state *)kn->kn_hook;
	knlist_remove(&state->ucs_selp.si_note, kn, 0);
}

static void
uinput_notify(struct uinput_cdev_state *state)
{

	UINPUT_LOCKQ_ASSERT(state);

	if (state->ucs_blocked) {
		state->ucs_blocked = false;
		wakeup(state);
	}
	if (state->ucs_selected) {
		state->ucs_selected = false;
		selwakeup(&state->ucs_selp);
	}
	KNOTE_LOCKED(&state->ucs_selp.si_note, 0);
}

static int
uinput_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct uinput_cdev_state *state;
	struct uinput_setup *us;
	struct uinput_abs_setup *uabs;
	int ret, len;
	char buf[NAMELEN];

	len = IOCPARM_LEN(cmd);

	debugf("uinput: ioctl called: cmd=0x%08lx, data=%p", cmd, data);

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	switch (IOCBASECMD(cmd)) {
	case UI_GET_SYSNAME(0):
		if (state->ucs_state != UINPUT_RUNNING)
			return (ENOENT);
		if (len == 0)
			return (EINVAL);
		snprintf(data, len, "event%d", state->ucs_evdev->ev_unit);
		return (0);
	}

	switch (cmd) {
	case UI_DEV_CREATE:
		if (state->ucs_state != UINPUT_CONFIGURED)
			return (EINVAL);

		evdev_set_methods(state->ucs_evdev, state, &uinput_ev_methods);
		evdev_set_flag(state->ucs_evdev, EVDEV_FLAG_SOFTREPEAT);
		evdev_register(NULL, state->ucs_evdev);
		state->ucs_state = UINPUT_RUNNING;
		return (0);

	case UI_DEV_DESTROY:
		if (state->ucs_state != UINPUT_RUNNING)
			return (0);

		evdev_unregister(NULL, state->ucs_evdev);
		state->ucs_state = UINPUT_NEW;
		return (0);

	case UI_DEV_SETUP:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);

		us = (struct uinput_setup *)data;
		return (uinput_setup_dev(state, &us->id, us->name,
		    us->ff_effects_max));

	case UI_ABS_SETUP:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);

		uabs = (struct uinput_abs_setup *)data;
		return (evdev_support_abs(state->ucs_evdev, uabs->code,
		    &uabs->absinfo));

	case UI_SET_EVBIT:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);

		return (evdev_support_event(state->ucs_evdev, *(int *)data));

	case UI_SET_KEYBIT:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		return (evdev_support_key(state->ucs_evdev, *(int *)data));

	case UI_SET_RELBIT:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		return (evdev_support_rel(state->ucs_evdev, *(int *)data));

	case UI_SET_ABSBIT:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		return (evdev_set_abs_bit(state->ucs_evdev, *(int *)data));

	case UI_SET_MSCBIT:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		return (evdev_support_msc(state->ucs_evdev, *(int *)data));

	case UI_SET_LEDBIT:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		return (evdev_support_led(state->ucs_evdev, *(int *)data));

	case UI_SET_SNDBIT:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		return (evdev_support_snd(state->ucs_evdev, *(int *)data));

	case UI_SET_FFBIT:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		/* Fake unsupported ioctl */
		return (0);

	case UI_SET_PHYS:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		ret = copyinstr(*(void **)data, buf, sizeof(buf), NULL);
		/* Linux returns EINVAL when string does not fit the buffer */
		if (ret == ENAMETOOLONG)
			ret = EINVAL;
		if (ret != 0)
			return (ret);
		evdev_set_phys(state->ucs_evdev, buf);
		return (0);

	case UI_SET_SWBIT:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		return (evdev_support_sw(state->ucs_evdev, *(int *)data));

	case UI_SET_PROPBIT:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		return (evdev_support_prop(state->ucs_evdev, *(int *)data));

	case UI_BEGIN_FF_UPLOAD:
	case UI_END_FF_UPLOAD:
	case UI_BEGIN_FF_ERASE:
	case UI_END_FF_ERASE:
		if (state->ucs_state == UINPUT_RUNNING)
			return (EINVAL);
		/* Fake unsupported ioctl */
		return (0);

	case UI_GET_VERSION:
		*(unsigned int *)data = UINPUT_VERSION;
		return (0);
	}

	return (EINVAL);
}

static int
uinput_cdev_create(void)
{
	struct make_dev_args mda;
	struct cdev *cdev;

	make_dev_args_init(&mda);
	mda.mda_devsw = &uinput_cdevsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_WHEEL;
	mda.mda_mode = 0600;

	make_dev_s(&mda, &cdev, "uinput");

	return (0);
}

SYSINIT(uinput, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, uinput_cdev_create, NULL);
