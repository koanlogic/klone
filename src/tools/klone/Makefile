# $Id: Makefile,v 1.8 2005/10/11 14:22:53 tat Exp $
include ../../../Makefile.conf

PROG = klone
SRCS = main.c

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}
LDFLAGS += -lc # see checkin 30-09-2005

LDADD += ../../libtrans/libtrans.a  \
         ../../libio/libio.a        \
         ../../libcodec/libcodec.a  \
         ../../libutils/libutils.a  

LDADD += ../../../libu/src/libu.a



include prog.mk
