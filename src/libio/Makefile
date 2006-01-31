# $Id: Makefile,v 1.5 2006/01/31 17:15:33 tho Exp $

include common.mk
include ../../Makefile.conf

LIB = io
SRCS = io.c iofd.c iomem.c

ifdef HAVE_LIBOPENSSL
SRCS += iossl.c
endif

include lib.mk
