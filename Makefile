# source top dir 
KLONE_SRC_DIR := $(shell pwd)

# makl vars
export MAKL_DIR := ${KLONE_SRC_DIR}/makl
MAKEFLAGS := -I ${MAKL_DIR}/mk

SUBDIR = build/libu webapp site src klone contrib doc

include $(MAKL_DIR)/mk/subdir.mk

# deps
webapp site src: build/libu
contrib: src
