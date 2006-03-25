# $Id: Makefile,v 1.14 2006/03/25 13:06:42 tat Exp $

include common.mk
include ../../../Makefile.conf

PROG = klone
SRCS = main.c

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

ifndef OS_WIN
LDFLAGS += -lc # see checkin 30-09-2005
endif

LDADD += ../../libtrans/libtrans.a  \
         ../../libcodec/libcodec.a  \
         ../../libutils/libutils.a  \
         ../../libio/libio.a        \
         ../../libcodec/libcodec.a

LDADD += ../../../libu/src/libu.a

include prog.mk
