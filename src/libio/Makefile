# $Id: Makefile,v 1.4 2005/10/04 10:27:57 stewy Exp $
include ../../Makefile.conf

LIB = io
SRCS = io.c iofd.c iomem.c

ifdef HAVE_LIBOPENSSL
SRCS += iossl.c
endif

include lib.mk
