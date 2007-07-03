# $Id: Makefile,v 1.4 2007/07/03 11:10:09 tat Exp $

include common.mk
include ../../Makefile.conf

ifdef HAVE_DOXYGEN 

all: img
	doxygen kl1.doxy

img:
	mkdir -p html/images
	cp -r images html

install: all
	mkdir -p ${RELOC}${DESTDIR}/doc/klone/ && \
        cp -r html ${RELOC}${DESTDIR}/doc/klone/

uninstall:
	rm -rf ${RELOC}${DESTDIR}/doc/klone/

clean:
	rm -rf html/

else

all clean install uninstall:

endif

depend cleandepend:
