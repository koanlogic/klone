# $Id: Makefile,v 1.2 2005/09/14 20:30:35 tat Exp $
include ../../Makefile.conf

LIB = tls
SRCS = 

ifdef HAVE_LIBSSL
SRCS += tls.c tls_dh_autogen.c tls_glue.c
endif

include lib.mk

#tls_dh_autogen.c:
#	echo "#include "conf.h"" > $@
#	echo "#ifdef HAVE_OPENSSL" >> $@
#	openssl dhparam 1024 -C -noout >> $@
#	echo "#endif" >> $@
