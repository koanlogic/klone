# $Id: Makefile,v 1.4 2005/09/30 13:35:19 tat Exp $
include ../../Makefile.conf

LIB = tls
SRCS = tls_glue.c

ifdef HAVE_OPENSSL
SRCS += tls.c tls_dh_autogen.c
endif

include lib.mk

#tls_dh_autogen.c:
#	echo "#include "conf.h"" > $@
#	echo "#ifdef HAVE_OPENSSL" >> $@
#	openssl dhparam 1024 -C -noout >> $@
#	echo "#endif" >> $@
