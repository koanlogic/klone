# $Id: Makefile,v 1.1 2005/09/21 15:15:25 tat Exp $

include ../Makefile.conf

PROG = iocat
SRCS = iocat.c

CFLAGS += ${LIBSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBSSL_LDADD} ${LIBZ_LDADD}

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
