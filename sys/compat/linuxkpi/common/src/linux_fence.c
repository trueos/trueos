#include <linux/fence.h>
#include <linux/fence-array.h>

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/condvar.h>

#include <sys/types.h>
#include <machine/atomic.h>

static unsigned fence_counter = 0;

unsigned
fence_context_alloc(unsigned num){
	return atomic_fetchadd_int(&fence_counter, num);
}

struct fence_array *fence_array_create(int num_fences, struct fence **fences,
				       u64 context, unsigned seqno,
				       bool signal_on_any)
{
	struct fence_array *array;
	size_t size = sizeof(*array);

	size += num_fences * sizeof(struct fence_array_cb);
	array = kzalloc(size, GFP_KERNEL);
	if (!array)
		return NULL;

	spin_lock_init(&array->lock);
	fence_init(&array->base, &fence_array_ops, &array->lock,
		   context, seqno);

	array->num_fences = num_fences;
	atomic_set(&array->num_pending, signal_on_any ? 1 : num_fences);
	array->fences = fences;

	return (array);
}
