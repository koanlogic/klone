# $Id: Makefile,v 1.5 2006/01/31 17:15:33 tho Exp $

include common.mk
include ../../Makefile.conf

LIB = klog
SRCS = klog.c mem.c file.c common.c

ifdef HAVE_SYSLOG
SRCS += syslog.c
endif

include lib.mk
