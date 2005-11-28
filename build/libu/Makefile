all: party

PARTY_BASE = ../../libu
PARTY_DEP = ${PARTY_BASE}/src/libu.a
PARTY_ARGS = -v --defown=${DEFOWN} --defgrp=${DEFGRP} --prefix=${DESTDIR}
PARTY_NO_INSTALL = true

include party.mk
include ../../Makefile.conf
