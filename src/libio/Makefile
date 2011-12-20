# $Id: Makefile,v 1.6 2007/08/07 14:47:10 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = io
SRCS = io.c iofd.c iomem.c

ifdef SSL_ON
CFLAGS := $(SSL_CFLAGS) $(CFLAGS)
SRCS += iossl.c
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
