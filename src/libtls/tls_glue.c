/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: tls_glue.c,v 1.14 2008/03/18 17:28:02 tho Exp $
 */

/*
 * This product includes software developed by Ralf S. Engelschall 
 * <rse@engelschall.com> for use in the mod_ssl project (http://www.modssl.org/)
 * 
 * This product includes software developed by the OpenSSL Project
 * for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/io.h>
#include <klone/emb.h>
#include <klone/tlsprv.h>

#ifndef SSL_OPENSSL
int tls_dummy_decl_stub = 0;
#else
#include <openssl/ssl.h>
#include <openssl/x509.h>

/* map an emb resource to a OpenSSL memory BIO */
BIO *bio_from_emb (const char *res_name)
{
    int c;
    char buf[1024];
    io_t *tmp = NULL;
    BIO *b = NULL;

    dbg_return_if (res_name == NULL, NULL);

    dbg_err_if (emb_open(res_name, &tmp));
    dbg_err_if (!(b = BIO_new(BIO_s_mem())));

    for (;;)
    {
        c = io_read(tmp, buf, sizeof buf);

        if (c == 0)     /* EOF */
            break;
        else if (c < 0) /* read error */
            goto err;

        dbg_err_if (BIO_write(b, buf, c) <= 0);
    }

    io_free(tmp);

    return b;
err:
    if (tmp) 
        io_free(tmp);
    if (b)  
        BIO_free(b);

    return NULL;
}

BIO* tls_get_file_bio(const char *res_name)
{
    BIO *b = NULL;

    /* load the cert from the embfs */
    if((b = bio_from_emb(res_name)) != NULL)
        return b;

    /* load the cert from the file system */
    if((b = BIO_new_file(res_name, "r")) != NULL)
        return b;

    /* no cert found */
    return NULL;
}


/* XXX the original returns the number of certs/crls added */
int tls_load_verify_locations (SSL_CTX *c, const char *res_name)
{
    int i;
    BIO *b = NULL;
    STACK_OF(X509_INFO) *info = NULL;

    dbg_return_if (!c, ~0);
    dbg_return_if (!res_name, ~0);

    dbg_err_if (!(b = tls_get_file_bio(res_name)));
    dbg_err_if (!(info = PEM_X509_INFO_read_bio(b, NULL, NULL, NULL)));
    BIO_free(b);

    for (i = 0; i < sk_X509_INFO_num(info); i++)
    {
        X509_INFO   *tmp = sk_X509_INFO_value(info, i);

        if (tmp->x509)
            X509_STORE_add_cert(c->cert_store, tmp->x509); 

        if (tmp->crl)
            X509_STORE_add_crl(c->cert_store, tmp->crl); 
    }

    sk_X509_INFO_pop_free(info, X509_INFO_free);

    return 0;
err:
    if (b)
        BIO_free(b);
    if (info)
        sk_X509_INFO_pop_free(info, X509_INFO_free);

    return ~0;
} 

/* reads certificates from file and returns a STACK_OF(X509_NAME) with 
 * the subject names found */
STACK_OF(X509_NAME) *tls_load_client_CA_file (const char *res_name)
{
    BIO *b = NULL;
    X509 *x = NULL;
    X509_NAME *xn = NULL;
    STACK_OF(X509_NAME) *ret, *sk;

    dbg_return_if (!res_name, NULL);
    
    dbg_err_if (!(ret = sk_X509_NAME_new_null()));
    dbg_err_if (!(sk = sk_X509_NAME_new(X509_NAME_cmp)));
    dbg_err_if (!(b = tls_get_file_bio(res_name)));

    for (;;)
    {
        if (!PEM_read_bio_X509(b, &x, NULL, NULL))
            break;

        dbg_err_if (!(xn = X509_get_subject_name(x)));

        /* check for duplicates */
        dbg_err_if (!(xn = X509_NAME_dup(xn)));
        if (sk_X509_NAME_find(sk, xn) >= 0)
            X509_NAME_free(xn);
        else
        {
            sk_X509_NAME_push(sk, xn);
            sk_X509_NAME_push(ret, xn);
        }
    }

    sk_X509_NAME_free(sk);
    BIO_free(b);
    X509_free(x);

    return ret;
err:
    if (ret)
    {
        sk_X509_NAME_pop_free(ret, X509_NAME_free);
        ret = NULL;
    }
    if (sk)
        sk_X509_NAME_free(sk);
    if (b)
        BIO_free(b);
    if (x)
        X509_free(x);

    return ret;
}

/* basically a wrapper for SSL_CTX_use_certificate() */
int tls_use_certificate_file (SSL_CTX *ctx, const char *res_name, int type)
{
    BIO *b = NULL;
    int ret = 0;
    X509 *x = NULL;

    dbg_return_if (!ctx, 0);
    dbg_return_if (!res_name, 0);
    dbg_return_if (type != SSL_FILETYPE_PEM, 0);

    dbg_goto_if (!(b = tls_get_file_bio(res_name)), end);
    dbg_goto_if (!(x = PEM_read_bio_X509(b, NULL, NULL, NULL)), end);
    ret = SSL_CTX_use_certificate(ctx, x);
    /* fall through */
end:
    if (x)
        X509_free(x);
    if (b)
        BIO_free(b);

    return ret;
}

