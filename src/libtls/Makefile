# $Id: Makefile,v 1.6 2005/11/23 11:14:02 tho Exp $
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
