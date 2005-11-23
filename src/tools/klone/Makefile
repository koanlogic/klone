# $Id: Makefile,v 1.9 2005/11/23 18:58:51 tat Exp $
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
         ../../libio/libio.a         

LDADD += ../../../libu/src/libu.a

ifdef OS_WIN
LDADD += ${WIN_LDFLAGS}
endif

include prog.mk
