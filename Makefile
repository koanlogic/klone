# $Id: Makefile,v 1.11 2006/02/01 09:39:12 tho Exp $

KLONE_SRC_DIR := $(shell pwd)

# MAKL_DIR is needed in the environment when executing the toolchain target, 
# so it must be exported.
export MAKL_DIR := ${KLONE_SRC_DIR}/makl
# MAKEFLAGS is automatically passed by GNU make on recursive calls
MAKEFLAGS := -I ${MAKL_DIR}/mk

all clean depend cleandepend install uninstall: toolchain
	${MAKE} -f Makefile.subdir ${MAKECMDGOALS}

KLONERC = ${KLONE_SRC_DIR}/.klonerc

env:
	-@cp .klonerc .klonerc.old
	@echo export MAKL_DIR=\"${MAKL_DIR}\"        >  ${KLONERC}
	@echo export MAKEFLAGS=\"-I ${MAKL_DIR}/mk\" >> ${KLONERC}

toolchain: ${MAKL_DIR}/etc/toolchain.mk

${MAKL_DIR}/etc/toolchain.mk:
	${MAKE} -C ${MAKL_DIR} toolchain
