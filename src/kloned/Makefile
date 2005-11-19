# $Id: Makefile,v 1.11 2005/11/19 16:28:52 tat Exp $

include ../../Makefile.conf

PROG = kloned
INCS = server_ppc_cmd.h
SRCS = server.c main.c entry.c modules_init.c modules_term.c ppc.c atom.c \
    ppc_log_add.c ppc_nop.c ppc_fork_child.c

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}
LDFLAGS += -lc # see checkin 30-09-2005

LDADD += ../libhttp/libhttp.a ../libutils/libutils.a ../libpage/libpage.a \
         ../libbackend/libbackend.a ../libemb/libemb.a ../libio/libio.a   \
         ../libtls/libtls.a ../../site/libsite.a ../libcodec/libcodec.a   \
         ../libklog/libklog.a

# add libu flags
CFLAGS += -I../../libu
LDADD += ../../libu/src/libu.a

include prog.mk
