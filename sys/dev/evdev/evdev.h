/*-
 * Copyright (c) 2014 Jakub Wojciech Klama <jceel@FreeBSD.org>
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

#ifndef	_DEV_EVDEV_EVDEV_H
#define	_DEV_EVDEV_EVDEV_H

#include <sys/bitstring.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kbio.h>
#include <sys/selinfo.h>
#include <dev/evdev/input.h>
#include <dev/kbd/kbdreg.h>

#define	NAMELEN		80

/*
 * bitstr_t implementation must be identical to one found in EVIOCG*
 * libevdev ioctls. Our bitstring(3) API is compatible since r299090.
 */
_Static_assert(sizeof(bitstr_t) == sizeof(unsigned long),
    "bitstr_t size mismatch");

MALLOC_DECLARE(M_EVDEV);

struct evdev_dev;
struct evdev_client;
struct evdev_mt;

typedef int (evdev_open_t)(struct evdev_dev *, void *);
typedef void (evdev_close_t)(struct evdev_dev *, void *);
typedef void (evdev_event_t)(struct evdev_dev *, void *, uint16_t,
    uint16_t, int32_t);
typedef void (evdev_keycode_t)(struct evdev_dev *, void *,
    struct input_keymap_entry *);
typedef void (evdev_client_event_t)(struct evdev_client *, void *);

#define	ABS_MT_FIRST	ABS_MT_TOUCH_MAJOR
#define	ABS_MT_LAST	ABS_MT_TOOL_Y
#define	ABS_IS_MT(x)	((x) >= ABS_MT_FIRST && (x) <= ABS_MT_LAST)
#define	ABS_MT_INDEX(x)	((x) - ABS_MT_FIRST)
#define	MT_CNT		(ABS_MT_INDEX(ABS_MT_LAST) + 1)
/* Multitouch protocol type A */
#define	MAX_MT_REPORTS	5
/* Multitouch protocol type B interface */
#define	MAX_MT_SLOTS	16
#define	CURRENT_MT_SLOT(evdev)	((evdev)->ev_absinfo[ABS_MT_SLOT].value)
#define	MAXIMAL_MT_SLOT(evdev)	((evdev)->ev_absinfo[ABS_MT_SLOT].maximum)

#define	EVDEV_FLAG_SOFTREPEAT	0x00	/* use evdev to repeat keys */
#define	EVDEV_FLAG_MT_STCOMPAT	0x01	/* autogenerate ST-compatible events
					 * for MT protocol type B reports */
#define	EVDEV_FLAG_MAX		0x1F
#define	EVDEV_FLAG_CNT		(EVDEV_FLAG_MAX + 1)

enum evdev_key_events
{
	KEY_EVENT_UP,
	KEY_EVENT_DOWN,
	KEY_EVENT_REPEAT
};

/* evdev clock IDs in Linux semantic */
enum evdev_clock_id
{
	EV_CLOCK_REALTIME = 0,	/* UTC clock */
	EV_CLOCK_MONOTONIC,	/* monotonic, stops on suspend */
	EV_CLOCK_BOOTTIME	/* monotonic, suspend-awared */
};

#define	PS2_KEYBOARD_VENDOR		1
#define	PS2_KEYBOARD_PRODUCT		1
#define	PS2_MOUSE_VENDOR		2
#define	PS2_MOUSE_GENERIC_PRODUCT	1

struct evdev_methods
{
	evdev_open_t		*ev_open;
	evdev_close_t		*ev_close;
	evdev_event_t		*ev_event;
	evdev_keycode_t		*ev_get_keycode;
	evdev_keycode_t		*ev_set_keycode;
};

struct evdev_dev
{
	char			ev_name[NAMELEN];
	char			ev_shortname[NAMELEN];
	char			ev_serial[NAMELEN];
	device_t		ev_dev;
	struct cdev *		ev_cdev;
	int			ev_unit;
	struct mtx		ev_mtx;
	struct input_id		ev_id;
	struct evdev_client *	ev_grabber;
	size_t			ev_report_size;

