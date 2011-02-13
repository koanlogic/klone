# $Id: Makefile,v 1.5 2007/10/30 17:25:37 tat Exp $

include common.mk
include ../../../Makefile.conf

PROG = test
SRCS = main.c

CFLAGS += -g
CFLAGS += ${SSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${SSL_LDFLAGS} ${LIBZ_LDFLAGS}
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
