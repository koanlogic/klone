# $Id: Makefile,v 1.3 2007/07/13 14:00:13 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = http
SRCS = broker.c cgi.c http.c sup_emb.c 

ifdef ENABLE_SUP_FS
SRCS += sup_fs.c
endif

ifdef ENABLE_SUP_CGI
SRCS += sup_cgi.c
endif

include lib.mk
