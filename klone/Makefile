# $Id: Makefile,v 1.8 2005/10/30 18:09:44 tat Exp $
include ../Makefile.conf

INCS = addr.h backend.h broker.h cgi.h codec.h cgzip.h config.h cnull.h   \
 emb.h embpage.h field.h header.h http.h io.h ioprv.h klone.h md5.h       \
 mime_map.h network.h os.h page.h parser.h paths.h.in queue.h request.h   \
 response.h run.h server.h ses_prv.h session.h str.h supplier.h           \
 sysqueue.h timer.h tls.h tlsprv.h translat.h uri.h utils.h var.h         \
 varprv.h vars.h ppc.h codecs.h caes.h rsfilter.h version.h

ifdef HAVE_LIBOPENSSL
INCS += ccipher.h
endif

include incs.mk
