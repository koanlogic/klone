# $Id: Makefile,v 1.4 2006/01/31 17:15:33 tho Exp $

include common.mk
include ../../Makefile.conf

PROG = iowc
SRCS = iowc.c

CFLAGS += ${LIBOPENSSL_CFLAGS} ${LIBZ_CFLAGS}
LDFLAGS += ${LIBOPENSSL_LDFLAGS} ${LIBZ_LDFLAGS}

LDADD += ../../src/libhttp/libhttp.a        \
         ../../src/libutils/libutils.a      \
         ../../src/libpage/libpage.a        \
         ../../src/libbackend/libbackend.a  \
         ../../src/libemb/libemb.a          \
         ../../src/libio/libio.a            \
         ../../src/libtls/libtls.a          \
         ../../src/libcodec/libcodec.a      \
         ../../site/libsite.a

LDADD += ../../libu/src/libu.a

ifdef OS_WIN
# windows libs must be after any lib*.a
POST_LDADD += ${WIN_LDFLAGS}
endif

include prog.mk
