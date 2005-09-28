# $Id: Makefile,v 1.1 2005/09/28 09:05:27 tat Exp $

include ../../Makefile.conf

PROG = kloned
SRCS = server.c main.c entry.c modules_init.c modules_term.c

CFLAGS += ${LIBSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBSSL_LDADD} ${LIBZ_LDADD}

LDADD += ../libhttp/libhttp.a ../libutils/libutils.a ../libpage/libpage.a \
         ../libbackend/libbackend.a ../libemb/libemb.a ../libio/libio.a   \
         ../libtls/libtls.a ../../site/libsite.a ../libcodec/libcodec.a

include prog.mk
