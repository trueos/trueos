# $FreeBSD$

PACKAGE=runtime

.include <src.opts.mk>

BINDIR=		/etc/init.d
FILESGROUP=	FILES

FILES=          abi accounting adjkerntz amd apm apmd archdep auditd auditdistd automount \
		automountd autounmountd \
		bgfsck blacklistd bluetooth bootmisc bootparams bridge bsnmpd bthidd \
		ccd cfumass cleanvar cleartmp cron ctld \
		ddb devd devfs devmatch dhcpcd dumpon \
		earlykld encswap fsck ftpd ftp-proxy \
		gbde geli geli2 gssd hcsecd hostid hostname inetd iscsictl iscsid ipfw jail \
		ldconfig local localmount local_unbound lockd lpd hostapd \
		mixer modules mountd mountlate moused motd \
		natd netmount network newsyslog nfsclient nfsd nfsuserd nisdomain nsswitch \
		nscd ntpd ntpdate pf pflog powerd ppp rarpd rc-enabled resolv root routing rpcbind runsvdir \
		s6-svscan savecache savecore sdpd sendmail \
		sshd statd staticroute swap syscons sysctl syslogd tmp tmpfiles.setup urandom \
		var wpa_supplicant \
		ypbind ypldap yppasswdd ypserv ypset ypupdated ypxfrd \
		zfs zvol
FILESMODE=	755

.for fg in ${FILESGROUPS}
${fg}MODE?=	${BINMODE}
.endfor

.if ${MK_POUDRIERE} != "no"
FILES+= poudriered
.endif

.include <bsd.prog.mk>
