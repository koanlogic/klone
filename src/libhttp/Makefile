# $Id: Makefile,v 1.6 2007/11/09 01:30:45 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = http
SRCS = broker.c cgi.c http.c sup_emb.c access.c vhost.c

ifdef ENABLE_SUP_FS
SRCS += sup_fs.c
endif

ifndef OS_WIN
ifdef ENABLE_SUP_CGI
SRCS += sup_cgi.c
endif
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
