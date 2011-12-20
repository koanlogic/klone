# $Id: Makefile,v 1.6 2007/08/07 14:47:10 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = codec
SRCS = codec.c null.c

ifdef HAVE_LIBZ
SRCS += gzip.c
endif

ifdef SSL_ON
CFLAGS := $(SSL_CFLAGS) $(CFLAGS)
endif

ifdef SSL_OPENSSL
SRCS += cipher.c
endif

ifdef SSL_CYASSL
SRCS += cipher_cyassl.c
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
