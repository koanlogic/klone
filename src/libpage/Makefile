# $Id: Makefile,v 1.2 2005/09/14 20:30:35 tat Exp $
include ../../Makefile.conf

LIB = page
INCS = rsfilter.h  
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

ifdef HAVE_LIBSSL
SRCS += ses_client.c
endif

include lib.mk