	/* Supported features: */
	bitstr_t		bit_decl(ev_prop_flags, INPUT_PROP_CNT);
	bitstr_t		bit_decl(ev_type_flags, EV_CNT);
	bitstr_t		bit_decl(ev_key_flags, KEY_CNT);
	bitstr_t		bit_decl(ev_rel_flags, REL_CNT);
	bitstr_t		bit_decl(ev_abs_flags, ABS_CNT);
	bitstr_t		bit_decl(ev_msc_flags, MSC_CNT);
	bitstr_t		bit_decl(ev_led_flags, LED_CNT);
	bitstr_t		bit_decl(ev_snd_flags, SND_CNT);
	bitstr_t		bit_decl(ev_sw_flags, SW_CNT);
	struct input_absinfo *	ev_absinfo;
	bitstr_t		bit_decl(ev_flags, EVDEV_FLAG_CNT);

	/* Repeat parameters & callout: */
	int			ev_rep[REP_CNT];
	struct callout		ev_rep_callout;
	uint16_t		ev_rep_key;

	/* State: */
	bitstr_t		bit_decl(ev_key_states, KEY_CNT);
	bitstr_t		bit_decl(ev_led_states, LED_CNT);
	bitstr_t		bit_decl(ev_snd_states, SND_CNT);
	bitstr_t		bit_decl(ev_sw_states, SW_CNT);
	bool			ev_report_opened;

	/* Multitouch protocol type B state: */
	struct evdev_mt *	ev_mt;

	/* Counters: */
	uint64_t		ev_event_count;
	uint64_t		ev_report_count;

	/* Parent driver callbacks: */
	struct evdev_methods *	ev_methods;
	void *			ev_softc;

	LIST_ENTRY(evdev_dev) ev_link;
	LIST_HEAD(, evdev_client) ev_clients;
};

#define	EVDEV_LOCK(evdev)		mtx_lock(&(evdev)->ev_mtx)
#define	EVDEV_UNLOCK(evdev)		mtx_unlock(&(evdev)->ev_mtx)
#define	EVDEV_LOCK_ASSERT(evdev)	mtx_assert(&(evdev)->ev_mtx, MA_OWNED)

struct evdev_client
{
	struct evdev_dev *	ec_evdev;
	struct mtx		ec_buffer_mtx;
	size_t			ec_buffer_size;
	size_t			ec_buffer_head;
	size_t			ec_buffer_tail;
	size_t			ec_buffer_ready;
	enum evdev_clock_id	ec_clock_id;
	struct selinfo		ec_selp;
	struct sigio *		ec_sigio;
	bool			ec_async;
	bool			ec_revoked;
	bool			ec_blocked;
	bool			ec_selected;

	LIST_ENTRY(evdev_client) ec_link;

	struct input_event	ec_buffer[];
};

#define	EVDEV_CLIENT_LOCKQ(client)	mtx_lock(&(client)->ec_buffer_mtx)
#define	EVDEV_CLIENT_UNLOCKQ(client)	mtx_unlock(&(client)->ec_buffer_mtx)
#define EVDEV_CLIENT_LOCKQ_ASSERT(client) \
    mtx_assert(&(client)->ec_buffer_mtx, MA_OWNED)
#define	EVDEV_CLIENT_EMPTYQ(client) \
    ((client)->ec_buffer_head == (client)->ec_buffer_ready)
#define	EVDEV_CLIENT_SIZEQ(client) \
    (((client)->ec_buffer_ready + (client)->ec_buffer_size - \
      (client)->ec_buffer_head) % (client)->ec_buffer_size)

