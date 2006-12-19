# $Id: Makefile,v 1.4 2006/12/19 14:41:00 tat Exp $

include common.mk
include ../../../Makefile.conf

PROG = test
SRCS = main.c

CFLAGS += -g
CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}
LDFLAGS += -lc # see checkin 30-09-2005
LDFLAGS += ../../../libu/srcs/libu.a

LDADD += ../libklog.a               \
         ../../libtrans/libtrans.a  \
         ../../libio/libio.a        \
         ../../libcodec/libcodec.a  \
         ../../libpage/libpage.a    \
         ../../libutils/libutils.a  \
         ../../libemb/libemb.a      \
         ../../libtls/libtls.a      \
         ../../libhttp/libhttp.a    \
         ../../../site/libsite.a

include prog.mk
