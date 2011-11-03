# $Id: Makefile,v 1.16 2008/10/27 21:28:03 tat Exp $

include common.mk
include ../Makefile.conf

INCDIR := $(INCDIR)/klone/

INCS = atom.h backend.h broker.h caes.h cgi.h codec.h                        \
       codecs.h context.h cgzip.h cnull.h emb.h embpage.h field.h header.h   \
       http.h io.h ioprv.h klone.h klog.h klogprv.h md5.h mime_map.h os.h    \
       page.h parser.h ppc.h ppc_cmd.h request.h response.h rsfilter.h run.h \
       server.h ses_prv.h session.h supplier.h timer.h tls.h tlsprv.h        \
       translat.h utils.h va.h var.h varprv.h vars.h version.h dypage.h

ifdef SSL_ON
INCS += ccipher.h
endif

ifdef INSTALL_DAEMON_ONLY
NO_INSTALL = true
endif

include incs.mk
