# $Id: Makefile,v 1.4 2005/10/04 10:27:57 stewy Exp $
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

ifdef HAVE_LIBOPENSSL
SRCS += ses_client.c
endif

include lib.mk
