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
#endif

