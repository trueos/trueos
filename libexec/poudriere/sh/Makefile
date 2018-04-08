# $FreeBSD$

PACKAGE=runtime
PROG=	sh

.include <src.opts.mk>

ORSRC=	${SRCTOP}/contrib/poudriere/src/libexec/poudriere
PTOP=	${SRCTOP}/contrib/poudriere
EXTSRC=	${SRCTOP}/contrib/poudriere/external

.PATH:  ${ORSRC}/sh

SRCS=	${EXTSRC}/sh_compat/strchrnul.c \
	${EXTSRC}/sh_compat/utimensat.c ${EXTSRC}/sh/alias.c \
	${EXTSRC}/sh/arith_yacc.c ${EXTSRC}/sh/arith_yylex.c \
	${EXTSRC}/sh/bltin/echo.c ${EXTSRC}/sh/cd.c ${EXTSRC}/sh/error.c \
	${EXTSRC}/sh/eval.c ${EXTSRC}/sh/exec.c ${EXTSRC}/sh/expand.c \
	${EXTSRC}/sh/histedit.c ${EXTSRC}/sh/input.c ${EXTSRC}/sh/jobs.c \
	${EXTSRC}/sh/kill.c ${EXTSRC}/sh/mail.c ${EXTSRC}/sh/main.c \
	${EXTSRC}/sh/memalloc.c ${EXTSRC}/sh/miscbltin.c \
	${EXTSRC}/sh/mystring.c ${EXTSRC}/sh/nodes.c \
	${EXTSRC}/sh/options.c ${EXTSRC}/sh/output.c \
	${EXTSRC}/sh/parser.c ${EXTSRC}/sh/printf.c ${EXTSRC}/sh/redir.c \
	${EXTSRC}/sh/show.c ${EXTSRC}/sh/syntax.c ${EXTSRC}/sh/test.c \
	${EXTSRC}/sh/trap.c ${EXTSRC}/sh/var.c \
	${ORSRC}/clock/clock.c \
	${ORSRC}/dirempty/dirempty.c \
	${ORSRC}/locked_mkdir/locked_mkdir.c \
	${ORSRC}/pwait/pwait.c \
	${ORSRC}/rename/rename.c \
	${PTOP}/src/poudriere-sh/helpers.c ${PTOP}/src/poudriere-sh/mkdir.c \
	${PTOP}/src/poudriere-sh/mkfifo.c ${PTOP}/src/poudriere-sh/mktemp.c \
	${PTOP}/src/poudriere-sh/realpath.c ${PTOP}/src/poudriere-sh/rmdir.c \
	${PTOP}/src/poudriere-sh/setproctitle.c ${PTOP}/src/poudriere-sh/sleep.c \
	${PTOP}/src/poudriere-sh/touch.c ${PTOP}/src/poudriere-sh/unlink.c \
	${SRCTOP}/libexec/poudriere/sh/builtins.c
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
