# $Id: Makefile,v 1.1 2005/09/09 15:26:16 tat Exp $

include ../../Makefile.conf

PROG = server
SRCS = server.c main.c entry.c

CFLAGS += ${LIBSSL_CFLAGS} ${LIBZ_CFLAGS}
LDADD += ${LIBSSL_LDADD} ${LIBZ_LDADD}

LDADD += ../libhttp/libhttp.a ../libutils/libutils.a ../libpage/libpage.a \
         ../libbackend/libbackend.a ../libemb/libemb.a ../libio/libio.a   \
         ../libtls/libtls.a ../../site/libsite.a ../libcodec/libcodec.a

include prog.mk
