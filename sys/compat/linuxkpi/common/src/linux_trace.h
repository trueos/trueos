#ifndef _LINUX_TRACE_H_
#define _LINUX_TRACE_H_

#define KTR_LINUX KTR_SPARE4


#define trace_compat_cdev_pager_fault(obj, offset, prot, mres) \
	CTR4(KTR_LINUX, "cdev_pager_fault: obj %p offset %zu prot: %x mres %p", obj, offset, prot, *mres);


#define trace_compat_cdev_page_lookup_busy(obj, page, paddr) \
	CTR3(KTR_LINUX, "cdev_page_lookup_busy: obj %p page %p paddr %zx", obj, page, paddr);


#define trace_compat_cdev_vm_ops_return(obj, page, paddr, err) \
	CTR4(KTR_LINUX, "cdev_vm_ops_return: obj %p page %p paddr %zx err %d", obj, page, paddr, err);

#endif
