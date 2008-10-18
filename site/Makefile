# $Id: Makefile,v 1.9 2008/10/18 17:23:32 tat Exp $

include common.mk
include ../Makefile.conf
include ../webapp/Makefile-webapp

LIB = site
SRCS = register.c

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

CFLAGS += ${WEBAPP_CFLAGS}
CLEANFILES += *.c *.cc *.a *.o *.d *.kld autogen.*

# if already set don't change it
KLONE ?= ../src/tools/klone/klone

# hack to support actual autogen.dps
srcdir = .

# update the list of srcs reading from autogen.mk
-include autogen.mk
SRCS += ${autogen_src}

include lib.mk

beforedepend: register.c

register.c:
	@echo "void register_pages(){} void unregister_pages(){}" > $@

autogen.dps:
	@echo > $@

autogen.mk:
	@echo "autogen_src=" > $@

-include autogen.dps
