#ifndef __LINUX_KFIFO_H_
#define __LINUX_KFIFO_H_

struct kfifo {
};

static inline int
kfifo_alloc(struct kfifo *fifo, unsigned int size, size_t esize, gfp_t gfp_mask){
	UNIMPLEMENTED();
	return (0);
}

#endif
