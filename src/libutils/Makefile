# $Id: Makefile,v 1.5 2007/08/07 14:47:10 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = utils
SRCS = utils.c md5.c mime_map.c addr.c timer.c date.c version.c

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
