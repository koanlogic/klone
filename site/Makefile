# $Id: Makefile,v 1.2 2005/11/28 12:52:52 tho Exp $

# default CFLAGS
include ../Makefile.conf

LIB = site
SRCS = register.c
CLEANFILES += *.c *.a *.o autogen.*

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
