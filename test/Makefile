# $Id: Makefile,v 1.1 2007/08/20 16:06:08 tat Exp $

include common.mk
include ../Makefile.conf

PROG = runtests
SRCS = main.c misc.c

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
    ../src/libpage/libpage.a 

LDADD += ../libu/srcs/libu.a
LDFLAGS += -lssl -lcrypto -lz

include prog.mk

