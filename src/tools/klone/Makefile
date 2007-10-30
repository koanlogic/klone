# $Id: Makefile,v 1.21 2007/10/30 17:25:37 tat Exp $

include common.mk
include ../../../Makefile.conf

PROG = klone
SRCS = main.c pm.c

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

LDADD += ../../libtrans/libtrans.a  \
         ../../libcodec/libcodec.a  \
         ../../libutils/libutils.a  \
         ../../libio/libio.a        \
         ../../libcodec/libcodec.a

LDADD += ../../../libu/srcs/libu.a

include prog.mk
