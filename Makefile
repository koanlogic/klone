# $Id: Makefile,v 1.10 2006/01/23 11:22:19 tho Exp $

KLONE_SRC_DIR := $(shell pwd)

export MAKL_DIR := ${KLONE_SRC_DIR}/makl
MAKEFLAGS := -I ${KLONE_SRC_DIR}/makl/mk

all clean depend cleandepend install uninstall: toolchain
	${MAKE} -f Makefile.subdir ${MAKECMDGOALS}

env:
	-@cp .klonerc .klonerc.old
	@echo export MAKL_DIR=\"${KLONE_SRC_DIR}/makl\" > .klonerc
	@echo export MAKEFLAGS=\"-I ${KLONE_SRC_DIR}/makl/mk\" >> .klonerc

toolchain: ${KLONE_SRC_DIR}/makl/etc/toolchain.mk

${KLONE_SRC_DIR}/makl/etc/toolchain.mk:
	${MAKE} -C ${KLONE_SRC_DIR}/makl toolchain
