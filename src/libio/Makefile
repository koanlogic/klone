# $Id: Makefile,v 1.2 2005/09/14 20:30:35 tat Exp $
include ../../Makefile.conf

LIB = io
SRCS = io.c iofd.c iomem.c

ifdef HAVE_LIBSSL
SRCS += iossl.c
endif

include lib.mk
