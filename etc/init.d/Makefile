# $FreeBSD$

PACKAGE=runtime

.include <src.opts.mk>

BINDIR=		/etc/init.d
FILESGROUP=	FILES

FILES=          adjkerntz bootmisc cron devd dumpon encswap fsck hostid hostname ipfw jail \
		ldconfig local localmount loopback mixer modules modules-load moused motd netmount network newsyslog \
		nscd osclock pf powerd rarpd rc-enabled root rpcbind runsvdir s6-svscan savecache savecore \
		sshd staticroute swap swclock syscons sysctl syslogd tmpfiles.setup urandom zfs zvol
FILESMODE=	755

.for fg in ${FILESGROUPS}
${fg}MODE?=	${BINMODE}
.endfor

OPENRC_BOOT_LINK=	adjkerntz \
			bootmisc \
			dumpon \
			fsck \
			hostid \
			hostname \
			localmount \
			loopback \
			modules \
			motd \
			network \
			newsyslog \
			root \
			savecore \
			staticroute \
			swap \
			syscons \
			sysctl \
			syslogd \
			urandom \
			zfs \
			zvol

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

OPENRC_DEFAULT_LINK=	local \
			netmount

OPENRC_SHUTDOWN_LINK=	savecache

OPENRC_NONET_LINK=	local

.include <bsd.prog.mk>
