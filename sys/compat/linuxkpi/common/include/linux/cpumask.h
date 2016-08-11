#ifndef __LINUX_CPUMASK_H
#define __LINUX_CPUMASK_H


/* Don't assign or return these: may not be this big! */
typedef struct cpumask { DECLARE_BITMAP(bits, NR_CPUS); } cpumask_t;

extern cpumask_t *cpu_all_mask;
extern cpumask_t *cpu_online_mask;

static inline ssize_t
cpumap_print_to_pagebuf(bool list, char *buf, const struct cpumask *mask)
{
#if 0
	return bitmap_print_to_pagebuf(list, buf, cpumask_bits(mask),
				       nr_cpu_ids);
#else
	return (0);
#endif
		
}
#endif
