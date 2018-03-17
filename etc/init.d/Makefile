# $FreeBSD$

PACKAGE=runtime

.include <src.opts.mk>

BINDIR=		/etc/init.d
FILESGROUP=	FILES

FILES=          abi accounting adjkerntz amd auditd auditdistd automount automountd autounmountd \
		blacklistd bluetooth bootmisc bridge cron \
		devd devfs dumpon earlykld encswap fsck ftpd ftp-proxy \
		gssd hostid hostname inetd iscsictl iscsid ipfw jail \
		ldconfig local localmount local_unbound lockd loopback lpd hostapd \
		mixer modules mountd mountlate moused motd \
		natd net-online netmount network newsyslog nfsclient nfsd nfsuserd nisdomain nsswitch \
		nscd ntpdate pf pflog powerd ppp rarpd rc-enabled resolv root routing rpcbind runsvdir \
		s6-svscan savecache savecore sendmail \
		sshd statd staticroute swap syscons sysctl syslogd tmpfiles.setup urandom \
		wpa_supplicant \
		ypbind ypldap yppasswdd ypserv ypset ypupdated ypxfrd \
		zfs zvol
FILESMODE=	755

.for fg in ${FILESGROUPS}
${fg}MODE?=	${BINMODE}
.endfor

.include <bsd.prog.mk>
