# $Id: Makefile,v 1.6 2007/08/07 14:47:10 tat Exp $

include common.mk
include ../../Makefile.conf

ifdef HAVE_DOXYGEN 

all: img
	doxygen kl1.doxy

img:
	mkdir -p html/images
	cp -r images html

ifdef INSTALL_DAEMON_ONLY
install:
else
install: all
	mkdir -p ${DESTDIR}/doc/klone/ && \
        cp -r html ${DESTDIR}/doc/klone/
endif

uninstall:
	rm -rf ${DESTDIR}/doc/klone/

clean:
	rm -rf html/

else

all clean install uninstall:

endif

depend cleandepend:
