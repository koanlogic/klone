# $Id: Makefile,v 1.7 2007/08/07 14:47:10 tat Exp $

include common.mk
include ../../Makefile.conf

LIB = page
SRCS =          \
    field.c     \
    header.c    \
    request.c   \
    response.c  \
    rsfilter.c  \
    ses_file.c  \
    ses_mem.c   \
    session.c   \
    var.c       \
    vars.c  

ifdef HAVE_LIBOPENSSL
SRCS += ses_client.c
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include lib.mk
