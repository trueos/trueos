#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

PROG=	jq

SRCDIR=		${SRCTOP}/contrib/jq

.PATH:	${SRCDIR}

WARNS?= 0       # some warn issues on 32 bit machines

SRCS+=  locfile.c bytecode.c compile.c execute.c builtin.c jv.c \
	jv_parse.c jv_print.c jv_dtoa.c jv_unicode.c jv_aux.c jv_file.c \
	jv_alloc.c jq_test.c util.c linker.c main.c
SRCS+=	jq_parser.h builtin.h bytecode.h compile.h exec_stack.h \
	libm.h jv_alloc.h jv_dtoa.h jv_unicode.h locfile.h \
	opcode_list.h jv_utf8_tables.h lexer.l util.h linker.h \
	version.h

CFLAGS+=-I${SRCDIR}
CFLAGS+=-DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 \
	-DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 \
	-DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 \
	-DHAVE_UNISTD_H=1 -DHAVE_DLFCN_H=1 \
	-DYYTEXT_POINTER=1 -DHAVE_MEMMEM=1 -DHAVE_MKSTEMP=1 \
	-DHAVE_ALLOCA=1 -DHAVE_ISATTY=1 -DHAVE_STRPTIME=1 -DHAVE_STRFTIME=1 \
	-DHAVE_TIMEGM=1 -DHAVE_GMTIME_R=1 -DHAVE_GMTIME=1 -DHAVE_GETTIMEOFDAY=1 \
	-DHAVE_TM_TM_GMT_OFF=1 -DHAVE_ACOS=1 -DHAVE_ACOSH=1 -DHAVE_ASIN=1 \
	-DHAVE_ASINH=1 -DHAVE_ATAN=1 -DHAVE_ATANH=1 -DHAVE_CBRT=1 -DHAVE_COS=1 \
	-DHAVE_COSH=1 -DHAVE_EXP2=1 -DHAVE_EXP=1 -DHAVE_FLOOR=1 -DHAVE_J0=1 \
	-DHAVE_LIBM=1 -DHAVE_J1=1 -DHAVE_LIBM=1 -DHAVE_LOG10=1 -DHAVE_LOG2=1 \
	-DHAVE_LOG=1 -DHAVE_SIN=1 -DHAVE_SINH=1 -DHAVE_SQRT=1 -DHAVE_TAN=1 \
	-DHAVE_TANH=1 -DHAVE_TGAMMA=1 -DHAVE_Y0=1 -DHAVE_LIBM=1 -DHAVE_Y1=1 \
	-DHAVE_LIBM=1 -DHAVE_POW=1 -DHAVE_ATAN2=1 -DHAVE_HYPOT=1 \
	-DHAVE_REMAINDER=1 -DHAVE___THREAD=1 -DIEEE_8087=1
LDFLAGS+=-lm -ljq -L${.OBJDIR}/../../lib/libjq

CSTD= gnu99

MAN=	jq.1

.include <bsd.prog.mk>
