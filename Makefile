# $Id: Makefile,v 1.9 2006/01/23 10:03:16 tho Exp $

include Makefile.conf

MAKEFLAGS := -I ${SRCDIR}/makl/mk

all clean depend cleandepend install uninstall:
	${MAKE} -f Makefile.subdir ${MAKECMDGOALS}

env:
	-@cp .klonerc .klonerc.old
	@echo export MAKL_DIR=\"${SRCDIR}/makl\" > .klonerc
	@echo export MAKEFLAGS=\"-I ${SRCDIR}/makl/mk\" >> .klonerc
