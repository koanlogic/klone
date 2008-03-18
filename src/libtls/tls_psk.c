/*
 * Copyright (c) 2005-2008 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: tls_psk.c,v 1.3 2008/03/18 23:09:09 tho Exp $
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <strings.h>

#ifndef HAVE_LIBOPENSSL_PSK
int tls_psk_dummy_decl_stub = 0;
#else /* HAVE_LIBOPENSSL_PSK */
#include <openssl/ssl.h>
#include <u/libu.h>
#include <klone/tls.h>
#include <klone/utils.h>
#include <klone/tlsprv.h>

static unsigned int psk_cb (SSL *ssl, const char *id, unsigned char *psk,
        unsigned int max_psk_len);

static int __pwd_exdata_idx (void);

int tls_psk_init (SSL_CTX *c, tls_ctx_args_t *cargs)
{
    int rc;
    u_pwd_t *pwd = NULL;

    /* create pwd instance */
    rc = u_pwd_init_agnostic(cargs->pskdb, cargs->psk_is_hashed, 0, &pwd);
    dbg_err_ifm (rc, "psk pwd creation failed (%s)", cargs->pskdb);

    /* stick the pwd handler into the SSL context */
    SSL_CTX_set_ex_data(c, __pwd_exdata_idx(), pwd);

    /* set psk callback */
    SSL_CTX_set_psk_server_callback(c, psk_cb);

    return 0;
err:
    if (pwd)
        u_pwd_term(pwd);

    return ~0;
}

static unsigned int psk_cb (SSL *ssl, const char *id, unsigned char *psk,
        unsigned int max_psk_len)
{
    u_pwd_t *pwd = NULL;
    u_pwd_rec_t *pwd_rec = NULL;
    int psk_len = 0;
    const char *__psk;
    BIGNUM *bn = NULL;

    /* retrieve pwd handler that we previously cached in SSL_CTX's ex_data */
    pwd = (u_pwd_t *) SSL_get_ex_data(ssl, __pwd_exdata_idx());
    dbg_err_if (pwd == NULL);

    /* get a pwd record for the supplied id */
    dbg_err_if (u_pwd_retr(pwd, id, &pwd_rec));
    dbg_err_if ((__psk = u_pwd_rec_get_password(pwd_rec)) == NULL);

    /* do the requested psk conversion */
    dbg_err_if (!BN_hex2bn(&bn, __psk));
    dbg_err_if ((unsigned int) BN_num_bytes(bn) > max_psk_len);
    dbg_err_if ((psk_len = BN_bn2bin(bn, psk)) < 0);

    /* dispose temp stuff */
    BN_free(bn);

    if (!u_pwd_in_memory(pwd))
        u_pwd_rec_free(pwd_rec);

    return psk_len;
err:
    if (bn)
        BN_free(bn);

    /* if we've (pwd_rec != NULL) we also have (pwd != NULL) */
    if (pwd_rec && !u_pwd_in_memory(pwd))
        u_pwd_rec_free(pwd_rec);

    return 0;
}

static int __pwd_exdata_idx (void)
{
    char tag[32];
    static int idx = -1;

    if (idx < 0)
    {
        CRYPTO_w_lock(CRYPTO_LOCK_SSL_CTX);
        strlcpy(tag, "PSK pwd instance", sizeof tag);
        idx = SSL_CTX_get_ex_new_index(0, tag, NULL, NULL, NULL);
        CRYPTO_w_unlock(CRYPTO_LOCK_SSL_CTX);
    }

    return idx;
}

#endif  /* !HAVE_LIBOPENSSL_PSK */
