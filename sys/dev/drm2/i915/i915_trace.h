#ifndef _I915_TRACE_H_
#define _I915_TRACE_H_

#include <dev/drm2/drmP.h>
#include "i915_drv.h"
#include "intel_drv.h"
#include "intel_ringbuffer.h"

static inline void
trace_i915_flip_complete(enum plane plane, struct drm_i915_gem_object *pending_flip_obj)
{
	CTR2(KTR_DRM, "i915_flip_complete %d %p", plane, pending_flip_obj);
}

static inline void
trace_i915_flip_request(enum plane plane, struct drm_i915_gem_object *obj)
{
	CTR2(KTR_DRM, "i915_flip_request %d %p", plane, obj);
}

static inline void
trace_i915_ring_wait_begin(struct intel_ring_buffer *ring){
	CTR1(KTR_DRM, "ring_wait_begin %s", ring->name);
}

static inline void
trace_i915_ring_wait_end(struct intel_ring_buffer *ring)
{
	CTR1(KTR_DRM, "ring_wait_end %s", ring->name);
}

static inline void
trace_i915_gem_ring_flush(struct intel_ring_buffer *ring, int gpu_domains, int flush_domains)
{
	CTR3(KTR_DRM, "ring_flush %s %d %d", ring->name, gpu_domains, flush_domains);
}

static inline void
trace_i915_gem_request_complete(struct intel_ring_buffer *ring, int seqno)
{
	CTR2(KTR_DRM, "request_complete %s %d", ring->name, seqno);
}

static inline void
trace_i915_gem_object_change_domain(struct drm_i915_gem_object *obj, u32 old_read, u32 old_write)
{
	CTR3(KTR_DRM, "object_change_domain move_to_active %p %x %x",
		obj, old_read, old_write);
}

static inline void
trace_i915_gem_ring_dispatch(struct intel_ring_buffer *ring, int seqno, u32 flags)
{
	CTR3(KTR_DRM, "ring_dispatch ring=%s seqno=%d flags=%u", ring->name,
		seqno, flags);
}

#endif /* _I915_TRACE_H_ */
