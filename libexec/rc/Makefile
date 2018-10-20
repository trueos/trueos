# $FreeBSD$

CONFGROUPS=	CONFETC CONFETCEXEC CONFETCDEFAULTS
CONFETCDIR=	/etc
CONFETC+=	network.subr openrc openrc.shutdown rc rc.devd rc.dynamicdiskless
CONFETC+=	rc.initdiskless rc.subr rc.shutdown
CONFETCMODE=	644
CONFETCEXEC=	netstart pccard_ether rc.resume rc.suspend
CONFETCEXECDIR=	/etc
CONFETCEXECMODE=	755
CONFETCDEFAULTSDIR=	/etc/defaults
CONFETCDEFAULTS=	rc.conf
#PACKAGE=rc

SUBDIR+=	bin
SUBDIR+=	etc.init.d
SUBDIR+=	init.d
SUBDIR+=	rc.d
SUBDIR+=	sbin
SUBDIR+=	sh

.include <bsd.prog.mk>
