# $FreeBSD$

.PATH:	${.CURDIR}/../../../../dev/drm2/amd/amdgpu ${.CURDIR}/../../../../dev/drm2/amd/scheduler ${.CURDIR}/../../../../dev/drm2/amd/powerplay ${.CURDIR}/../../../../dev/drm2/amd/powerplay/eventmgr ${.CURDIR}/../../../../dev/drm2/amd/powerplay/hwmgr ${.CURDIR}/../../../../dev/drm2/amd/powerplay/smumgr

KMOD	= amdgpu

# XXX we do not enable this yet
#       amdgpu_acp.h \
#       amdgpu_acp.c
# XXX not ported yet
#	gpu_scheduler.c : kfifo
# XXX not enabled:
#	amdgpu_mn.c 

SRCS	= \
	amdgpu.h \
        amdgpu_acpi.c \
        amdgpu_afmt.c \
        amdgpu_amdkfd.c \
	amdgpu_amdkfd_gfx_v7.c \
	amdgpu_amdkfd_gfx_v8.c \
	amdgpu_atombios.c \
	amdgpu_atpx_handler.c \
	amdgpu_benchmark.c \
	amdgpu_bios.c \
	amdgpu_bo_list.c \
	amdgpu_connectors.c \
	amdgpu_cs.c \
	amdgpu_ctx.c \
	amdgpu_device.c \
	amdgpu_display.c \
	amdgpu_dpm.c \
	amdgpu_drv.c \
	amdgpu_encoders.c \
	amdgpu_fb.c \
	amdgpu_fence.c \
	amdgpu_freebsd.c \
	amdgpu_gart.c \
	amdgpu_gem.c \
	amdgpu_gfx.c \
	amdgpu_i2c.c \
	amdgpu_ib.c \
	amdgpu_ih.c \
	amdgpu_ioc32.c \
	amdgpu_irq.c \
	amdgpu_job.c \
	amdgpu_kms.c \
	amdgpu_object.c \
	amdgpu_pll.c \
	amdgpu_pm.c \
	amdgpu_powerplay.c \
	amdgpu_prime.c \
	amdgpu_ring.c \
	amdgpu_sa.c \
	amdgpu_sync.c \
	amdgpu_test.c \
	amdgpu_trace_points.c \
	amdgpu_ttm.c \
	amdgpu_ucode.c \
	amdgpu_uvd.c \
	amdgpu_vce.c \
	amdgpu_vm.c \
	atom.c \
	atombios_crtc.c \
	atombios_dp.c \
	atombios_encoders.c \
	atombios_i2c.c \
	ci_dpm.c \
	ci_smc.c \
	cik.c \
	cik_ih.c \
	cik_sdma.c \
	cz_dpm.c \
        cz_ih.c \
        cz_smc.c \
	dce_v10_0.c \
	dce_v11_0.c \
	dce_v8_0.c \
	fiji_dpm.c \
	fiji_smc.c \
	gfx_v7_0.c \
	gfx_v8_0.c \
	gmc_v7_0.c \
	gmc_v8_0.c \
	iceland_dpm.c \
	iceland_ih.c \
	iceland_smc.c \
	kv_dpm.c \
	kv_smc.c \
	sdma_v2_4.c \
	sdma_v3_0.c \
	tonga_dpm.c \
	tonga_ih.c \
	tonga_smc.c \
	uvd_v4_2.c \
	uvd_v5_0.c \
	uvd_v6_0.c \
	vce_v2_0.c \
	vce_v3_0.c \
	vi.c \
	sched_fence.c \
	amd_powerplay.c \
	eventactionchains.c \
	eventinit.c \
	eventmanagement.c \
	eventmgr.c \
	eventsubchains.c \
	eventtasks.c \
	psm.c \
	cz_clockpowergating.c \
	cz_hwmgr.c \
	fiji_clockpowergating.c \
	fiji_hwmgr.c \
	fiji_powertune.c \
	fiji_thermal.c \
	functiontables.c \
	gpu_scheduler.c \
	hardwaremanager.c \
	hwmgr.c \
	pp_acpi.c \
	ppatomctrl.c \
	pppcielanes.c \
	processpptables.c \
	tonga_clockpowergating.c \
	tonga_hwmgr.c \
	tonga_processpptables.c \
	tonga_thermal.c \
	smumgr.c \
	cz_smumgr.c \
	tonga_smumgr.c \
	fiji_smumgr.c


