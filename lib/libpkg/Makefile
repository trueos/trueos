# $FreeBSD$

PACKAGE=lib${LIB}
SHLIBDIR?=      /lib

.include <src.opts.mk>

LIBPKGSRC=	${SRCTOP}/contrib/pkg

.PATH:	${LIBPKGSRC}/libpkg

LIB=	pkg
SHLIB_MAJOR=0

SRCS=	backup.c clean_cache.c diff.c dns_utils.c elfhints.c
SRCS+=	fetch.c merge3.c metalog.c packing.c pkg_add.c pkg_arch.c
SRCS+=	pkg_attributes.c pkg_audit.c pkg_checksum.c pkg_config.c
SRCS+=	pkg_create.c pkg_cudf.c pkg_delete.c pkg_deps.c pkg_elf.c
SRCS+=	pkg_event.c pkg_jobs_conflicts.c pkg_jobs_universe.c pkg_jobs.c
SRCS+=	pkg_manifest.c pkg_object.c pkg_old.c pkg_ports.c
SRCS+=	pkg_printf.c pkg_repo_create.c pkg_repo_meta.c pkg_repo_update.c
SRCS+=	pkg_repo.c pkg_solve.c pkg_status.c pkg_version.c pkg.c pkgdb_iterator.c
SRCS+=	pkgdb_query.c pkgdb.c plugins.c rcscripts.c rsa.c scripts.c sha256.c ssh.c
SRCS+=	utils.c
SRCS+=	../external/sqlite/sqlite3.c ../external/sqlite/shell.c
SRCS+=	../external/picosat/picosat.c ../external/blake2/blake2b-ref.c
SRCS+=	../external/blake2/blake2s-ref.c
SRCS+=	repo/binary/binary.c repo/binary/fetch.c repo/binary/init.c
SRCS+=	repo/binary/query.c repo/binary/update.c repo/binary/common.c
SRCS+=	../compat/dirname.c

CFLAGS+=-I${LIBPKGSRC}/libpkg -I${LIBPKGSRC}/compat -I${.CURDIR}
CFLAGS+=-I${LIBPKGSRC}/external/sqlite -I${LIBPKGSRC}/external/uthash
CFLAGS+=-I${LIBPKGSRC}/external/libucl/include -I${LIBPKGSRC}/external/libucl/klib
CFLAGS+=-I${LIBPKGSRC}/external/expat/lib -I${LIBPKGSRC}/external/blake2
CFLAGS+=-I${LIBPKGSRC}/external/include -I${LIBPKGSRC}/external/picosat
CFLAGS+=-I${LIBPKGSRC}/libpkg/repo/binary -I${LIBPKGSRC}/compat
CFLAGS+=-DHAVE_CONFIG_H

LIBADD+=	sbuf crypto ssl util elf archive jail z bz2 lzma bsdxml

WARNS?= 0

.include <bsd.lib.mk>
