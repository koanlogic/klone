/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: tlsprv.h,v 1.8 2007/06/26 08:57:57 tat Exp $
 */

#ifndef _KLONE_TLS_PRV_H_
#define _KLONE_TLS_PRV_H_

#include "klone_conf.h"
#ifdef  HAVE_LIBOPENSSL

#ifdef __cplusplus
extern "C" {
#endif

/* used by tls.c */
DH  *get_dh1024 (void);
BIO *bio_from_emb (const char *);
int SSL_CTX_use_certificate_chain (SSL_CTX *, const char *, int, int (*)());
int tls_load_verify_locations(SSL_CTX *, const char *);
int tls_use_certificate_file(SSL_CTX *, const char *, int);
int tls_use_PrivateKey_file(SSL_CTX *, const char *, int);
int SSL_CTX_use_certificate_chain_file(SSL_CTX *, const char *);
int tls_use_certificate_chain(SSL_CTX *, const char *, int, int (*)(void));
BIO* tls_get_file_bio(const char *res_name);

STACK_OF(X509_NAME) *tls_load_client_CA_file(const char *);

#ifdef __cplusplus
}
#endif 

#endif /* HAVE_LIBOPENSSL */
#endif /* _KLONE_TLS_PRV_H_ */
