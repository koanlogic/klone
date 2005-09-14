# $Id: Makefile,v 1.2 2005/09/14 20:30:35 tat Exp $
include ../../../Makefile.conf

PROG = klone-site
SRCS = main.c

CFLAGS += ${LIBSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBSSL_LDADD} ${LIBZ_LDADD}
LDADD += ../../libtrans/libtrans.a  \
         ../../libio/libio.a        \
         ../../libcodec/libcodec.a  \
         ../../libpage/libpage.a    \
         ../../libutils/libutils.a  \
         ../../libemb/libemb.a      \
         ../../libhttp/libhttp.a    \
         ../../../site/libsite.a

include prog.mk
