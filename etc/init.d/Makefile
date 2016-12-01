# $FreeBSD$

PACKAGE=runtime

.include <src.opts.mk>

BINDIR=		/etc/init.d
FILESGROUP=	FILES

FILES=          adjkerntz automount automountd bootmisc cron devd devfs dumpon encswap fsck hostid \
	       	hostname ipfw jail \
		ldconfig local localmount lockd loopback mixer modules modules-load moused motd \
		netmount network newsyslog nfsclient \
		nscd osclock pf powerd rarpd rc-enabled root rpcbind runsvdir s6-svscan savecache savecore \
		sshd statd staticroute swap swclock syscons sysctl syslogd tmpfiles.setup urandom zfs zvol
FILESMODE=	755

.for fg in ${FILESGROUPS}
${fg}MODE?=	${BINMODE}
.endfor

.include <bsd.prog.mk>
