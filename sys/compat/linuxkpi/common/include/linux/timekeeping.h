#ifndef _LINUX_TIMEKEEPING_H
#define _LINUX_TIMEKEEPING_H

#include <linux/ktime.h>

static inline u64 ktime_get_raw_ns(void)
{
        struct timespec ts;

        nanouptime(&ts);

        return (ts.tv_sec * NSEC_PER_SEC) + ts.tv_nsec;
}

static inline ktime_t
ktime_mono_to_real(ktime_t mono)
{
	struct timespec tsb, tsn;
	uint64_t now, boot;
	ktime_t kt;
        nanotime(&tsn);
        nanouptime(&tsb);

	now = (tsn.tv_sec * NSEC_PER_SEC) + tsn.tv_nsec;
	boot = (tsb.tv_sec * NSEC_PER_SEC) + tsb.tv_nsec;
	/* return ktime_mono_to_any(mono, TK_OFFS_REAL); */
	kt.tv64 += (now - boot);

	return (kt);
}

/**
 * ktime_get_real - get the real (wall-) time in ktime_t format
 */
static inline ktime_t
ktime_get_real(void)
{
	struct timespec ts;
	ktime_t kt;
	
        nanotime(&ts);
        kt.tv64 = (ts.tv_sec * NSEC_PER_SEC) + ts.tv_nsec;
	return (kt);
}

static inline ktime_t
ktime_get_boottime(void)
{
        struct timespec ts;
	ktime_t kt;

        nanouptime(&ts);

        kt.tv64 = (ts.tv_sec * NSEC_PER_SEC) + ts.tv_nsec;
	return (kt);
}

#endif
