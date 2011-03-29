# $Id: Makefile,v 1.8 2007/09/04 12:15:16 tat Exp $

SUBDIR  = libutils
SUBDIR += libbackend
SUBDIR += libcodec
SUBDIR += libemb
SUBDIR += libhttp
SUBDIR += libio
SUBDIR += libpage
ifdef SSL_ON
SUBDIR += libtls
endif
SUBDIR += libtrans
SUBDIR += libklog
SUBDIR += libhook
SUBDIR += tools
SUBDIR += kloned

include subdir.mk

# dir dependencies
kloned tools: $(filter-out kloned tools, $(SUBDIR))
