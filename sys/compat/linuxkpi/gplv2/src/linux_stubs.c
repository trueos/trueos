#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/cpumask.h>
#include <linux/pci.h>

cpumask_t *cpu_all_mask;
cpumask_t *cpu_online_mask;
