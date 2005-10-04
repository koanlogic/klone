# $Id: Makefile,v 1.5 2005/10/04 10:27:57 stewy Exp $
include ../../Makefile.conf

LIB = tls
SRCS = tls_glue.c

ifdef HAVE_LIBOPENSSL
SRCS += tls.c tls_dh_autogen.c
endif

include lib.mk

#tls_dh_autogen.c:
#	echo "#include "conf.h"" > $@
#	echo "#ifdef HAVE_LIBOPENSSL" >> $@
#	openssl dhparam 1024 -C -noout >> $@
#	echo "#endif" >> $@
