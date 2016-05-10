# $FreeBSD$

.PATH:	${.CURDIR}/../../../../dev/drm2/amd/amdkfd

KMOD	= amdkfd

# XXX NOT PORTED YET:
#	kfd_device.c : amd_iommu 
#	kfd_chardev.c : 
#	kfd_device_queue_manager_cik.c :
#	kfd_device_queue_manager_vi.c :
#	kfd_doorbell.c :
#	kfd_events.c :
#	kfd_interrupt.c :
#	kfd_kernel_queue_cik.c :
#	kfd_kernel_queue_vi.c :
#	kfd_module.c :
#	kfd_mqd_manager.c :
#	kfd_mqd_manager_cik.c :
#	kfd_mqd_manager_vi.c :
#	kfd_packet_manager.c :
#	kfd_pasid.c :
#	kfd_process.c :
#	kfd_process_queue_manager.c :
#	kfd_queue.c :
#	kfd_topology.c :

SRCS	= \
	kfd_dbgmgr.c \
	kfd_dbgdev.c \
	kfd_dbgmgr.c \
	kfd_device_queue_manager.c \
	kfd_flat_memory.c \
	kfd_kernel_queue.c

CFLAGS+=	-I${.CURDIR}/../../../../compat/linuxkpi/common/include
CFLAGS+=	-I${.CURDIR}/../../../../compat/linuxkpi/common/include/drm
CFLAGS+=	-I${.CURDIR}/../../../../compat/linuxkpi/common/include/uapi
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/amdgpu
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/include
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/acp/include
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/powerplay/inc
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/scheduler
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/include/asic_reg
CFLAGS+=	'-DKBUILD_MODNAME="${KMOD}"'

SRCS	+=device_if.h vnode_if.h bus_if.h pci_if.h device_if.h iicbus_if.h opt_drm.h \
        opt_vm.h opt_compat.h opt_syscons.h

.include <bsd.kmod.mk>

CWARNFLAGS+=	-Wno-pointer-arith
CWARNFLAGS+=	-Wno-pointer-sign ${CWARNFLAGS.${.IMPSRC:T}}

CWARNFLAGS.kfd_dbgdev.c=	-Wno-format
CWARNFLAGS.kfd_device_queue_manager.c=	-Wno-format
