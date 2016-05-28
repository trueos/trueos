#include <sys/types.h>
#include <sys/systm.h>

#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/acpi.h>
#include <linux/oom.h>
#include <linux/mmu_notifier.h>
#include <linux/shrinker.h>
#include <linux/backlight.h>
#include <acpi/button.h>

/*
 * This file will be used to register 
 */

int
register_reboot_notifier(struct notifier_block *nb)
{
	WARN_NOT();
	return (0);
}

int
unregister_reboot_notifier(struct notifier_block *nb)
{
	WARN_NOT();
	return (0);
}

int
acpi_lid_notifier_register(struct notifier_block *nb)
{
	WARN_NOT();
	return (0);
}

int
acpi_lid_notifier_unregister(struct notifier_block *nb)
{
	WARN_NOT();
	return (0);
}

int
register_acpi_notifier(struct notifier_block *nb)
{
	WARN_NOT();
	return (0);
}

int
unregister_acpi_notifier(struct notifier_block *nb)
{
	WARN_NOT();
	return (0);
}

int
register_oom_notifier(struct notifier_block *nb)
{

	WARN_NOT();
	return (0);
}

int
unregister_oom_notifier(struct notifier_block *nb)
{

	WARN_NOT();
	return (0);

}

int
register_shrinker(struct shrinker *s)
{

	WARN_NOT();
	return (0);
}

void
unregister_shrinker(struct shrinker *s)
{
	WARN_NOT();
}

int
backlight_register_notifier(struct notifier_block *nb)
{

	WARN_NOT();
	return (0);
}

int
backlight_unregister_notifier(struct notifier_block *nb)
{
	WARN_NOT();
	return (0);
}
