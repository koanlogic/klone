# $Id: Makefile,v 1.3 2005/12/30 17:21:53 tat Exp $

# default CFLAGS
include ../Makefile.conf

LIB = site
SRCS = register.c
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
