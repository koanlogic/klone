include common.mk

XENO_FETCH_URI = http://koanlogic.com/klone/klone-core-$(KLONE_VERSION).tar.bz2

XENO_UNZIP_FLAGS = jxf

XENO_NO_CONF = true
XENO_NO_BUILD = true
XENO_NO_INSTALL = true

ifeq ($(KLONE_TARGET_PATCH_FILE)$(KLONE_TARGET_PATCH_URI),)
XENO_NO_PATCH = true
endif

ifdef KLONE_TARGET_PATCH_FILE
XENO_PATCH_FILE = $(KLONE_TARGET_PATCH_FILE)
endif

ifdef KLONE_TARGET_PATCH_URI
XENO_PATCH_URI = $(KLONE_TARGET_PATCH_URI)
endif

include xeno.mk
