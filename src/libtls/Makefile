# $Id: Makefile,v 1.1.1.1 2005/09/09 15:26:15 tat Exp $
include ../../Makefile.conf

LIB = tls
SRCS = tls_glue.c tls_dh_autogen.c tls.c

include lib.mk

#tls_dh_autogen.c:
#	echo "#include "conf.h"" > $@
#	echo "#ifdef HAVE_OPENSSL" >> $@
#	openssl dhparam 1024 -C -noout >> $@
#	echo "#endif" >> $@
