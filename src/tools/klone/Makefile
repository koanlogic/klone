# $Id: Makefile,v 1.10 2005/11/23 19:57:24 tat Exp $
include ../../../Makefile.conf

PROG = klone
SRCS = main.c

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

ifndef OS_WIN
LDFLAGS += -lc # see checkin 30-09-2005
endif

LDADD += ../../libtrans/libtrans.a  \
         ../../libcodec/libcodec.a  \
         ../../libutils/libutils.a  \
         ../../libio/libio.a         

LDADD += ../../../libu/src/libu.a

ifdef OS_WIN
# win libs must be after any lib*.a
POST_LDADD += ${WIN_LDFLAGS}
endif

include prog.mk
