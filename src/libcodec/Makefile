# $Id: Makefile,v 1.4 2005/11/23 09:59:42 tho Exp $
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
