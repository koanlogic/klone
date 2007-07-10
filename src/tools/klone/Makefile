# $Id: Makefile,v 1.19 2007/07/10 11:41:21 tat Exp $

include common.mk
include ../../../Makefile.conf

PROG = klone
SRCS = main.c pm.c

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

LDADD += ../../../libu/srcs/libu.a

include prog.mk
