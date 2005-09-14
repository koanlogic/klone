# $Id: Makefile,v 1.2 2005/09/14 20:30:35 tat Exp $

include ../../Makefile.conf

PROG = server
SRCS = server.c main.c entry.c

CFLAGS += ${LIBSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBSSL_LDADD} ${LIBZ_LDADD}

LDADD += ../libhttp/libhttp.a ../libutils/libutils.a ../libpage/libpage.a \
         ../libbackend/libbackend.a ../libemb/libemb.a ../libio/libio.a   \
         ../libtls/libtls.a ../../site/libsite.a ../libcodec/libcodec.a

include prog.mk
