# $Id: Makefile,v 1.7 2008/03/18 17:28:02 tho Exp $

include common.mk
include ../../Makefile.conf

LIB = utils
SRCS = utils.c md5.c mime_map.c timer.c date.c version.c path.c pwd.c

ifdef SSL_ON
CFLAGS := $(SSL_CFLAGS) $(CFLAGS)
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
