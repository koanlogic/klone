# $Id: Makefile,v 1.6 2006/12/19 14:41:00 tat Exp $

include common.mk
include ../../Makefile.conf

PROG = iocat
SRCS = iocat.c

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

LDADD += ../../src/libhttp/libhttp.a        \
         ../../src/libutils/libutils.a      \
         ../../src/libpage/libpage.a        \
         ../../src/libbackend/libbackend.a  \
         ../../src/libemb/libemb.a          \
         ../../src/libio/libio.a            \
         ../../src/libtls/libtls.a          \
         ../../src/libcodec/libcodec.a      \
         ../../site/libsite.a

LDADD += ../../libu/srcs/libu.a

include prog.mk
