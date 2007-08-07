# $Id: Makefile,v 1.8 2007/08/07 14:47:10 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = tls
SRCS = tls_glue.c

ifdef HAVE_LIBOPENSSL
SRCS += tls.c tls_dh_autogen.c
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk

#tls_dh_autogen.c:
#	echo "#include "klone_conf.h"" > $@
#	echo "#ifdef HAVE_LIBOPENSSL" >> $@
#	openssl dhparam 1024 -C -noout >> $@
#	echo "#endif" >> $@
