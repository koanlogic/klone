/* $Id: tls.h,v 1.2 2005/09/14 20:18:31 tat Exp $ */

#ifndef _KLONE_TLS_H_
#define _KLONE_TLS_H_

#include "conf.h"
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>

/* (pseudo) unique data to feed the PRNG */
struct tls_rand_seed_s 
{
    pid_t   pid;
    long    t1, t2;
    void    *stack;
};

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
    const char *dh;         /* Diffie-Hellman parameters (PEM) */
    int         depth;      /* max depth for the cert chain verification */
    int         vmode;      /* SSL verification mode */
};

typedef struct tls_rand_seed_s tls_rand_seed_t;
typedef struct tls_ctx_args_s tls_ctx_args_t;


SSL_CTX *tls_init_ctx (tls_ctx_args_t *);
int     tls_load_ctx_args(config_t *, tls_ctx_args_t **);
char    *tls_get_error (void);

#endif /* HAVE_OPENSSL */

#endif /* !_KLONE_TLS_H */
