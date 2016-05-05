# $FreeBSD$

.PATH:	${.CURDIR}/../../../../dev/drm2/amd/amdgpu

KMOD	= amdgpu

# XXX we do not enable this yet
#       amdgpu_acp.h \
#       amdgpu_acp.c
SRCS	= \
	amdgpu.h \
        amdgpu_acpi.c \
        amdgpu_afmt.c \
        amdgpu_amdkfd.h \
        amdgpu_amdkfd.c \
        amdgpu_atombios.h \
        amdgpu_atombios.c \
        amdgpu_bios.c \
	amdgpu_cs.c \
	amdgpu_ctx.c \
	amdgpu_device.c \
	amdgpu_display.c \
	amdgpu_dpm.c \
	amdgpu_drv.c \
	amdgpu_encoders.c \
	amdgpu_fb.c \
	amdgpu_fence.c \
	amdgpu_gart.c \
	amdgpu_gem.c \
	amdgpu_ring.c \
	amdgpu_sa.c \
	amdgpu_test.c \
	amdgpu_ucode.c \
	amdgpu_uvd.c \
	amdgpu_vce.c \
	ci_dpm.c \
	ci_smc.c \
	cik.h \
	cik.c \
	cik_dpm.h \
	cik_ih.h \
	cik_ih.c \
        cz_dpm.h \
        cz_dpm.c \
        cz_ih.h \
        cz_ih.c \
        cz_smc.c \
        cz_smumgr.h \
	dce_v10_0.h \
	dce_v10_0.c \
	dce_v11_0.h \
	dce_v11_0.c \
	dce_v8_0.h \
	dce_v8_0.c \
	fiji_dpm.c \
	fiji_smc.c \
	fiji_smum.h \
	gfx_v7_0.h \
	gfx_v7_0.c \
	gfx_v8_0.h \
	gfx_v8_0.c \
	gmc_v7_0.h \
	gmc_v7_0.c \
	gmc_v8_0.h \
	gmc_v8_0.c \
	iceland_dpm.c \
	iceland_ih.h \
	iceland_ih.c \
	iceland_sdma_pkt_open.h \
	iceland_smc.c \
	iceland_smumgr.h \
	kv_dpm.h \
	kv_dpm.c \
	kv_smc.c \
	smu_ucode_xfer_vi.h \
	tonga_dpm.c \
	tonga_ih.h \
	tonga_ih.c \
	tonga_sdma_pkt_open.h \
	tonga_smc.c \
	tonga_smum.h \
	uvd_v4_2.h \
	uvd_v4_2.c \
	uvd_v5_0.h \
	uvd_v5_0.c \
	uvd_v6_0.h \
	uvd_v6_0.c \
	vce_v2_0.h \
	vce_v2_0.c \
	vce_v3_0.h \
	vce_v3_0.c \
	vi.h \
	vi.c \
	vi_dpm.h \
	vid.h

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

CWARNFLAGS.amdgpu_acpi.c=	-Wno-int-conversion -Wno-missing-prototypes -Wno-unused-variable
CWARNFLAGS.amdgpu_amdkfd.c=	-Wno-missing-prototypes
CWARNFLAGS.amdgpu_cs.c=	-Wno-missing-prototypes
CWARNFLAGS.amdgpu_device.c=	-Wno-format -Wno-cast-qual
CWARNFLAGS.amdgpu_fence.c=	-Wno-format
CWARNFLAGS.amdgpu_ring.c=	-Wno-cast-qual
CWARNFLAGS.amdgpu_ucode.c=	-Wno-incompatible-pointer-types-discards-qualifiers -Wno-cast-qual
CWARNFLAGS.amdgpu_uvd.c=	-Wno-format
CWARNFLAGS.amdgpu_vce.c=	-Wno-format
CWARNFLAGS.amdgpu_test.c=	-Wno-format
CWARNFLAGS.ci_dpm.c=	-Wno-unused-const-variable
CWARNFLAGS.cz_smc.c=	-Wno-missing-prototypes
CWARNFLAGS.fiji_smc.c=	-Wno-cast-qual
CWARNFLAGS.gfx_v7_0.c=	-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.gfx_v8_0.c=	-Wno-missing-prototypes
CWARNFLAGS.iceland_smc.c=	-Wno-missing-prototypes
CWARNFLAGS.kv_dpm.c=	-Wno-unused-const-variable
CWARNFLAGS.tonga_smc.c=	-Wno-cast-qual
