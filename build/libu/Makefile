include common.mk
include ../../Makefile.conf

PARTY_NAME = libu

PARTY_NO_DOWN = true
PARTY_NO_CHK = true
PARTY_NO_DECOMP = true
PARTY_NO_INSTALL = true

PARTY_CONF = env MAKL_DIR=${SRCDIR}/makl ./configure
PARTY_BASE = ../../libu
PARTY_DEP = ${PARTY_BASE}/src/libu.a
PARTY_ARGS = --prefix=${DESTDIR}

ifneq ($(strip ${DEFOWN}),)
PARTY_ARGS += --defown=${DEFOWN}
endif

ifneq ($(strip ${DEFGRP}),)
PARTY_ARGS += --defgrp=${DEFGRP}
endif

ifdef LIBU_DEBUG
PARTY_ARGS += --enable_debug
endif

ifdef LIBU_MINI
PARTY_ARGS += --no_net --no_env --no_hmap
endif

include party.mk
