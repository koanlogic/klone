# $Id: Makefile,v 1.4 2005/10/22 15:54:47 tat Exp $

include ../Makefile.conf

PROG = iocat
SRCS = iocat.c

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

LDADD += ../libu/src/libu.a

LDADD += ../src/libhttp/libhttp.a        \
         ../src/libutils/libutils.a      \
         ../src/libpage/libpage.a        \
         ../src/libbackend/libbackend.a  \
         ../src/libemb/libemb.a          \
         ../src/libio/libio.a            \
         ../src/libtls/libtls.a          \
         ../src/libcodec/libcodec.a      \
         ../site/libsite.a

include prog.mk
