# $Id: Makefile,v 1.3 2005/09/29 19:11:53 tho Exp $
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

ifdef HAVE_OPENSSL
SRCS += ses_client.c
endif

include lib.mk
