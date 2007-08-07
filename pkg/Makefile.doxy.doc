# $Id: Makefile.doxy.doc,v 1.5 2007/08/07 15:58:52 tat Exp $

include common.mk
include ../../Makefile.conf

HTMLDOCDIR = ${DOCDIR}/klone/html/

all clean depend cleandepend:

ifdef INSTALL_DAEMON_ONLY
install:
else
install:
	${MKINSTALLDIRS} ${HTMLDOCDIR} && cp -r *.html *.css ${HTMLDOCDIR}
endif

uninstall:
	rm -rf ${HTMLDOCDIR}
