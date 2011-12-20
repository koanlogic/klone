# $Id: Makefile,v 1.4 2007/08/07 14:47:10 tat Exp $ 

include common.mk
include ../Makefile.conf
include Makefile-webapp

LIB = webapp

SRCS = ${WEBAPP_SRCS}

CFLAGS += ${WEBAPP_CFLAGS}
ifdef SSL_ON
CFLAGS := $(SSL_CFLAGS) $(CFLAGS)
endif

LDADD += ${WEBAPP_LDADD}

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
