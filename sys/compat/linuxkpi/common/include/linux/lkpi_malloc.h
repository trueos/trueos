#ifndef _LKPI_MALLOC_H_
#define _LKPI_MALLOC_H_
#include <sys/malloc.h>

void	*lkpi_malloc(unsigned long size, struct malloc_type *type, int flags)
	    __malloc_like __result_use_check __alloc_size(1);

void	lkpi_free(void *addr, struct malloc_type *type);

void	*lkpi_realloc(void *addr, unsigned long size, struct malloc_type *mtp, int flags);
#endif
