# $Id: Makefile,v 1.1 2007/09/04 12:15:16 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = hook
SRCS = hook.c server_nfy.c child_nfy.c req.c

ifdef SSL_ON
CFLAGS := $(SSL_CFLAGS) $(CFLAGS)
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
