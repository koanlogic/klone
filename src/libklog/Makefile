# $Id: Makefile,v 1.6 2007/08/07 14:47:10 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = klog
SRCS = klog.c mem.c file.c common.c

ifdef HAVE_SYSLOG
SRCS += syslog.c
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
