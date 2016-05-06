# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/drm2/radeon

### XXX NOT PORTED YET:
#	radeon_agp.c : missing definitions in struct agp_info

KMOD	= radeonkms
SRCS	= \
	atom.c \
	atombios_crtc.c \
	atombios_dp.c \
	atombios_encoders.c \
	atombios_i2c.c \
	btc_dpm.c \
	cayman_blit_shaders.c \
	ci_dpm.c \
	ci_smc.c \
	cik.c \
	cik_blit_shaders.c \
	cik_sdma.c \
	cypress_dpm.c \
	dce3_1_afmt.c \
	dce6_afmt.c \
	evergreen.c \
	evergreen_blit_shaders.c \
	evergreen_cs.c \
	evergreen_dma.c \
	evergreen_hdmi.c \
	kv_dpm.c \
	kv_smc.c \
	ni.c \
	ni_dma.c \
	ni_dpm.c \
	r100.c \
	r200.c \
	r300.c \
	r420.c \
	r520.c \
	r600.c \
	r600_blit_shaders.c \
	r600_cs.c \
	r600_dma.c \
	r600_dpm.c \
	r600_hdmi.c \
	radeon_acpi.c \
	radeon_asic.c \
	radeon_atombios.c \
	radeon_atpx_handler.c \
	radeon_audio.c \
	radeon_benchmark.c \
	radeon_bios.c \
	radeon_clocks.c \
	radeon_combios.c \
	radeon_connectors.c \
	radeon_cs.c \
	radeon_cursor.c \
	radeon_device.c \
	radeon_display.c \
	rs400.c \
	rs600.c \
	rs690.c \
	rs780_dpm.c \
	rv515.c \
	rv6xx_dpm.c \
	rv730_dpm.c \
	rv740_dpm.c \
	rv770.c \
	rv770_dma.c \
	rv770_dpm.c \
	rv770_smc.c \
	si.c \
	si_blit_shaders.c \
	si_dma.c \
	si_dpm.c \
	si_smc.c \
	sumo_dpm.c \
	sumo_smc.c \
	trinity_dpm.c \
	trinity_smc.c \
	uvd_v1_0.c \
	uvd_v2_2.c \
	uvd_v3_1.c \
	uvd_v4_2.c \
	vce_v1_0.c \
	vce_v2_0.c

CFLAGS+= -I${.CURDIR}/../../../compat/linuxkpi/common/include
CFLAGS+= -I${.CURDIR}/../../../compat/linuxkpi/common/include/drm
CFLAGS+= -I${.CURDIR}/../../../compat/linuxkpi/common/include/uapi
CFLAGS+= -I${.CURDIR}/../../../dev/drm2/radeon
CFLAGS+= -I${.CURDIR}/../../../dev/drm2/amd/include
CFLAGS+= '-DKBUILD_MODNAME="${KMOD}"'

SRCS    +=device_if.h vnode_if.h bus_if.h pci_if.h device_if.h iicbus_if.h opt_drm.h \
        opt_vm.h opt_compat.h opt_syscons.h

.include <bsd.kmod.mk>

CWARNFLAGS.atombios_dp.c=	-Wno-format
CWARNFLAGS.atombios_encoders.c=	-Wno-missing-prototypes
CWARNFLAGS.atombios_i2c.c=	-Wno-missing-prototypes
CWARNFLAGS.ci_dpm.c=		-Wno-unused-const-variable
CWARNFLAGS.cik.c=		-Wno-cast-qual -Wno-missing-prototypes
CWARNFLAGS.cik_sdma.c=		-Wno-missing-prototypes
CWARNFLAGS.dce3_1_afmt.c=	-Wno-missing-prototypes
CWARNFLAGS.dce6_afmt.c=		-Wno-missing-prototypes
CWARNFLAGS.evergreen.c=		-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.evergreen_cs.c=	-Wno-format -Wno-missing-prototypes
CWARNFLAGS.evergreen_hdmi.c=	-Wno-missing-prototypes
CWARNFLAGS.kv_dpm.c=		-Wno-unused-const-variable
CWARNFLAGS.ni.c=		-Wno-cast-qual -Wno-missing-prototypes
CWARNFLAGS.ni_dpm.c=		-Wno-missing-prototypes
CWARNFLAGS.r600.c=		-Wno-missing-prototypes -Wno-cast-qual -Wno-format
CWARNFLAGS.r600_cs.c=		-Wno-format -Wno-missing-prototypes
CWARNFLAGS.r600_hdmi.c=		-Wno-missing-prototypes
CWARNFLAGS.radeon_agp.c=	-Wno-format
CWARNFLAGS.radeon_atombios.c=	-Wno-missing-prototypes
CWARNFLAGS.radeon_atpx_handler.c=	-Wno-missing-prototypes
CWARNFLAGS.radeon_clocks.c=	-Wno-missing-prototypes
CWARNFLAGS.radeon_device.c=	-Wno-format -Wno-cast-qual
CWARNFLAGS.radeon_display.c=	-Wno-format
CWARNFLAGS.rv770.c=		-Wno-missing-prototypes -Wno-format
CWARNFLAGS.rv770_dpm.c=		-Wno-missing-prototypes
CWARNFLAGS.si.c=		-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.si_dpm.c=		-Wno-missing-prototypes -Wno-unused-const-variable
CWARNFLAGS.sumo_dpm.c=		-Wno-missing-prototypes
CWARNFLAGS.trinity_dpm.c=	-Wno-unused-const-variable
CWARNFLAGS.vce_v1_0.c=		-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.vce_v2_0.c=		-Wno-missing-prototypes
CWARNFLAGS+=			-Wno-pointer-arith ${CWARNFLAGS.${.IMPSRC:T}}
