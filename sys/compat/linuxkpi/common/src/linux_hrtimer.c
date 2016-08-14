#include <sys/types.h>
#include <sys/systm.h>

#include <linux/hrtimer.h>
#include <machine/cpu.h>

CTASSERT(sizeof(struct callout) <= BSD_CALLOUT_SIZE);

static void
__hrtimer_handler(void *arg)
{
	struct hrtimer *h = arg;
	enum hrtimer_restart rc;
	int flags;
	struct callout *c = __DECONST(struct callout *, &h->bsd_callout_);

	flags = C_PREL(0);
	rc = h->function(h);
	if (rc == HRTIMER_RESTART)
		callout_reset_sbt(c, c->c_time, c->c_precision, __hrtimer_handler, h, flags);
}

int
hrtimer_try_to_cancel(struct hrtimer *timer)
{
	struct callout *c;

	if (!hrtimer_active(timer))
		return (0);

	c = (struct callout *)&timer->bsd_callout_;
	return -(callout_stop(c) == 0);
}

void
hrtimer_init(struct hrtimer *timer, clockid_t which_clock,  enum hrtimer_mode mode)
{
	struct callout *c = __DECONST(struct callout *, &timer->bsd_callout_);

	/* we only support MONOTONIC */
	MPASS(which_clock == CLOCK_MONOTONIC);

	memset(timer, 0, sizeof(struct hrtimer));
	callout_init(c, 1 /* mpsafe */);
}

void
hrtimer_start_range_ns(struct hrtimer *timer, ktime_t tim, u64 range_ns, const enum hrtimer_mode mode)
{
	struct callout *c = __DECONST(struct callout *, &timer->bsd_callout_);
	int flags;

	flags = C_PREL(0);
	if (mode == HRTIMER_MODE_ABS ||
	    mode == HRTIMER_MODE_ABS_PINNED)
		flags |= C_ABSOLUTE;
		
	callout_reset_sbt(c, tim.tv64*SBT_1NS, range_ns*SBT_1NS,  __hrtimer_handler, timer, flags);
}

bool
hrtimer_active(const struct hrtimer *timer)
{
	struct callout *c = __DECONST(struct callout *, &timer->bsd_callout_);

	return (callout_running(c));
}

int
hrtimer_cancel(struct hrtimer *timer)
{
	int rc;

	for (;;) {
		rc = hrtimer_try_to_cancel(timer);

		if (rc >= 0)
			return (rc);
		cpu_spinwait();
	}	
}
