# $Id: Makefile,v 1.2 2005/09/29 12:11:09 tat Exp $

include ../../Makefile.conf

PROG = kloned
SRCS = server.c main.c entry.c modules_init.c modules_term.c

CFLAGS += ${OPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${OPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

LDADD += ../libhttp/libhttp.a ../libutils/libutils.a ../libpage/libpage.a \
         ../libbackend/libbackend.a ../libemb/libemb.a ../libio/libio.a   \
         ../libtls/libtls.a ../../site/libsite.a ../libcodec/libcodec.a

include prog.mk
