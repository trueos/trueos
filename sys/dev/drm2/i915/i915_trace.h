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
trace_i915_gem_ring_flush(struct intel_ring_buffer *ring, u32 gpu_domains, u32 flush_domains)
{
	CTR3(KTR_DRM, "ring_flush %s %d %d", ring->name, gpu_domains, flush_domains);
}

static inline void
trace_i915_gem_request_complete(struct intel_ring_buffer *ring, u32 seqno)
{
	CTR2(KTR_DRM, "request_complete %s %d", ring->name, seqno);
}

static inline void
trace_i915_gem_ring_dispatch(struct intel_ring_buffer *ring, u32 seqno, u32 flags)
{
	CTR3(KTR_DRM, "ring_dispatch ring=%s seqno=%d flags=%u", ring->name,
		seqno, flags);
}

static inline void
trace_i915_gem_object_create(struct drm_i915_gem_object *obj)
{
	CTR2(KTR_DRM, "object_create %p %x", obj, size);
}

static inline void
trace_i915_gem_object_pread(struct drm_i915_gem_object *obj, u64 offset, u64 size)
{
	CTR3(KTR_DRM, "pread %p %jx %jx", obj, offset, size);
}

static inline void
trace_i915_gem_object_pwrite(struct drm_i915_gem_object *obj, u64 offset, u64 size)
{
	CTR3(KTR_DRM, "pwrite %p %jx %jx", obj, offset, size);
}

static inline void
trace_i915_gem_request_wait_end(struct intel_ring_buffer *ring, u32 seqno)
{
	CTR2(KTR_DRM, "request_wait_end %s %d", ring->name, seqno);
}

static inline void
trace_i915_gem_request_add(struct intel_ring_buffer *ring, u32 seqno)
{
	CTR2(KTR_DRM, "request_add %s %d", ring->name, seqno);
}

static inline void
trace_i915_gem_request_retire(struct intel_ring_buffer *ring, u32 seqno)
{
	CTR2(KTR_DRM, "retire_request_seqno_passed %s %d",
		ring->name, seqno);
}

static inline void
trace_i915_gem_object_change_domain(struct drm_i915_gem_object *obj, u32 old_read_domains, u32 old_write_domain)
{
	CTR3(KTR_DRM, "object_change_domain  %p %x %x",
		obj, old_read_domains, old_write_domain);
}

static inline void
trace_i915_gem_request_wait_begin(struct intel_ring_buffer *ring, u32 seqno)
{
	CTR2(KTR_DRM, "request_wait_begin %s %d", ring->name, seqno);
}

static inline void
trace_i915_gem_object_unbind(struct drm_i915_gem_object *obj)
{
	CTR1(KTR_DRM, "object_unbind %p", obj);
}

static inline void
trace_i915_gem_object_clflush(struct drm_i915_gem_object *obj)
{
	CTR1(KTR_DRM, "object_clflush %p", obj);
}

static inline void
trace_i915_gem_object_bind(struct drm_i915_gem_object *obj, bool map_and_fenceable)
{
	CTR4(KTR_DRM, "object_bind %p %x %x %d", obj, obj->gtt_offset,
		obj->base.size, map_and_fenceable);
}

static inline void
trace_i915_gem_object_destroy(struct drm_i915_gem_object *obj)
{
	CTR1(KTR_DRM, "object_destroy_tail %p", obj);
}

static inline void
trace_i915_gem_evict(struct drm_device *dev, int min_size, unsigned alignment, bool mappable)
{
	CTR4(KTR_DRM, "evict_something %p %d %u %d", dev, min_size,
		alignment, mappable);
}

static inline void
trace_i915_gem_evict_everything(struct drm_device *dev){
	CTR1(KTR_DRM, "evict_everything %p", dev);
}

#endif /* _I915_TRACE_H_ */
