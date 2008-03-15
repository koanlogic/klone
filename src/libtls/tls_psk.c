/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: tls_psk.c,v 1.1 2008/03/15 15:45:33 tho Exp $
 */

#ifndef HAVE_LIBOPENSSL_PSK
int tls_psk_dummy_decl_stub = 0;
#else /* HAVE_LIBOPENSSL_PSK */
#include <openssl/ssl.h>

static unsigned int psk_cb (SSL* ssl, const char* id, unsigned char* psk,
        unsigned int max_psk_len);

int tls_psk_init (SSL_CTX *c, const char *pskdb)
{
    u_unused_args(cargs, c);

    /* load psk db into mem */
    /* set psk callback */

    return 0;
}

static unsigned int psk_cb (SSL* ssl, const char* id, unsigned char* psk,
        unsigned int max_psk_len)
{
    int psk_len = 0;
    const char *__psk;
    BIGNUM *bn = NULL;

    u_unused_args(ssl);

    /* look up STB PSK (TODO) */
    dbg_err_if ((__psk = NULL) == NULL);

    dbg_err_if (!BN_hex2bn(&bn, __psk));
    dbg_err_if ((unsigned int) BN_num_bytes(bn) > max_psk_len);
    dbg_err_if ((psk_len = BN_bn2bin(bn, psk)) < 0);

    BN_free(bn);

    return psk_len;
err:
    if (bn)
        BN_free(bn);

    return 0;
}

#endif
