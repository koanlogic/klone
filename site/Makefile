# $Id: Makefile,v 1.4 2006/01/10 12:34:37 stewy Exp $

# default CFLAGS
include ../Makefile.conf
include ../webapp/Makefile-webapp

LIB = site
SRCS = register.c
CFLAGS += ${WEBAPP_CFLAGS}
CLEANFILES += *.c *.cc *.a *.o autogen.*

KLONE = ../src/tools/klone/klone

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
	@echo -n > $@

autogen.mk:
	@echo "autogen_src=" > $@

-include autogen.dps
