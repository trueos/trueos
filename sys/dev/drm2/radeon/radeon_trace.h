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



#endif
