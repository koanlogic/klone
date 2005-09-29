# $Id: Makefile,v 1.2 2005/09/29 12:11:09 tat Exp $

include ../Makefile.conf

PROG = iocat
SRCS = iocat.c

CFLAGS += ${OPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${OPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

LDADD +=../src/libhttp/libhttp.a        \
        ../src/libutils/libutils.a      \
        ../src/libpage/libpage.a        \
        ../src/libbackend/libbackend.a  \
        ../src/libemb/libemb.a          \
        ../src/libio/libio.a            \
        ../src/libtls/libtls.a          \
        ../src/libcodec/libcodec.a      \
        ../site/libsite.a

include prog.mk
