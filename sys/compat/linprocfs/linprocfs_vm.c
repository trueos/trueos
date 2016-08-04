#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/sbuf.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/procfs/procfs.h>

#include <compat/linprocfs/linprocfs.h>

static int
lp_vm_admin_reserve_kbytes(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_block_dump(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_compact_memory(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_compact_unevictable_allowed(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_dirty_background_bytes(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_dirty_background_ratio(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_dirty_bytes(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_dirty_expire_centisecs(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_dirty_ratio(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_dirtytime_expire_seconds(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_dirty_writeback_centisecs(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_drop_caches(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_extfrag_threshold(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_hugepages_treat_as_movable(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_hugetlb_shm_group(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_laptop_mode(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_legacy_va_layout(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_lowmem_reserve_ratio(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_max_map_count(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_memory_failure_early_kill(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_memory_failure_recovery(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_min_free_kbytes(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_min_slab_ratio(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_min_unmapped_ratio(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_mmap_min_addr(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_nr_hugepages(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_nr_hugepages_mempolicy(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_nr_overcommit_hugepages(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_nr_pdflush_threads(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_numa_zonelist_order(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_oom_dump_tasks(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_oom_kill_allocating_task(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_overcommit_kbytes(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_overcommit_memory(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_overcommit_ratio(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_page_cluster(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_panic_on_oom(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_percpu_pagelist_fraction(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_stat_interval(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_swappiness(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_user_reserve_kbytes(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_vfs_cache_pressure(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}

static int
lp_vm_zone_reclaim_mode(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "");
	return (0);
}



struct lp_fill_entry vm_entries[] = {
	{"admin_reserve_kbytes",
	 &lp_vm_admin_reserve_kbytes,
	 PFS_RD},
	{"block_dump",
	 &lp_vm_block_dump,
	 PFS_RD},
	{"compact_memory",
	 &lp_vm_compact_memory,
	 PFS_RD},
	{"compact_unevictable_allowed",
	 &lp_vm_compact_unevictable_allowed,
	 PFS_RD},
	{"dirty_background_bytes",
	 &lp_vm_dirty_background_bytes,
	 PFS_RD},
	{"dirty_background_ratio",
	 &lp_vm_dirty_background_ratio,
	 PFS_RD},
	{"dirty_bytes",
	 &lp_vm_dirty_bytes,
	 PFS_RD},
	{"dirty_expire_centisecs",
	 &lp_vm_dirty_expire_centisecs,
	 PFS_RD},
	{"dirty_ratio",
	 &lp_vm_dirty_ratio,
	 PFS_RD},
	{"dirtytime_expire_seconds",
	 &lp_vm_dirtytime_expire_seconds,
	 PFS_RD},
	{"dirty_writeback_centisecs",
	 &lp_vm_dirty_writeback_centisecs,
	 PFS_RD},
	{"drop_caches",
	 &lp_vm_drop_caches,
	 PFS_RD},
	{"extfrag_threshold",
	 &lp_vm_extfrag_threshold,
	 PFS_RD},
	{"hugepages_treat_as_movable",
	 &lp_vm_hugepages_treat_as_movable,
	 PFS_RD},
	{"hugetlb_shm_group",
	 &lp_vm_hugetlb_shm_group,
	 PFS_RD},
	{"laptop_mode",
	 &lp_vm_laptop_mode,
	 PFS_RD},
	{"legacy_va_layout",
	 &lp_vm_legacy_va_layout,
	 PFS_RD},
	{"lowmem_reserve_ratio",
	 &lp_vm_lowmem_reserve_ratio,
	 PFS_RD},
	{"max_map_count",
	 &lp_vm_max_map_count,
	 PFS_RD},
	{"memory_failure_early_kill",
	 &lp_vm_memory_failure_early_kill,
	 PFS_RD},
	{"memory_failure_recovery",
	 &lp_vm_memory_failure_recovery,
	 PFS_RD},
	{"min_free_kbytes",
	 &lp_vm_min_free_kbytes,
	 PFS_RD},
	{"min_slab_ratio",
	 &lp_vm_min_slab_ratio,
	 PFS_RD},
	{"min_unmapped_ratio",
	 &lp_vm_min_unmapped_ratio,
	 PFS_RD},
	{"mmap_min_addr",
	 &lp_vm_mmap_min_addr,
	 PFS_RD},
	{"nr_hugepages",
	 &lp_vm_nr_hugepages,
	 PFS_RD},
	{"nr_hugepages_mempolicy",
	 &lp_vm_nr_hugepages_mempolicy,
	 PFS_RD},
	{"nr_overcommit_hugepages",
	 &lp_vm_nr_overcommit_hugepages,
	 PFS_RD},
	{"nr_pdflush_threads",
	 &lp_vm_nr_pdflush_threads,
	 PFS_RD},
	{"numa_zonelist_order",
	 &lp_vm_numa_zonelist_order,
	 PFS_RD},
	{"oom_dump_tasks",
	 &lp_vm_oom_dump_tasks,
	 PFS_RD},
	{"oom_kill_allocating_task",
	 &lp_vm_oom_kill_allocating_task,
	 PFS_RD},
	{"overcommit_kbytes",
	 &lp_vm_overcommit_kbytes,
	 PFS_RD},
	{"overcommit_memory",
	 &lp_vm_overcommit_memory,
	 PFS_RD},
	{"overcommit_ratio",
	 &lp_vm_overcommit_ratio,
	 PFS_RD},
	{"page-cluster",
	 &lp_vm_page_cluster,
	 PFS_RD},
	{"panic_on_oom",
	 &lp_vm_panic_on_oom,
	 PFS_RD},
	{"percpu_pagelist_fraction",
	 &lp_vm_percpu_pagelist_fraction,
	 PFS_RD},
	{"stat_interval",
	 &lp_vm_stat_interval,
	 PFS_RD},
	{"swappiness",
	 &lp_vm_swappiness,
	 PFS_RD},
	{"user_reserve_kbytes",
	 &lp_vm_user_reserve_kbytes,
	 PFS_RD},
	{"vfs_cache_pressure",
	 &lp_vm_vfs_cache_pressure,
	 PFS_RD},
	{"zone_reclaim_mode",
	 &lp_vm_zone_reclaim_mode,
	 PFS_RD},
	{NULL, NULL, 0}
};

void
linprocfs_vm_init(struct pfs_node *root)
{
	struct pfs_node *dir;
	struct lp_fill_entry *fe;

	dir = pfs_create_dir(root, "vm", NULL, NULL, NULL, 0);
	for (fe = vm_entries; fe->fe_name != NULL; fe++) 
		pfs_create_file(dir, fe->fe_name, fe->fe_fill,
				NULL, NULL, NULL, fe->fe_flags);
}
