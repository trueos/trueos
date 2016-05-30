#if !defined(_AMDGPU_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _AMDGPU_TRACE_H_

#include <drm/drmP.h>
#include "amdgpu.h"

static inline void
trace_amdgpu_cs_ioctl(struct amdgpu_job *job){
	CTR1(KTR_DRM, "amdgpu_cs_ioctl %p", job);
}

static inline void
trace_amdgpu_cs(struct amdgpu_cs_parser *parser, int i){
	CTR2(KTR_DRM, "amdgpu_cs %d %p", i, parser);
}

static inline void
trace_amdgpu_sched_run_job(struct amdgpu_job *job){
	CTR1(KTR_DRM, "amdgpu_sched_run_job %p", job);
}

static inline void
trace_amdgpu_bo_create(void *bo)
{
	CTR1(KTR_DRM, "amdgpu_bo_create %p", bo);
}

static inline void
trace_amdgpu_bo_list_set(void *a, void *b) {
	CTR2(KTR_DRM, "amdgpu_bo_list_set %p %p", a, b);
}
static inline void
trace_amdgpu_vm_grab_id(struct amdgpu_vm *vm, int idx, unsigned vm_id, uint64_t vm_pd_addr){
        CTR4(KTR_DRM, "amdgpu_vm_grab_id %p %u %u %x", vm, idx, vm_id, vm_pd_addr);
}

static inline void
trace_amdgpu_vm_flush(uint64_t pd_addr, int idx, unsigned vm_id){
	CTR3(KTR_DRM, "amdgpu_vm_flush %x %u %u", pd_addr, idx, vm_id);
}

static inline void
trace_amdgpu_vm_bo_unmap(struct amdgpu_bo_va * bo_va, struct amdgpu_bo_va_mapping * mapping){
	CTR2(KTR_DRM, "amdgpu_vm_bo_unmap %p %p", bo_va, mapping);
}

static inline void
trace_amdgpu_vm_bo_update(struct amdgpu_bo_va_mapping * mapping){
	CTR1(KTR_DRM, "amdgpu_vm_bo_update %p", mapping);
}

static inline void
trace_amdgpu_vm_set_page(uint64_t pe, uint64_t addr, unsigned count, uint32_t incr, uint32_t flags){
	CTR5(KTR_DRM, "amdgpu_vm_set_page %x %x %u %u %x", pe, addr, count, incr, flags);
}

static inline int
trace_amdgpu_vm_bo_mapping_enabled(void){
	CTR0(KTR_DRM, "amdgpu_vm_bo_mapping_enabled");
	return (0);
}

static inline void
trace_amdgpu_vm_bo_mapping(struct amdgpu_bo_va_mapping * mapping){
	CTR1(KTR_DRM, "amdgpu_vm_bo_mapping %p", mapping);
}

#endif

