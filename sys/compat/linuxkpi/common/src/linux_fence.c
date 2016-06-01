#include <linux/fence.h>

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/condvar.h>

#include <sys/types.h>
#include <machine/atomic.h>

static atomic64_t fence_context_counter = ATOMIC64_INIT(0);


u64
fence_context_alloc(unsigned num)
{
	return atomic64_add_return(num, &fence_context_counter) - num;
}
