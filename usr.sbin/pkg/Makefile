# $FreeBSD$

PROG=	pkg

LIBPKGSRC=	${SRCTOP}/contrib/pkg
.PATH:	${LIBPKGSRC}/src

SRCS= add.c alias.c annotate.c audit.c autoremove.c backup.c
SRCS+= check.c clean.c config.c convert.c create.c delete.c
SRCS+= event.c fetch.c globals.c info.c install.c lock.c main.c
SRCS+= plugins.c query.c register.c repo.c rquery.c search.c set.c
SRCS+= shell.c shlib.c ssh.c stats.c update.c updating.c upgrade.c
SRCS+= utils.c version.c which.c

CFLAGS+=-I${LIBPKGSRC}/libpkg -I${LIBPKGSRC}/compat -I${.CURDIR}
CFLAGS+=-I${LIBPKGSRC}/external/sqlite -I${LIBPKGSRC}/external/uthash
CFLAGS+=-I${LIBPKGSRC}/external/libucl/include -I${LIBPKGSRC}/external/libucl/klib
CFLAGS+=-I${LIBPKGSRC}/external/expat/lib -I${LIBPKGSRC}/external/blake2
CFLAGS+=-I${LIBPKGSRC}/external/include -I${LIBPKGSRC}/external/picosat
CFLAGS+=-DHAVE_CONFIG_H

WARNS?= 0

CFLAGS+=-I${SRCTOP}/contrib/libucl/include -lutil -ljail -lpkg -lelf
.PATH:	${SRCTOP}/contrib/libucl/include
LIBADD=	archive pkg fetch ucl sbuf crypto ssl

MAN=	${LIBPKGSRC}/docs/pkg_printf.3
MAN+=	${LIBPKGSRC}/docs/pkg_repos.3
MAN+=	${LIBPKGSRC}/docs/pkg.8
MAN+=	${LIBPKGSRC}/docs/pkg-add.8
MAN+=	${LIBPKGSRC}/docs/pkg-alias.8
MAN+=	${LIBPKGSRC}/docs/pkg-annotate.8
MAN+=	${LIBPKGSRC}/docs/pkg-audit.8
MAN+=	${LIBPKGSRC}/docs/pkg-autoremove.8
MAN+=	${LIBPKGSRC}/docs/pkg-backup.8
MAN+=	${LIBPKGSRC}/docs/pkg-check.8
MAN+=	${LIBPKGSRC}/docs/pkg-clean.8
MAN+=	${LIBPKGSRC}/docs/pkg-config.8
MAN+=	${LIBPKGSRC}/docs/pkg-convert.8
MAN+=	${LIBPKGSRC}/docs/pkg-create.8
MAN+=	${LIBPKGSRC}/docs/pkg-delete.8
MAN+=	${LIBPKGSRC}/docs/pkg-fetch.8
MAN+=	${LIBPKGSRC}/docs/pkg-info.8
MAN+=	${LIBPKGSRC}/docs/pkg-install.8
MAN+=	${LIBPKGSRC}/docs/pkg-lock.8
MAN+=	${LIBPKGSRC}/docs/pkg-query.8
MAN+=	${LIBPKGSRC}/docs/pkg-register.8
MAN+=	${LIBPKGSRC}/docs/pkg-repo.8
MAN+=	${LIBPKGSRC}/docs/pkg-repository.5
MAN+=	${LIBPKGSRC}/docs/pkg-rquery.8
MAN+=	${LIBPKGSRC}/docs/pkg-search.8
MAN+=	${LIBPKGSRC}/docs/pkg-set.8
MAN+=	${LIBPKGSRC}/docs/pkg-shell.8
MAN+=	${LIBPKGSRC}/docs/pkg-shlib.8
MAN+=	${LIBPKGSRC}/docs/pkg-ssh.8
MAN+=	${LIBPKGSRC}/docs/pkg-stats.8
MAN+=	${LIBPKGSRC}/docs/pkg-update.8
MAN+=	${LIBPKGSRC}/docs/pkg-updating.8
MAN+=	${LIBPKGSRC}/docs/pkg-upgrade.8
MAN+=	${LIBPKGSRC}/docs/pkg-version.8
MAN+=	${LIBPKGSRC}/docs/pkg-which.8
MAN+=	${LIBPKGSRC}/docs/pkg.conf.5

.include <bsd.prog.mk>