CFLAGS+= 	-I${.CURDIR}/../../../../contrib/ck/include
CFLAGS+=	-I${.CURDIR}/../../../../compat/linuxkpi/common/include
CFLAGS+=	-I${.CURDIR}/../../../../compat/linuxkpi/common/include/drm
CFLAGS+=	-I${.CURDIR}/../../../../compat/linuxkpi/common/include/uapi
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/amdgpu
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/include
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/acp/include
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/powerplay/eventmgr
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/powerplay/hwmgr
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/powerplay/inc
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/powerplay/smumgr
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/scheduler
CFLAGS+=	-I${.CURDIR}/../../../../dev/drm2/amd/include/asic_reg
CFLAGS+=	'-DKBUILD_MODNAME="${KMOD}"'
#CFLAGS+=	-O0
CFLAGS+=	-include ${.CURDIR}/../../../../dev/drm2/drm_os_config.h


SRCS	+=device_if.h vnode_if.h bus_if.h pci_if.h device_if.h iicbus_if.h opt_drm.h \
        opt_vm.h opt_compat.h opt_syscons.h

.include <bsd.kmod.mk>

CWARNFLAGS+=	-Wno-pointer-arith -Wno-format
CWARNFLAGS+=	-Wno-pointer-sign ${CWARNFLAGS.${.IMPSRC:T}}

CWARNFLAGS.amdgpu_acpi.c=	-Wno-int-conversion -Wno-missing-prototypes -Wno-unused-variable
CWARNFLAGS.amdgpu_amdkfd.c=	-Wno-missing-prototypes
CWARNFLAGS.amdgpu_bo_list.c=	-Wno-missing-prototypes
CWARNFLAGS.amdgpu_cs.c=	-Wno-missing-prototypes
CWARNFLAGS.amdgpu_device.c=	 -Wno-cast-qual
CWARNFLAGS.amdgpu_gfx.c=	-Wno-missing-prototypes
CWARNFLAGS.amdgpu_amdkfd_gfx_v7.c=	-Wno-cast-qual
CWARNFLAGS.amdgpu_amdkfd_gfx_v8.c=	-Wno-cast-qual
CWARNFLAGS.amdgpu_atpx_handler.c=	-Wno-missing-prototypes
CWARNFLAGS.amdgpu_ih.c=	-Wno-cast-qual
CWARNFLAGS.amdgpu_ioc32.c=	-Wno-missing-prototypes
CWARNFLAGS.amdgpu_mn.c=		-Wno-unused-variable
CWARNFLAGS.amdgpu_pll.c=	-Wno-missing-prototypes
CWARNFLAGS.amdgpu_pm.c=		-Wno-missing-prototypes -Wno-enum-conversion
CWARNFLAGS.amdgpu_ring.c=	-Wno-cast-qual
CWARNFLAGS.amdgpu_ttm.c=	-Wno-missing-prototypes
CWARNFLAGS.amdgpu_ucode.c=	-Wno-incompatible-pointer-types-discards-qualifiers -Wno-cast-qual
CWARNFLAGS.atombios_crtc.c=	-Wno-missing-prototypes
CWARNFLAGS.atombios_i2c.c=	-Wno-missing-prototypes
CWARNFLAGS.ci_dpm.c=	-Wno-unused-const-variable
CWARNFLAGS.cz_smc.c=	-Wno-missing-prototypes
CWARNFLAGS.fiji_smc.c=	-Wno-cast-qual
CWARNFLAGS.gfx_v7_0.c=	-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.gfx_v8_0.c=	-Wno-missing-prototypes
CWARNFLAGS.iceland_smc.c=	-Wno-missing-prototypes
CWARNFLAGS.kv_dpm.c=	-Wno-unused-const-variable
CWARNFLAGS.tonga_smc.c=	-Wno-cast-qual
CWARNFLAGS.gpu_scheduler.c=	-Wno-missing-prototypes
CWARNFLAGS.amd_powerplay.c=	-Wno-missing-prototypes
CWARNFLAGS.eventtasks.c=	-Wno-missing-prototypes
CWARNFLAGS.cz_clockpowergating.c=	-Wno-missing-prototypes -Wno-enum-conversion
CWARNFLAGS.cz_hwmgr.c=	-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.fiji_hwmgr.c=	-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.fiji_thermal.c=	-Wno-missing-prototypes
CWARNFLAGS.pp_acpi.c=	-Wno-missing-prototypes 
CWARNFLAGS.ppatomctrl.c=	-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.processpptables.c=	-Wno-missing-prototypes -Wno-sometimes-uninitialized
CWARNFLAGS.tonga_clockpowergating.c=	-Wno-missing-prototypes -Wno-enum-conversion
CWARNFLAGS.tonga_hwmgr.c=	-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.tonga_processpptables.c=	-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.tonga_thermal.c=	-Wno-missing-prototypes
CWARNFLAGS.tonga_smumgr.c=	-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.fiji_smumgr.c=	-Wno-missing-prototypes -Wno-cast-qual