/* Input device interface: */
struct evdev_dev *evdev_alloc(void);
void evdev_free(struct evdev_dev *);
void evdev_set_name(struct evdev_dev *, const char *);
void evdev_set_phys(struct evdev_dev *, const char *);
void evdev_set_serial(struct evdev_dev *, const char *);
void evdev_set_methods(struct evdev_dev *, void *, struct evdev_methods *);
int evdev_register(device_t, struct evdev_dev *);
int evdev_unregister(device_t, struct evdev_dev *);
int evdev_push_event(struct evdev_dev *, uint16_t, uint16_t, int32_t);
void evdev_send_event(struct evdev_dev *, uint16_t, uint16_t, int32_t);
int evdev_inject_event(struct evdev_dev *, uint16_t, uint16_t, int32_t);
int evdev_sync(struct evdev_dev *);
int evdev_mt_sync(struct evdev_dev *);
int evdev_cdev_create(struct evdev_dev *);
int evdev_cdev_destroy(struct evdev_dev *);
int evdev_support_prop(struct evdev_dev *, uint16_t);
int evdev_support_event(struct evdev_dev *, uint16_t);
int evdev_support_key(struct evdev_dev *, uint16_t);
int evdev_support_rel(struct evdev_dev *, uint16_t);
int evdev_support_abs(struct evdev_dev *, uint16_t, struct input_absinfo *);
int evdev_support_msc(struct evdev_dev *, uint16_t);
int evdev_support_led(struct evdev_dev *, uint16_t);
int evdev_support_snd(struct evdev_dev *, uint16_t);
int evdev_support_sw(struct evdev_dev *, uint16_t);
bool evdev_event_supported(struct evdev_dev *, uint16_t);
int evdev_set_abs_bit(struct evdev_dev *, uint16_t);
int evdev_set_absinfo(struct evdev_dev *, uint16_t, struct input_absinfo *);
void evdev_set_repeat_params(struct evdev_dev *, uint16_t, int);
int evdev_set_report_size(struct evdev_dev *, size_t);
int evdev_set_flag(struct evdev_dev *, uint16_t);

/* Client interface: */
int evdev_register_client(struct evdev_dev *, struct evdev_client *);
void evdev_dispose_client(struct evdev_dev *, struct evdev_client *);
int evdev_grab_client(struct evdev_dev *, struct evdev_client *);
int evdev_release_client(struct evdev_dev *, struct evdev_client *);
void evdev_client_push(struct evdev_client *, uint16_t, uint16_t, int32_t);
void evdev_notify_event(struct evdev_client *);

/* Multitouch related functions: */
void evdev_mt_init(struct evdev_dev *);
void evdev_mt_free(struct evdev_dev *);
int32_t evdev_get_last_mt_slot(struct evdev_dev *);
void evdev_set_last_mt_slot(struct evdev_dev *, int32_t);
int32_t evdev_get_mt_value(struct evdev_dev *, int32_t, int16_t);
void evdev_set_mt_value(struct evdev_dev *, int32_t, int16_t, int32_t);
int32_t evdev_get_mt_slot_by_tracking_id(struct evdev_dev *, int32_t);
void evdev_support_nfingers(struct evdev_dev *, int32_t);
void evdev_support_mt_compat(struct evdev_dev *);
void evdev_push_nfingers(struct evdev_dev *, int32_t);
void evdev_send_mt_compat(struct evdev_dev *);
void evdev_push_mt_compat(struct evdev_dev *);

/* Utility functions: */
uint16_t evdev_hid2key(int);
void evdev_support_all_known_keys(struct evdev_dev *);
uint16_t evdev_scancode2key(int *, int);
void evdev_client_dumpqueue(struct evdev_client *);
void evdev_push_mouse_btn(struct evdev_dev *, int);
void evdev_push_leds(struct evdev_dev *, int);
void evdev_push_repeats(struct evdev_dev *, keyboard_t *);
evdev_event_t evdev_ev_kbd_event;

#endif	/* _DEV_EVDEV_EVDEV_H */
