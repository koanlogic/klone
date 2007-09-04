# $Id: Makefile,v 1.8 2007/09/04 12:15:16 tat Exp $

SUBDIR =    \
 libutils   \
 libbackend \
 libcodec   \
 libemb     \
 libhttp    \
 libio      \
 libpage    \
 libtls     \
 libtrans   \
 libklog    \
 libhook    \
 tools      \
 kloned 

include subdir.mk

# dir dependencies
kloned tools: $(filter-out kloned tools, $(SUBDIR))
