include ../../Makefile.conf

all: party

PARTY_BASE = ../../libu
PARTY_DEP = ${PARTY_BASE}/src/libu.a
PARTY_ARGS = -v --prefix=${DESTDIR}
ifneq ($(strip ${DEFOWN}),)
PARTY_ARGS += --defown=${DEFOWN}
endif
ifneq ($(strip ${DEFGRP}),)
PARTY_ARGS += --defgrp=${DEFGRP}
endif
PARTY_NO_INSTALL = true

include party.mk
