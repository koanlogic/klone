# Variables to be defined
#
#   KLONE_VERSION  KLone version to use (will be downloaded from koanlogic.com)
#   
#   SUBDIR          (optional) list of additional directories to build on make
#
#   WEBAPP_CFLAGS   (optional) additional CFLAGS
#   WEBAPP_LDFLAGS  (optional) additional LDFLAGS
#   WEBAPP_LDADD    (optional) additional object files and/or static libs (.a)
#                              (requires absolute paths)
#
# Predefined variables:
#
#  KLONE_APP_TOP    top-dir directory (where this Makefile is stored)
#  KLONE_SRC        location of klone source tree
#
-include Makefile.conf

# klone version to use
KLONE_VERSION ?= 2.0.0rc3

# klone configure arguments
KLONE_CONF_ARGS ?=      \
    --prefix=$(DESTDIR) \
    --libs=$(DESTDIR) 

# cross-compiling (see build/makl/tc/* for the list of supported platforms)
# MAKL_PLATFORM ?=

# or provide custom toolchain file
# KLONE_CUSTOM_TC ?=

# helper libs
# SUBDIR ?= 

# web application directory (defaults to webapp/ in the current dir)
# WEBAPP_DIR ?= 

# additional CFLAGS, LDFLAGS and LDADD (optional, use absolute paths for LDADD)
WEBAPP_CFLAGS ?= -I$(KLONE_APP_TOP) -I$(KLONE_SRC) 
WEBAPP_LDADD ?= 
WEBAPP_LDFLAGS ?= 

-include build/mk/klone-app.mk

