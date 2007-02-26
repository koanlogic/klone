# $Id: Makefile,v 1.7 2007/02/26 21:43:29 tat Exp $

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
 tools      \
 kloned 

include subdir.mk

# dir dependencies
kloned tools: $(filter-out kloned tools, $(SUBDIR))
