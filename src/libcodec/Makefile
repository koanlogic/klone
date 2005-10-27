# $Id: Makefile,v 1.3 2005/10/27 14:56:36 tat Exp $
include ../../Makefile.conf

LIB = codec
SRCS = codec.c gzip.c null.c

ifdef HAVE_LIBOPENSSL
SRCS += cipher.c
endif

include lib.mk
