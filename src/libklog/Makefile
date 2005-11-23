# $Id: Makefile,v 1.4 2005/11/23 18:58:51 tat Exp $
include ../../Makefile.conf

LIB = klog
SRCS = klog.c mem.c file.c common.c

ifdef HAVE_SYSLOG
SRCS += syslog.c
endif

include lib.mk
