# $FreeBSD$

CONFGROUPS=	CONFETC CONFETCEXEC CONFETCDEFAULTS CONFPRIV
CONFETCDIR=	/etc
CONFETC+=	network.subr openrc openrc.shutdown rc rc.bsdextended rc.dynamicdiskless
CONFETC+=	rc.initdiskless rc.subr rc.shutdown
CONFETC+=	group login.access termcap.small
CONFETCMODE=	644
CONFETCEXEC=	netstart pccard_ether rc.devd rc.resume rc.suspend pkgbase.setup
CONFETCEXECDIR=	/etc
CONFETCEXECMODE=	755
CONFETCDEFAULTSDIR=	/etc/defaults
CONFETCDEFAULTS=	rc.conf
CONFPRIV+=	master.passwd
CONFPRIVDIR=	/etc
CONFPRIVMODE=	600
#PACKAGE=rc

SUBDIR+=	bin
SUBDIR+=	gss
SUBDIR+=	init.d
SUBDIR+=	pkg
SUBDIR+=	rc.d
SUBDIR+=	sbin
SUBDIR+=	sh
SUBDIR+=	ssl

# Needs to be added after sh
SUBDIR+=	etc.init.d

.include <bsd.prog.mk>
