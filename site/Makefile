# $Id: Makefile,v 1.10 2008/10/27 21:28:04 tat Exp $

include common.mk
include ../Makefile.conf
include ../webapp/Makefile-webapp

LIB = site
SRCS = register.c

ifdef SSL_ON
CFLAGS := $(SSL_CFLAGS) $(CFLAGS)
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

CFLAGS += ${WEBAPP_CFLAGS}
CLEANFILES += pg_*.c pg_*.cc pg_*.o pg_*.d pg_*.kld autogen.* register.c libsite.a

# if already set don't change it
KLONE ?= ../src/tools/klone/klone

# hack to support actual autogen.dps
srcdir = .

# update the list of srcs reading from autogen.mk
-include autogen.mk
SRCS += ${autogen_src}

include lib.mk

beforedepend: register.c

register.c: register-def.c
	@cp -f $< $@

autogen.dps:
	@echo > $@

autogen.mk:
	@echo "autogen_src=" > $@

-include autogen.dps
