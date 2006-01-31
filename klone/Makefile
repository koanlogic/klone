# $Id: Makefile,v 1.12 2006/01/31 17:15:33 tho Exp $

include common.mk
include ../Makefile.conf

INCS = addr.h atom.h backend.h broker.h caes.h cgi.h codec.h                 \
       codecs.h context.h cgzip.h cnull.h emb.h embpage.h field.h header.h   \
       http.h io.h ioprv.h klone.h klog.h klogprv.h md5.h mime_map.h os.h    \
       page.h parser.h ppc.h ppc_cmd.h request.h response.h rsfilter.h run.h \
       server.h ses_prv.h session.h supplier.h timer.h tls.h tlsprv.h        \
       translat.h utils.h var.h varprv.h vars.h version.h

ifdef HAVE_LIBOPENSSL
INCS += ccipher.h
endif

include incs.mk
