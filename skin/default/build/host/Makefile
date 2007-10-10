include common.mk

# reset MaKL platform, we want to compile for the host CPU using KLone's own
# MaKL files
export MAKL_PLATFORM =
export MAKL_TC =
export MAKL_DIR =
export MAKEFLAGS=

XENO_FETCH_URI = http://koanlogic.com/klone/klone-core-$(KLONE_VERSION).tar.gz

XENO_UNZIP_FLAGS = zxf

XENO_NO_CONF = true
XENO_NO_BUILD = true
XENO_NO_INSTALL = true

ifeq ($(KLONE_HOST_PATCH_FILE)$(KLONE_HOST_PATCH_URI),)
XENO_NO_PATCH = true
endif

ifdef KLONE_HOST_PATCH_FILE
XENO_PATCH_FILE = $(KLONE_HOST_PATCH_FILE)
endif

ifdef KLONE_HOST_PATCH_URI
XENO_PATCH_URI = $(KLONE_HOST_PATCH_URI)
endif

include xeno.mk
