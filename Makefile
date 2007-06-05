# $Id: Makefile,v 1.12 2007/06/05 08:50:03 tat Exp $

# source top dir 
KLONE_SRC_DIR := $(shell pwd)

# MAKL_DIR is needed in the environment when executing the toolchain target, 
# so it must be exported; MAKEFLAGS is automatically passed by GNU make 
# on recursive calls
export MAKL_DIR := ${KLONE_SRC_DIR}/makl
MAKEFLAGS := -I ${MAKL_DIR}/mk

KLONERC = ${KLONE_SRC_DIR}/.klonerc

SUBDIR = build/libu webapp site src klone contrib doc

include subdir.mk

# deps
webapp site src: build/libu
contrib: src

toolchain: ${MAKL_DIR}/etc/toolchain.mk

${MAKL_DIR}/etc/toolchain.mk:
	${MAKE} -C ${MAKL_DIR} toolchain

env:
	-@cp .klonerc .klonerc.old
	@echo export MAKL_DIR=\"${MAKL_DIR}\"        >  ${KLONERC}
	@echo export MAKEFLAGS=\"-I ${MAKL_DIR}/mk\" >> ${KLONERC}


