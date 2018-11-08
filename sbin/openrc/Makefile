# $FreeBSD$

PACKAGE=runtime
PROG=	openrc
LINKS=	${BINDIR}/openrc ${BINDIR}/rc

OPENRC=	${.CURDIR}/../../contrib/openrc

RC=	${OPENRC}/src/rc
EINFO=	${OPENRC}/src/libeinfo
LIBRC=	${OPENRC}/src/librc
OLIBRC=	${.OBJDIR}/../../lib/librc/contrib/openrc/src/librc
INC=	${OPENRC}/src/includes

SRCS=	rc.c rc-logger.c rc-misc.c rc-plugin.c _usage.c rc-logger.h
CFLAGS+= -I${RC} -I${EINFO} -I${LIBRC} -I${OLIBRC} -I${INC}

WARNS?=	3
MAN=	openrc.8

LIBADD=	einfo rc util

.PATH: ${RC}
.PATH: ${OPENRC}/man

SUBDIR+=	conf.d
SUBDIR+=	local.d
SUBDIR+=	sysctl.d

OPENRC_BOOT_LINK=	abi \
			adjkerntz \
			bootmisc \
			bridge \
			cron \
			devd \
			dmesg \
			dumpon \
			fsck \
			hostid \
			hostname \
			kldxref \
			localmount \
			modules \
			motd \
			network \
			newsyslog \
			root \
			routing \
			savecore \
			staticroute \
			swap \
			syscons \
			sysctl \
			syslogd \
			urandom \
			zfs \
			zvol

OPENRC_DEFAULT_LINK=	local \
			netmount

OPENRC_SHUTDOWN_LINK=	savecache

OPENRC_NONET_LINK=	local

afterinstall:
	if [ ! -e ${DESTDIR}/etc/init.d/functions.sh ]; then \
		${INSTALL_SYMLINK} /libexec/rc/sh/functions.sh ${DESTDIR}/etc/init.d/functions.sh; \
	fi
	if [ ! -d ${DESTDIR}/etc/runlevels/boot ]; then \
		mkdir -p ${DESTDIR}/etc/runlevels/boot; \
	fi
	if [ ! -d ${DESTDIR}/etc/runlevels/default ]; then \
		mkdir ${DESTDIR}/etc/runlevels/default; \
	fi
	if [ ! -d ${DESTDIR}/etc/runlevels/nonetwork ]; then \
		mkdir ${DESTDIR}/etc/runlevels/nonetwork; \
	fi
	if [ ! -d ${DESTDIR}/etc/runlevels/shutdown ]; then \
		mkdir ${DESTDIR}/etc/runlevels/shutdown; \
	fi
	if [ ! -d ${DESTDIR}/etc/runlevels/sysinit ]; then \
		mkdir ${DESTDIR}/etc/runlevels/sysinit; \
	fi
	for rlink in ${OPENRC_BOOT_LINK}; do \
		${INSTALL_SYMLINK} /etc/init.d/$${rlink} \
		    ${DESTDIR}/etc/runlevels/boot/$${rlink}; \
	done
	for rlink in ${OPENRC_DEFAULT_LINK}; do \
		${INSTALL_SYMLINK} /etc/init.d/$${rlink} \
		    ${DESTDIR}/etc/runlevels/default/$${rlink}; \
	done
	for rlink in ${OPENRC_NONET_LINK}; do \
		${INSTALL_SYMLINK} /etc/init.d/$${rlink} \
		    ${DESTDIR}/etc/runlevels/nonetwork/$${rlink}; \
	done
	for rlink in ${OPENRC_SHUTDOWN_LINK}; do \
		${INSTALL_SYMLINK} /etc/init.d/$${rlink} \
		    ${DESTDIR}/etc/runlevels/shutdown/$${rlink}; \
	done

.include <bsd.prog.mk>
