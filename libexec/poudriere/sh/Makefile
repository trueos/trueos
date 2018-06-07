# $FreeBSD$

.include <src.opts.mk>

PACKAGE=runtime
PROG=	sh

ORSRC=	${SRCTOP}/contrib/poudriere/src/libexec/poudriere
PTOP=	${SRCTOP}/contrib/poudriere
EXTSRC=	${SRCTOP}/contrib/poudriere/external

.PATH:  ${EXTSRC}/sh_compat
SRCS=	strchrnul.c \
	utimensat.c

.PATH:  ${EXTSRC}/sh
SRCS+=	alias.c \
	arith_yacc.c arith_yylex.c \
	bltin/echo.c cd.c error.c \
	eval.c exec.c expand.c \
	histedit.c input.c jobs.c \
	kill.c mail.c main.c \
	memalloc.c miscbltin.c \
	mystring.c nodes.c \
	options.c output.c \
	parser.c printf.c redir.c \
	show.c syntax.c test.c \
	trap.c var.c

.PATH:  ${ORSRC}
SRCS+=	clock/clock.c \
	dirempty/dirempty.c \
	locked_mkdir/locked_mkdir.c \
	pwait/pwait.c \
	rename/rename.c

.PATH:  ${PTOP}/src/poudriere-sh
SRCS+=	helpers.c mkdir.c \
	mkfifo.c mktemp.c \
	realpath.c rmdir.c \
	setproctitle.c sleep.c \
	touch.c unlink.c

.PATH:  ${SRCTOP}/libexec/poudriere/sh
SRCS+=	builtins.c

CFLAGS+=	-I${SRCTOP}/libexec/poudriere \
		-I${PTOP}/src \
		-I${PTOP}/src/poudriere-sh \
		-I${EXTSRC}/sh \
		-I${SRCTOP}/libexec/poudriere/sh \
		-DSHELL \
		-DNO_HISTORY \
		-DVTABSIZE=1000

WARNS?=	1

BINDIR=/libexec/poudriere
LIBADD=
MAN=

.include <bsd.prog.mk>
