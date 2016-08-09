#ifndef _LINUX_PWM_H_
#define _LINUX_PWM_H_

#include <linux/mutex.h>
#include <linux/device.h>

struct pwm_device;

static inline void
pwm_put(struct pwm_device *pwm)
{
	UNIMPLEMENTED();
}

static inline struct pwm_device *
pwm_get(struct device *dev, const char *con_id)
{
	UNIMPLEMENTED();
	return (NULL);
}

static inline int
pwm_enable(struct pwm_device *pwm)
{
	UNIMPLEMENTED();
	return (0);
}

static inline void
pwm_disable(struct pwm_device *pwm)
{
	UNIMPLEMENTED();
}

static inline unsigned int
pwm_get_duty_cycle(const struct pwm_device *pwm)
{
	UNIMPLEMENTED();
	return (0);
}

static inline int
pwm_config(struct pwm_device *pwm, int duty_ns, int period_ns)
{
	UNIMPLEMENTED();
	return (0);
}

enum pwm_polarity {
        PWM_POLARITY_NORMAL,
        PWM_POLARITY_INVERSED,
};

struct pwm_device {
        const char *label;
        unsigned long flags;
        unsigned int hwpwm;
        unsigned int pwm;
        struct pwm_chip *chip;
        void *chip_data;
        struct mutex lock;

        unsigned int period;
        unsigned int duty_cycle;
	enum pwm_polarity polarity;
};
#endif
