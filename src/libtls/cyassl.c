/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 */
#include "klone_conf.h"
#include <u/libu.h>
#include <klone/io.h>
#include <klone/emb.h>
#include <klone/tlsprv.h>

#ifndef SSL_CYASSL
int tls_dummy_decl_stub = 0;
#else
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/opensslv.h>

static int tls_fsfile_to_ubuf(const char *filename, u_buf_t **pubuf)
{
    u_buf_t *ubuf = NULL;

    dbg_err_if(filename == NULL);
    dbg_err_if(pubuf == NULL);

    dbg_err_if(u_buf_create(&ubuf));

    dbg_err_if(u_buf_load(ubuf, filename));

    *pubuf = ubuf;

    return 0;
err:
    if(ubuf)
        u_buf_free(ubuf);
    return ~0;
}

static int tls_file_to_ubuf(const char *filename, u_buf_t **pubuf)
{
    dbg_err_if(filename == NULL);
    dbg_err_if(pubuf == NULL);

    if(emb_to_ubuf(filename, pubuf) == 0)
        return 0;

    if(tls_fsfile_to_ubuf(filename, pubuf) == 0)
        return 0;

err:
    return ~0; 
}

int tls_load_verify_locations (SSL_CTX *c, const char *res_name)
{
    u_buf_t *ubuf = NULL;

    dbg_err_if(c == NULL);
    dbg_err_if(res_name == NULL);

    dbg_err_if(tls_file_to_ubuf(res_name, &ubuf));

    dbg_err_if( CyaSSL_CTX_load_verify_buffer(c, u_buf_ptr(ubuf), 
                    u_buf_len(ubuf), SSL_FILETYPE_PEM) != SSL_SUCCESS);

    u_buf_free(ubuf); ubuf = NULL;

    return 0;
err:
    if(ubuf)
        u_buf_free(ubuf);
    return ~0;
} 

int tls_use_certificate_file (SSL_CTX *ctx, const char *res_name, int type)
{
    u_buf_t *ubuf = NULL;
    int rc = 0;

    dbg_err_if(ctx == NULL);
    dbg_err_if(res_name == NULL);

    dbg_err_if(tls_file_to_ubuf(res_name, &ubuf));

    dbg_err_if((rc = CyaSSL_CTX_use_certificate_buffer(ctx, u_buf_ptr(ubuf), 
                u_buf_len(ubuf), type)) != SSL_SUCCESS);

    u_buf_free(ubuf); ubuf = NULL;

    return SSL_SUCCESS;
err:
    if(rc)
        crit("load cert error %d", rc);
    if(ubuf)
        u_buf_free(ubuf);
    return -1; /* doesn't return ~0 like all other functions */
}

int tls_use_PrivateKey_file (SSL_CTX *ctx, const char *res_name, int type)
{
    u_buf_t *ubuf = NULL;
    int rc = 0;

    dbg_err_if(ctx == NULL);
    dbg_err_if(res_name == NULL);

    dbg_err_if(tls_file_to_ubuf(res_name, &ubuf));

    dbg_err_if((rc = CyaSSL_CTX_use_PrivateKey_buffer(ctx, u_buf_ptr(ubuf), 
                u_buf_len(ubuf), type)) != SSL_SUCCESS);

    u_buf_free(ubuf); ubuf = NULL;

    return SSL_SUCCESS;
err:
    if(rc)
        crit("load private key error %d", rc);
    if(ubuf)
        u_buf_free(ubuf);
    return -1; /* doesn't return ~0 like all other functions */
}

int tls_use_crls (SSL_CTX *ctx, tls_ctx_args_t *cargs)
{
    u_unused_args(ctx, cargs);
    warn("CyaSSL (%d): CRLs not supported", OPENSSL_VERSION_NUMBER);
    return 0;
}

int tls_use_certificate_chain (SSL_CTX *ctx, const char *res_name, 
        int skipfirst, int (*cb)(char *, int, int, void *)) 
{
    u_buf_t *ubuf = NULL;
    int rc = 0;

    u_unused_args(skipfirst, cb);

    dbg_err_if(ctx == NULL);
    dbg_err_if(res_name == NULL);

    dbg_err_if(tls_file_to_ubuf(res_name, &ubuf));

    dbg_err_if((rc = CyaSSL_CTX_use_certificate_chain_buffer(ctx, 
            u_buf_ptr(ubuf), u_buf_len(ubuf))) != SSL_SUCCESS);

    u_buf_free(ubuf); ubuf = NULL;

    return SSL_SUCCESS;
err:
    if(rc)
        crit("load cert chain error %d", rc);
    if(ubuf)
        u_buf_free(ubuf);
    return -1; /* doesn't return ~0 like all other functions */
}

#endif 
