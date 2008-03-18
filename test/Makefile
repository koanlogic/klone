# $Id: Makefile,v 1.4 2008/03/18 22:48:51 tho Exp $

include common.mk
include ../Makefile.conf

PROG = runtests
SRCS = main.c misc.c path.c

# add libu flags
CFLAGS += -I../libu/include
LDADD += ../src/libhttp/libhttp.a ../src/libutils/libutils.a \
    ../src/libpage/libpage.a  ../src/libbackend/libbackend.a \
    ../src/libemb/libemb.a ../src/libio/libio.a     \
    ../src/libtls/libtls.a \
    ../src/libcodec/libcodec.a ../src/libklog/libklog.a \
    ../src/libhttp/libhttp.a ../src/libpage/libpage.a \
    ../src/libutils/libutils.a ../src/libio/libio.a \
    ../src/libtls/libtls.a ../src/libcodec/libcodec.a \
    ../src/libpage/libpage.a ../site/libsite.a

LDADD += ../libu/srcs/libu.a
CFLAGS += $(LIBOPENSSL_CFLAGS) $(LIBZ_CFLAGS)
LDFLAGS += $(LIBOPENSSL_LDFLAGS) $(LIBZ_LDFLAGS)

include prog.mk

