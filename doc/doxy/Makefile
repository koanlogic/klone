# $Id: Makefile,v 1.3 2006/01/31 17:15:33 tho Exp $

include common.mk
include ../../Makefile.conf

ifdef HAVE_DOXYGEN 

all: img
	doxygen kl1.doxy

img:
	mkdir -p html/images
	cp -r images html

install: all
	mkdir -p ${DESTDIR}/doc/klone/ && cp -r html ${DESTDIR}/doc/klone/

uninstall:
	rm -rf ${DESTDIR}/doc/klone/

clean:
	rm -rf html/

else

all clean install uninstall:

endif

depend cleandepend:
