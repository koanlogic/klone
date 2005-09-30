# $Id: Makefile,v 1.5 2005/09/30 14:10:29 tat Exp $
include ../../../Makefile.conf

PROG = klone
SRCS = main.c

CFLAGS += ${OPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${OPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}
LDFLAGS += -lc # see checkin 30-09-2005

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
