#include <linux/fence.h>

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
