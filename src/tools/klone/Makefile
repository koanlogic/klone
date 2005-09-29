# $Id: Makefile,v 1.4 2005/09/29 12:11:09 tat Exp $
include ../../../Makefile.conf

PROG = klone
SRCS = main.c

CFLAGS += ${OPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${OPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}
LDADD += ../../libtrans/libtrans.a  \
         ../../libio/libio.a        \
         ../../libcodec/libcodec.a  \
         ../../libpage/libpage.a    \
         ../../libutils/libutils.a  \
         ../../libemb/libemb.a      \
         ../../libtls/libtls.a      \
         ../../libhttp/libhttp.a    \
         ../../../site/libsite.a


include prog.mk
