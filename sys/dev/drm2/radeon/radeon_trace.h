#if !defined(_RADEON_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RADEON_TRACE_H_

#include <drm/drmP.h>

static inline void
trace_radeon_cs(struct radeon_cs_parser * parser){
	CTR1(KTR_DRM, "radeon_cs %p", parser);
}

#endif
