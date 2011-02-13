# $Id: Makefile,v 1.10 2008/03/18 17:28:02 tho Exp $

include common.mk
include ../../Makefile.conf

LIB = tls
CFLAGS += $(SSL_CFLAGS)

ifdef SSL_CYASSL
SRCS = tls.c cyassl.c
endif

ifdef SSL_OPENSSL
SRCS = tls_glue.c tls.c tls_dh_autogen.c
endif

ifdef SSL_OPENSSL_PSK
SRCS += tls_psk.c
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk

#tls_dh_autogen.c:
#	echo "#include "klone_conf.h"" > $@
#	echo "#ifdef SSL_OPENSSL" >> $@
#	openssl dhparam 1024 -C -noout >> $@
#	echo "#endif" >> $@
