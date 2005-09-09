#ifndef _KLONE_TLS_PRV_H_
#define _KLONE_TLS_PRV_H_
#include "conf.h"
#ifdef  HAVE_OPENSSL

/* used by tls.c */
DH  *get_dh1024 (void);
BIO *bio_from_emb (const char *);
int SSL_CTX_use_certificate_chain (SSL_CTX *, const char *, int, int (*)());
int tls_load_verify_locations(SSL_CTX *, const char *);
int tls_use_certificate_file(SSL_CTX *, const char *, int);
int tls_use_PrivateKey_file(SSL_CTX *, const char *, int);
int SSL_CTX_use_certificate_chain_file(SSL_CTX *, const char *);
int tls_use_certificate_chain(SSL_CTX *, const char *, int, int (*)(void));
STACK_OF(X509_NAME) *tls_load_client_CA_file(const char *);

#endif /* HAVE_OPENSSL */
#endif /* _KLONE_TLS_PRV_H_ */
