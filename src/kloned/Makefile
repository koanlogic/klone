# $Id: Makefile,v 1.30 2007/10/30 17:25:37 tat Exp $

include common.mk
include ../../Makefile.conf
include ../../webapp/Makefile-webapp

ifdef DAEMON_NAME
PROG = $(DAEMON_NAME)
else
PROG = kloned
endif

INCS = main.h server_s.h server_ppc_cmd.h child.h
SRCS = server.c main.c entry.c ppc.c atom.c ppc_log_add.c ppc_log_get.c \
    ppc_nop.c child.c

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

LDFLAGS += ${WEBAPP_LDFLAGS}

ifdef OS_UNIX
SRCS += ppc_fork_child.c
endif

LDADD += ../libhttp/libhttp.a ../libutils/libutils.a ../libpage/libpage.a   \
         ../libbackend/libbackend.a ../libemb/libemb.a ../libio/libio.a     \
         ../libtls/libtls.a ../../site/libsite.a ../libcodec/libcodec.a     \
         ../libklog/libklog.a ../../webapp/libwebapp.a ../libhttp/libhttp.a \
         ../libpage/libpage.a ../libutils/libutils.a ../libio/libio.a       \
         ../libtls/libtls.a ../libcodec/libcodec.a ../libpage/libpage.a     \
         ../libhook/libhook.a \
         ${WEBAPP_LDADD}

# add libu flags
CFLAGS += -I../../libu/include
LDADD += ../../libu/srcs/libu.a

BINDIR = ${DESTDIR}/sbin

ifdef MAKL_PLATFORM
# 'install' may not be able to strip cross compiled binaries
INSTALL_STRIP = 
endif

include prog.mk

strip: $(PROG)
	@$(STRIP) $(PROG)

ifdef DAEMON_NAME
# always create "kloned" for backward compatibility
all-hook-post:
	@ln -sf $(DAEMON_NAME) kloned
endif

