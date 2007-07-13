# $Id: Makefile,v 1.4 2007/07/13 23:08:24 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = http
SRCS = broker.c cgi.c http.c sup_emb.c 

ifdef ENABLE_SUP_FS
SRCS += sup_fs.c
endif

ifndef OS_WIN
ifdef ENABLE_SUP_CGI
SRCS += sup_cgi.c
endif
endif

include lib.mk
