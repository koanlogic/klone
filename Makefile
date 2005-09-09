# $Id: Makefile,v 1.1 2005/09/09 15:25:42 tat Exp $

SUBDIR = site src klone

# very basic autoconfiguration
MK_CONF = Makefile.conf

#.hier: 
#	@echo "===> creating installation hierarchy" ; \
#	./helpers/hier.sh < helpers/hier.tree
#	@echo "===> DONE"

.conf:
	#@echo "CFLAGS =-Wall" > ${MK_CONF}
	@echo "CFLAGS +=-I`pwd` -I`pwd`/conf" >> ${MK_CONF}
	@echo "export CFLAGS" >> ${MK_CONF}
	#@echo "export DESTDIR = `pwd`/../install" >> ${MK_CONF}
	#@echo "export DEFOWN = `id -un`" >> ${MK_CONF}
	#@echo "export DEFGRP = `id -gn`" >> ${MK_CONF}

include subdir.mk
