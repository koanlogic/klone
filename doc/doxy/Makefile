# $Id: Makefile,v 1.6 2007/08/07 14:47:10 tat Exp $

include common.mk
include ../../Makefile.conf

DOXYDEST = $(DESTDIR)/doc/klone/

ifdef HAVE_DOXYGEN 

all:
	$(PATH_DOXYGEN) kl1.doxy
	mkdir -p html/images
	cp -r images html

ifdef INSTALL_DAEMON_ONLY
install:
else    # !INSTALL_DAEMON_ONLY
install: all
	$(MKINSTALLDIRS) -p $(DOXYDEST)
	cp -r html $(DOXYDEST)
endif   # INSTALL_DAEMON_ONLY

uninstall:  ; rm -rf $(DOXYDEST)
clean:      ; rm -rf html/

else    # !HAVE_DOXYGEN

all clean install uninstall:

endif   # HAVE_DOXYGEN

depend cleandepend:
