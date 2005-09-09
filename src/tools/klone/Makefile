# $Id: Makefile,v 1.1.1.1 2005/09/09 15:26:19 tat Exp $
include ../../../Makefile.conf

PROG = klone
SRCS = main.c run.c

CFLAGS += ${LIBSSL_CFLAGS} ${LIBZ_CFLAGS}
LDADD += ${LIBSSL_LDADD} ${LIBZ_LDADD}
LDADD += ../../libtrans/libtrans.a  \
         ../../libio/libio.a        \
         ../../libcodec/libcodec.a  \
         ../../libpage/libpage.a    \
         ../../libutils/libutils.a  \
         ../../libemb/libemb.a      \
         ../../../site/libsite.a


include prog.mk
