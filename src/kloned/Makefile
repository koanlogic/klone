# $Id: Makefile,v 1.14 2005/11/24 14:54:01 stewy Exp $

include ../../Makefile.conf
include ../../webapp/Makefile-webapp

PROG = kloned
INCS = server_ppc_cmd.h
SRCS = server.c main.c entry.c modules_init.c modules_term.c ppc.c atom.c \
    ppc_log_add.c ppc_nop.c

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

ifndef OS_WIN
LDFLAGS += -lc # see checkin 30-09-2005
LDFLAGS += ${WEBAPP_LDFLAGS}
SRCS += ppc_fork_child.c
endif

LDADD += ../libhttp/libhttp.a ../libutils/libutils.a ../libpage/libpage.a \
         ../libbackend/libbackend.a ../libemb/libemb.a ../libio/libio.a   \
         ../libtls/libtls.a ../../site/libsite.a ../libcodec/libcodec.a   \
         ../libklog/libklog.a ../../webapp/libwebapp.a 

# add libu flags
CFLAGS += -I../../libu
LDADD += ../../libu/src/libu.a

ifdef OS_WIN
# win libs must be after any lib*.a
POST_LDADD += ${WIN_LDFLAGS}
endif

include prog.mk
