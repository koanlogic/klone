/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: tlsprv.h,v 1.13 2008/03/26 09:02:24 tho Exp $
 */

#ifndef _KLONE_TLS_PRV_H_
#define _KLONE_TLS_PRV_H_

#include "klone_conf.h"
#ifdef SSL_ON

#ifdef __cplusplus
extern "C" {
#endif

/* (pseudo) unique data to feed the PRNG */
struct tls_rand_seed_s 
{
    pid_t pid;
    long t1, t2;
    void *stack;
};

typedef struct tls_rand_seed_s tls_rand_seed_t;

/* SSL_CTX initialization parameters.  Mapping of "verify_client" configuration
 * directive to vmode is done in the following way:
 *  "none"      -> SSL_VERIFY_NONE
 *  "optional"  -> SSL_VERIFY_PEER
 *  "require"   -> SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT */
struct tls_ctx_args_s
{
    const char *cert;       /* server certificate file (PEM) */
    const char *key;        /* server private key (PEM) */
    const char *certchain;  /* Server Certificate Authorities (PEM) */
    const char *ca;         /* Client Certification Authorities file (PEM) */
    const char *crl;        /* Certificate Revocation List (PEM) */
    const char *dh;         /* Diffie-Hellman parameters (PEM) */
#ifdef SSL_OPENSSL_PSK
    const char *pskdb;      /* Pre Shared Keys password file */
    int psk_is_hashed;      /* !0 if password is hashed (MD5), 0 if cleartext */
    const char *psk_hint;   /* PSK global hint (may be overridden locally) */
#endif
    int crlopts;            /* CRL check mode: 'all' or 'client-only' */
    int depth;              /* max depth for the cert chain verification */
    int vmode;              /* SSL verification mode */
};

typedef struct tls_ctx_args_s tls_ctx_args_t;

/* used by tls.c */
#ifdef SSL_OPENSSL
DH *get_dh1024 (void);
BIO *bio_from_emb (const char *);
BIO *tls_get_file_bio(const char *res_name);
STACK_OF(X509_NAME) *tls_load_client_CA_file(const char *);
#endif
int tls_load_verify_locations(SSL_CTX *, const char *);
int tls_use_certificate_file(SSL_CTX *, const char *, int);
int tls_use_PrivateKey_file(SSL_CTX *, const char *, int);
int tls_use_certificate_chain(SSL_CTX *, const char *, int, 
        int (*)(char *, int, int, void *));
int tls_use_crls (SSL_CTX *ctx, tls_ctx_args_t *cargs);
int tls_verify_cb (int ok, X509_STORE_CTX *ctx);
char *tls_get_error (void);
#ifdef SSL_OPENSSL_PSK
int tls_psk_init (SSL_CTX *c, tls_ctx_args_t *cargs);
#endif

#ifdef __cplusplus
}
#endif 

#endif /* SSL_ON */
#endif /* _KLONE_TLS_PRV_H_ */
