# $Id: Makefile,v 1.6 2006/01/31 17:15:33 tho Exp $

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

include lib.mk