int tls_use_crls (SSL_CTX *ctx, tls_ctx_args_t *cargs)
{
#if OPENSSL_VERSION_NUMBER >= 0x00907000L
    int count;
    BIO *b = NULL;
    X509_CRL *crl = NULL;
    X509_STORE *store;

    dbg_return_if (cargs == NULL, ~0);
    dbg_return_if (cargs->crl == NULL, ~0);

    /* get X509 STORE i.e. client certificate verify context */
    dbg_err_if ((store = SSL_CTX_get_cert_store(ctx)) == NULL);

    /* read CRL from the resource (embfs or fs) */
    dbg_err_if ((b = tls_get_file_bio(cargs->crl)) == NULL);

    /* get CRLs one by one out of 'cargs->crl' */
    for (count = 0; ; count++)
    {
        crl = PEM_read_bio_X509_CRL(b, NULL, NULL, NULL);

        if (crl == NULL)
        {
            u_long e = ERR_peek_last_error();

            if (ERR_GET_REASON(e) == PEM_R_NO_START_LINE && count > 0)
            {
                ERR_clear_error();
                break;
            } 
            else if (count == 0)
                warn_err("no CRL found in file \'%s\'", cargs->crl);
            else
                warn_err("bad CRL (%d) in file \'%s\'", count, cargs->crl);
        }

        /* add CRL to the verify ctx */
        dbg_err_if (!X509_STORE_add_crl(store, crl));
        X509_CRL_free(crl);
        crl = NULL;
    }
 
    /* say to openssl how we want to check certificate revocation status:
     * every cert in chain or just the client certificate */
    X509_STORE_set_flags(store, cargs->crlopts);

    BIO_free(b);

    return 0;
err:
    if (b)
        BIO_free(b);
    if (crl)
        X509_CRL_free(crl);

    return ~0;
#else
    u_unused_args(ctx, cargs);
    warn("OpenSSL too old (%d): CRL configuration directives won't be honoured",
            OPENSSL_VERSION_NUMBER);
    return 0;
#endif  
}

/* wrapper for SSL_CTX_use_PrivateKey() */
int tls_use_PrivateKey_file (SSL_CTX *ctx, const char *res_name, int type)
{
    int ret = 0;
    BIO *b = NULL;
    EVP_PKEY *pkey = NULL;

    dbg_return_if (!ctx, 0);
    dbg_return_if (!res_name, 0);
    dbg_return_if (type != SSL_FILETYPE_PEM, 0);

    dbg_goto_if (!(b = tls_get_file_bio(res_name)), end);
    dbg_goto_if (!(pkey = PEM_read_bio_PrivateKey(b, NULL, NULL, NULL)), end);
    ret = SSL_CTX_use_PrivateKey(ctx, pkey);
    EVP_PKEY_free(pkey);
    /* fall through */
end:
    if (b)
        BIO_free(b);

    return ret;
}

/* Read a file that optionally contains the server certificate in PEM
 * format, possibly followed by a sequence of CA certificates that
 * should be sent to the peer in the SSL Certificate message.  */
int tls_use_certificate_chain (SSL_CTX *ctx, const char *res_name, 
        int skipfirst, int (*cb)(char *, int, int, void *)) 
{
    BIO *b = NULL;
    X509 *x = NULL;
    unsigned long err;
    int n;

    dbg_return_if (!ctx, -1);
    dbg_return_if (!res_name, -1);

    dbg_err_if (!(b = tls_get_file_bio(res_name)));

    /* optionally skip a leading server certificate */
    if (skipfirst)
    {
        dbg_err_if (!(x = PEM_read_bio_X509(b, NULL, cb, NULL)));
        X509_free(x);
        x = NULL;
    }

    /* free a perhaps already configured extra chain */
    if (!ctx->extra_certs)
    {
        sk_X509_pop_free(ctx->extra_certs, X509_free);
        ctx->extra_certs = NULL;
    }

    /* create new extra chain by loading the certs */
    n = 0;
    while ((x = PEM_read_bio_X509(b, NULL, cb, NULL))) 
    {
        dbg_err_if (!SSL_CTX_add_extra_chain_cert(ctx, x));
        n++;
    }

    /* Make sure that only the error is just an EOF */
    if ((err = ERR_peek_error()) > 0) 
    {
        dbg_err_if (!(ERR_GET_LIB(err) == ERR_LIB_PEM && 
                    ERR_GET_REASON(err) == PEM_R_NO_START_LINE));

        while (ERR_get_error() > 0) ;
    }

    BIO_free(b);

    return n;
err:
    if (b)
        BIO_free(b);
    if (x)
        X509_free(x);

    return -1;
}

#endif /* SSL_OPENSSL */
