
#ifndef _VM_EVENTHANDLER_H_
#define _VM_EVENTHANDLER_H_
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

struct vm_eventhandler_map {
	LIST_HEAD(, vm_eventhandler)	vem_head;
	struct mtx			vem_mtx;
};

struct vm_eventhandler_ops {

	void (*vme_exit)(struct vm_eventhandler *vme,
			 vm_map_t map);

	void (*vme_invalidate_page)(struct vm_eventhandler *vme,
				    vm_map_t map,
				    vm_offset_t addr);

	void (*vme_invalidate_range_start)(struct vm_eventhandler *vme,
					   vm_map_t map,
					   vm_offset_t start,
					   vm_offset_t end);

	void (*vme_invalidate_range_end)(struct vm_eventhandler *vme,
					 vm_map_t map,
					 vm_offset_t start,
					 vm_offset_t end);	
#ifdef __notyet__
	/* needed for the Intel Shared Virtual Memory driver (not GPU) */
	void (*vme_update_mapping)(struct vm_eventhandler *vme,
				   vm_map_t map,
				   vm_offset_t addr,
				   pte_t pte);
#endif	
};

struct vm_eventhandler {
	LIST_ENTRY(vm_eventhandler) vme_entry;
	const struct vm_eventhandler_ops vme_ops;
};


static inline int
vme_map_has_eh(vm_map_t map)
{
	return (__predict_false(map->vem_map != NULL));
}

void vm_eventhandler_register(vm_map_t map, struct vm_eventhandler *ve);
void vm_eventhandler_deregister(vm_map_t map, struct vm_eventhandler *ve);

int vme_map_has_invalidate_page(vm_map_t map);
void vme_invalidate_range_start_impl(vm_map_t map, vm_offset_t start, vm_offset_t end);
void vme_invalidate_range_end_impl(vm_map_t map, vm_offset_t start, vm_offset_t end);
void vme_invalidate_page_impl(vm_map_t map, vm_offset_t addr);
void vme_exit_impl(vm_map_t map);


static inline void
vme_invalidate_range_start(vm_map_t map, vm_offset_t start, vm_offset_t end)
{
	vm_offset_t addr;

	if (vme_map_has_eh(map))
		vme_invalidate_range_start_impl(map, start, end);
	if (vme_map_has_eh(map) && vme_map_has_invalidate_page(map))
		for (addr = start; addr < end; addr += PAGE_SIZE)
			vme_invalidate_page_impl(map, addr);
}

static inline void
vme_invalidate_range_end(vm_map_t map, vm_offset_t start, vm_offset_t end)
{
	if (vme_map_has_eh(map))
		vme_invalidate_range_end_impl(map, start, end);
}

static inline void
vme_exit(vm_map_t map)
{
	if (vme_map_has_eh(map))
		vme_exit_impl(map);
}

#endif
