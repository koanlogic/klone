# $Id: Makefile,v 1.5 2006/01/31 17:15:33 tho Exp $

include common.mk
include ../../Makefile.conf

LIB = codec
SRCS = codec.c null.c

ifdef HAVE_LIBZ
SRCS += gzip.c
endif

ifdef HAVE_LIBOPENSSL
SRCS += cipher.c
endif

include lib.mk
