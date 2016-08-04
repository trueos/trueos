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
#include <linux/rcupdate.h>

/*
 * This file will be used to register 
 */

static int
notifier_chain_register(struct notifier_block **nl,
			struct notifier_block *n)
{
	while ((*nl) != NULL) {
		if (n->priority > (*nl)->priority)
			break;
		nl = &((*nl)->next);
	}
	n->next = *nl;
	rcu_assign_pointer(*nl, n);
	return (0);
}

static int
notifier_chain_unregister(struct notifier_block **nl,
			  struct notifier_block *n)
{
	while ((*nl) != NULL) {
		if ((*nl) == n) {
			rcu_assign_pointer(*nl, n->next);
			return (0);
		}
		nl = &((*nl)->next);
	}
	return (-ENOENT);
}


static int
notifier_call_chain(struct notifier_block **nl,
		    unsigned long val, void *v,
		    int nr_to_call, int *nr_calls)
{
	int ret = NOTIFY_DONE;
	struct notifier_block *nb, *next_nb;

	nb = rcu_dereference(*nl);

	while (nb && nr_to_call) {
		next_nb = rcu_dereference(nb->next);

		ret = nb->notifier_call(nb, val, v);

		if (nr_calls)
			(*nr_calls)++;

		if ((ret & NOTIFY_STOP_MASK) == NOTIFY_STOP_MASK)
			break;
		nb = next_nb;
		nr_to_call--;
	}
	return (ret);
}

int
blocking_notifier_chain_register(struct blocking_notifier_head *nh,
		struct notifier_block *n)
{
	int rc;

	down_write(&nh->rwsem);
	rc = notifier_chain_register(&nh->head, n);
	up_write(&nh->rwsem);
	return (rc);
}

int
blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
		struct notifier_block *n)
{
	int rc;

	down_write(&nh->rwsem);
	rc = notifier_chain_unregister(&nh->head, n);
	up_write(&nh->rwsem);
	return (rc);
}

static int
__blocking_notifier_call_chain(struct blocking_notifier_head *nh,
				   unsigned long val, void *v,
				   int nr_to_call, int *nr_calls)
{
	int rc = NOTIFY_DONE;

	if (rcu_access_pointer(nh->head)) {
		down_read(&nh->rwsem);
		rc = notifier_call_chain(&nh->head, val, v, nr_to_call,
					nr_calls);
		up_read(&nh->rwsem);
	}
	return (rc);
}

int
blocking_notifier_call_chain(struct blocking_notifier_head *nh,
		unsigned long val, void *v)
{
	return (__blocking_notifier_call_chain(nh, val, v, -1, NULL));
}
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

