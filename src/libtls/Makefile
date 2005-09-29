# $Id: Makefile,v 1.3 2005/09/29 12:11:09 tat Exp $
include ../../Makefile.conf

LIB = tls
SRCS = 

ifdef HAVE_OPENSSL
SRCS += tls.c tls_dh_autogen.c tls_glue.c
endif

include lib.mk

#tls_dh_autogen.c:
#	echo "#include "conf.h"" > $@
#	echo "#ifdef HAVE_OPENSSL" >> $@
#	openssl dhparam 1024 -C -noout >> $@
#	echo "#endif" >> $@
