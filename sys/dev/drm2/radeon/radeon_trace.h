#if !defined(_RADEON_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RADEON_TRACE_H_

#include <drm/drmP.h>

static inline void
trace_radeon_cs(struct radeon_cs_parser * parser){
	CTR1(KTR_DRM, "radeon_cs %p", parser);
}

static inline void
trace_radeon_semaphore_signale(int ridx, struct radeon_semaphore * semaphore){
	CTR2(KTR_DRM, "radeon_semaphore_signale %d %p", ridx, semaphore);
}

static inline void
trace_radeon_semaphore_wait(int ridx, struct radeon_semaphore * semaphore){
	CTR2(KTR_DRM, "radeon_semaphore_wait %d %p", ridx, semaphore);
}

static inline void
trace_radeon_bo_create(void *bo)
{
        CTR1(KTR_DRM, "radeon_bo_create %p", bo);
}

static inline void
trace_radeon_fence_emit(void* ddev, int ring, int seq){
	CTR3(KTR_DRM, "radeon_fence_emit %p %d %d", ddev, ring, seq);
}

static inline void
trace_radeon_fence_wait_begin(void* ddev, int i, int seq){
	CTR3(KTR_DRM, "radeon_fence_wait_begin %p %d %d", ddev, i, seq);
}

static inline void
trace_radeon_fence_wait_end(void* ddev, int i, int seq){
	CTR3(KTR_DRM, "radeon_fence_wait_end %p %d %d", ddev, i, seq);
}

#endif
