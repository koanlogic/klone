# $Id: Makefile,v 1.3 2005/09/29 12:11:09 tat Exp $
include ../../Makefile.conf

LIB = io
SRCS = io.c iofd.c iomem.c

ifdef HAVE_OPENSSL
SRCS += iossl.c
endif

include lib.mk
