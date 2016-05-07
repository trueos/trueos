#if !defined(_GPU_SCHED_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _GPU_SCHED_TRACE_H_

#include <drm/drmP.h>

static inline void
trace_amd_sched_job(void* sched_job){
	CTR1(KTR_DRM, "amd_sched_job %p",sched_job);
}

static inline void
trace_amd_sched_process_job(void* s_fence){
	CTR1(KTR_DRM, "amd_process_sched_job %p",s_fence);
}

#endif
