# $FreeBSD$

PACKAGE=runtime

.include <src.opts.mk>

BINDIR=		/etc/init.d
FILESGROUP=	FILES

FILES=          abi accounting adjkerntz amd auditd auditdistd automount automountd autounmountd \
	       	blacklistd bluetooth bootmisc bridge cron \
	       	devd devfs dumpon encswap fsck ftpd \
	       	gssd hostid hostname ipfw jail \
		ldconfig local localmount lockd loopback mixer \
		modules modules-load mountd moused motd \
		natd netmount network newsyslog nfsclient nfsd nfsuserd \
		nscd osclock pf powerd rarpd rc-enabled root rpcbind runsvdir s6-svscan savecache savecore \
		sshd statd staticroute swap swclock syscons sysctl syslogd tmpfiles.setup urandom zfs zvol
FILESMODE=	755

.for fg in ${FILESGROUPS}
${fg}MODE?=	${BINMODE}
.endfor

.include <bsd.prog.mk>
