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

.include <bsd.prog.mk>
