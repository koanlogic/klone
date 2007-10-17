# $Id: Makefile,v 1.6 2007/10/17 22:58:35 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = utils
SRCS = utils.c md5.c mime_map.c addr.c timer.c date.c version.c path.c

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
