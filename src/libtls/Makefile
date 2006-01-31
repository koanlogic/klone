# $Id: Makefile,v 1.7 2006/01/31 17:15:33 tho Exp $

include common.mk
include ../../Makefile.conf

LIB = tls
SRCS = tls_glue.c

ifdef HAVE_LIBOPENSSL
SRCS += tls.c tls_dh_autogen.c
endif

include lib.mk

#tls_dh_autogen.c:
#	echo "#include "klone_conf.h"" > $@
#	echo "#ifdef HAVE_LIBOPENSSL" >> $@
#	openssl dhparam 1024 -C -noout >> $@
#	echo "#endif" >> $@
