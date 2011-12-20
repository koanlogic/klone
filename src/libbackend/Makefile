# $Id: Makefile,v 1.3 2007/08/07 14:47:10 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = backend
SRCS = backend.c

ifdef SSL_ON
CFLAGS := $(SSL_CFLAGS) $(CFLAGS)
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
