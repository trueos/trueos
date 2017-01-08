/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_IO_MAPPING_H_
#define	_LINUX_IO_MAPPING_H_

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/bug.h>
#include <linux/io.h>
#include <linux/page.h>

struct io_mapping {
	vm_paddr_t base;
	unsigned long size;
	vm_prot_t prot;
	void __iomem *iomem;
	struct resource *r;
};

static inline struct io_mapping *
io_mapping_init_wc(struct io_mapping *iomap,
		   resource_size_t base,
		   unsigned long size)
{
	iomap->base = base;
	iomap->size = size;
	iomap->iomem = ioremap_wc(base, size);
	iomap->prot = pgprot_writecombine(PAGE_KERNEL_IO);

	return iomap;
}

static inline struct io_mapping *
io_mapping_create_wc(vm_paddr_t base, unsigned long size)
{
	struct io_mapping *iomap;

	if ((iomap = kmalloc(sizeof(*iomap), GFP_KERNEL)) == NULL)
		return (NULL);

	if (!io_mapping_init_wc(iomap, base, size)) {
		kfree(iomap);
		return NULL;
	}

	return (iomap);
}

static inline void
io_mapping_fini(struct io_mapping *mapping)
{
	iounmap(mapping->iomem);
}

static inline void
io_mapping_free(struct io_mapping *mapping)
{
	io_mapping_fini(mapping);
	kfree(mapping);
}

static inline void *
io_mapping_map_atomic_wc(struct io_mapping *mapping,
			 unsigned long offset)
{
	vm_paddr_t phys_addr;
	unsigned long pfn;

	BUG_ON(offset >= mapping->size);
	phys_addr = mapping->base + offset;
	pfn = (unsigned long) (phys_addr >> PAGE_SHIFT);
	mapping->prot = PAT_WRITE_COMBINING;
	return iomap_atomic_prot_pfn(pfn, mapping->prot);
}

static inline void
io_mapping_unmap_atomic(void *vaddr)
{
	iounmap_atomic(vaddr);
}

static inline void __iomem *
io_mapping_map_wc(struct io_mapping *mapping,
		  unsigned long offset,
		  unsigned long size)
{
	resource_size_t phys_addr;

	BUG_ON(offset >= mapping->size);
	phys_addr = mapping->base + offset;

	return ioremap_wc(phys_addr, size);
}

static inline void
io_mapping_unmap(void *vaddr)
{
	iounmap(vaddr);
}

#endif	/* _LINUX_IO_MAPPING_H_ */
