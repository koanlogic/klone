# $Id: Makefile,v 1.22 2006/03/25 13:06:42 tat Exp $

include common.mk
include ../../Makefile.conf
include ../../webapp/Makefile-webapp

PROG = kloned
INCS = main.h server_s.h server_ppc_cmd.h child.h
SRCS = server.c main.c entry.c modules_init.c modules_term.c ppc.c atom.c   \
    ppc_log_add.c ppc_log_get.c ppc_nop.c child.c

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

ifndef OS_WIN
LDFLAGS += -lc # see checkin 30-09-2005
LDFLAGS += ${WEBAPP_LDFLAGS}
SRCS += ppc_fork_child.c
endif

LDADD += ../libhttp/libhttp.a ../libutils/libutils.a ../libpage/libpage.a   \
         ../libbackend/libbackend.a ../libemb/libemb.a ../libio/libio.a     \
         ../libtls/libtls.a ../../site/libsite.a ../libcodec/libcodec.a     \
         ../libklog/libklog.a ../../webapp/libwebapp.a ../libhttp/libhttp.a \
         ../libpage/libpage.a ../libutils/libutils.a ${WEBAPP_LDADD}

# add libu flags
CFLAGS += -I../../libu
LDADD += ../../libu/src/libu.a

BINDIR = ${DESTDIR}/sbin

include prog.mk

strip: $(PROG)
	@$(STRIP) $(PROG)
