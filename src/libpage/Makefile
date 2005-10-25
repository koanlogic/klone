# $Id: Makefile,v 1.5 2005/10/25 13:33:24 tat Exp $
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
