# $Id: Makefile,v 1.5 2007/07/07 09:13:57 tat Exp $

include common.mk
include ../../Makefile.conf

ifdef HAVE_DOXYGEN 

all: img
	doxygen kl1.doxy

img:
	mkdir -p html/images
	cp -r images html

install: all
	mkdir -p ${DESTDIR}/doc/klone/ && \
        cp -r html ${DESTDIR}/doc/klone/

uninstall:
	rm -rf ${DESTDIR}/doc/klone/

clean:
	rm -rf html/

else

all clean install uninstall:

endif

depend cleandepend:
