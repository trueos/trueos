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
		gbde geli geli2 gptboot growfs gssd \
		hastd hcsecd hostid hostid_save hostname \
		inetd iovctl ipfs ipropd_master ipropd_slave iscsictl iscsid ipfw ipfw_netflow ipsec \
		jail kadmind kdc keyserv kfd kpasswdd ldapd \
		ldconfig local localmount local_unbound lockd lpd hostapd \
		mixer modules mountd mountlate moused motd \
		natd netmount network newsyslog nfscbd nfsclient nfsd nfsuserd nisdomain nsswitch \
		nscd ntpd ntpdate pf pflog powerd ppp rarpd resolv rwho root routing rpcbind runsvdir \
		s6-svscan savecache savecore sdpd sendmail \
		sshd statd staticroute swap syscons sysctl syslogd timed tmp tmpfiles.setup ubthidhci urandom \
		var watchdogd wpa_supplicant \
		ypbind ypldap yppasswdd ypserv ypset ypupdated ypxfrd \
		zfs zfsd zvol
FILESMODE=	755

.for fg in ${FILESGROUPS}
${fg}MODE?=	${BINMODE}
.endfor

.if ${MK_POUDRIERE} != "no"
FILES+= poudriered
.endif

.include <bsd.prog.mk>
