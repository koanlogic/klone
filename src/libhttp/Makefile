# $Id: Makefile,v 1.7 2008/10/27 21:28:04 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = http
SRCS = broker.c cgi.c http.c sup_emb.c access.c vhost.c dypage.c

ifdef ENABLE_SUP_FS
SRCS += sup_fs.c
endif

ifndef OS_WIN
ifdef ENABLE_SUP_CGI
SRCS += sup_cgi.c
endif
endif

ifdef ENABLE_SUP_KILT
SRCS += sup_kilt.c
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
