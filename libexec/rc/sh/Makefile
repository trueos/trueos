# $FreeBSD$

SFILES=	contrib/openrc/sh/functions \
	 contrib/openrc/sh/gendepends \
	 contrib/openrc/sh/openrc-run \
	 contrib/openrc/sh/rc-functions

FILES+=	${.CURDIR}/../../../contrib/openrc/sh/rc-mount.sh \
	${.CURDIR}/../../../contrib/openrc/sh/runit.sh \
	${.CURDIR}/../../../contrib/openrc/sh/s6.sh \
	${.CURDIR}/../../../contrib/openrc/sh/start-stop-daemon.sh \
	${.CURDIR}/../../../contrib/openrc/sh/supervise-daemon.sh

.for f in ${SFILES}
${f}: ${f}
	FILES+=${.OBJDIR}/${@}.sh
	sed 's|@SHELL@|/bin/sh|g' ${.CURDIR}/../../../${@}.sh.in >${.OBJDIR}/${@}.sh
	sed -i '' 's|@LIBEXECDIR@|/libexec/rc|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@BINDIR@|/usr/local/bin|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@SBINDIR@|/usr/local/sbin|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@PKG_PREFIX@|/usr/local|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@LOCAL_PREFIX@|/usr/local|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@SYSCONFDIR@|/etc|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@LIB@|/lib|g' ${.OBJDIR}/${@}.sh
.endfor

# Copy the correct init.sh script
.for f in contrib/openrc/sh/init
${f}: ${f}
	FILES+=${.OBJDIR}/${@}.sh
	sed 's|@SHELL@|/bin/sh|g' ${.CURDIR}/../../../${@}.BSD.sh.in >${.OBJDIR}/${@}.sh
	sed -i '' 's|@LIBEXECDIR@|/libexec/rc|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@BINDIR@|/usr/local/bin|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@SBINDIR@|/usr/local/sbin|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@PKG_PREFIX@|/usr/local|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@LOCAL_PREFIX@|/usr/local|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@SYSCONFDIR@|/etc|g' ${.OBJDIR}/${@}.sh
	sed -i '' 's|@LIB@|/lib|g' ${.OBJDIR}/${@}.sh
.endfor

NO_OBJ=
FILESDIR=       /libexec/rc/sh
FILESMODE=      755

.include <bsd.prog.mk>

