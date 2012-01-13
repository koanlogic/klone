/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <u/libu.h>
#include <klone/session.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/vars.h>
#include <klone/utils.h>
#include <klone/emb.h>
#include <klone/ses_prv.h>
#include <klone/codecs.h>
#ifdef SSL_ON
#include <openssl/hmac.h>
#include <openssl/rand.h>
#endif
#ifdef SSL_CYASSL
#include <config.h>
#include <types.h>
#include <ctc_hmac.h>
#include <ctc_aes.h>
#define EVP_MAX_MD_SIZE 64
#endif

#define KL1_CLISES_DATA     "KL1_CLISES_DATA"
#define KL1_CLISES_MTIME    "KL1_CLISES_MTIME"
#define KL1_CLISES_HMAC     "KL1_CLISES_HMAC"
#define KL1_CLISES_IV       "KL1_CLISES_IV"

enum { HMAC_HEX_SIZE = 2*EVP_MAX_MD_SIZE + 1 };

/* calc the hmac of data+sid+mtime */
static int session_client_hmac(HMAC_CTX *ctx, char *hmac, size_t hmac_sz, 
    const char *data, const char *sid, const char *mtime, const char *hex_iv)
{
    char mac[EVP_MAX_MD_SIZE];
    int mac_len;

    dbg_err_if (ctx == NULL);
    dbg_err_if (hmac == NULL);
    dbg_err_if (data == NULL);
    dbg_err_if (sid == NULL);
    dbg_err_if (mtime == NULL);
    /* hex_iv may be NULL */
    
#if SSL_OPENSSL
    /* hmac must be at least 'EVP_MAX_MD_SIZE*2 + 1' (it will be hex encoded) */
    dbg_err_if(hmac_sz < EVP_MAX_MD_SIZE*2 + 1);

    /* calc HMAC hash of the data buf + mtime (reuse stored key and md) */
    HMAC_Init_ex(ctx, NULL, 0, NULL, NULL); 
    HMAC_Update(ctx, data, strlen(data));
    HMAC_Update(ctx, sid, strlen(sid));
    HMAC_Update(ctx, mtime, strlen(mtime));
    if(hex_iv)
        HMAC_Update(ctx, hex_iv, strlen(hex_iv));
    HMAC_Final(ctx, mac, &mac_len);
#endif

#ifdef SSL_CYASSL
    HmacUpdate(ctx, (const byte*)data, strlen(data));
    HmacUpdate(ctx, (const byte*)sid, strlen(sid));
    HmacUpdate(ctx, (const byte*)mtime, strlen(mtime));
    if(hex_iv)
        HmacUpdate(ctx, (const byte*)hex_iv, strlen(hex_iv));
    HmacFinal(ctx, (byte*)mac);

    if (ctx->macType == MD5)
        mac_len = MD5_DIGEST_SIZE;
    else if (ctx->macType == SHA)
        mac_len = SHA_DIGEST_SIZE;
    else
        crit_err("unknown hash");
#endif

    /* encode the hash */
    dbg_err_if(u_hexncpy(hmac, mac, mac_len, HEXCPY_ENCODE) <= 0);

    return 0;
err:
    return -1;
}

static int session_client_save(session_t *ss)
{
    /* BUF_SIZE: cookie size + MAC size + gzip header + encryption padding  */
    enum { 
        MTIME_SIZE = 32, 
        BUF_SIZE = COOKIE_MAX_SIZE + EVP_MAX_BLOCK_LENGTH + 96 
    };
    session_opt_t *so = ss->so;
    char hmac[HMAC_HEX_SIZE], ebuf[BUF_SIZE], mtime[MTIME_SIZE];
    char *buf = NULL, cipher_iv_hex[CIPHER_IV_LEN * 2 + 1];
    size_t sz;
    time_t now;

    dbg_err_if (ss == NULL);

    #ifdef SSL_ON
    if(ss->so->encrypt)
    {
        /* generate a new IV for each session */
        #ifdef SSL_OPENSSL
        dbg_err_if(!RAND_pseudo_bytes(ss->so->cipher_iv, CIPHER_IV_LEN));
        #endif

        #ifdef SSL_CYASSL
        dbg_err_if(!RAND_bytes(ss->so->cipher_iv, CIPHER_IV_LEN));
        #endif

        /* hex encode the IV and save it in a cookie */
        dbg_err_if(u_hexncpy(cipher_iv_hex, ss->so->cipher_iv, CIPHER_IV_LEN,
            HEXCPY_ENCODE) <= 0);
        dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_IV, cipher_iv_hex, 
            0, NULL, NULL, 0));
    }
    #endif

    /* save the session data to freshly alloc'd buf of size sz */
    dbg_err_if(session_prv_save_to_buf(ss, &buf, &sz));

    warn_err_ifm(sz > COOKIE_MAX_SIZE, 
                "session data too big for client-side sessions");

    /* hex-encode the buffer */
    dbg_err_if(u_hexncpy(ebuf, buf, sz, HEXCPY_ENCODE) <= 0);

    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_DATA, ebuf, 0, NULL, 
        NULL, 0));

    /* set mtime cookie */
    dbg_err_sif ((now = time(NULL)) == (time_t) -1);
    dbg_err_if (u_snprintf(mtime, sizeof mtime, "%d", (ss->mtime = (int) now)));
    dbg_err_if (response_set_cookie(ss->rs, KL1_CLISES_MTIME, mtime, 0, NULL,
                NULL, 0));

    /* calc the HMAC */
    dbg_err_if(session_client_hmac(&so->hmac_ctx, hmac, HMAC_HEX_SIZE, 
        ebuf, ss->id, mtime, ss->so->encrypt ? cipher_iv_hex : NULL));

    /* store the hash in a cookie */
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_HMAC, hmac, 0, NULL, 
        NULL, 0));

    u_free(buf);

    return 0;
err:
    if(buf)
        u_free(buf);
    return ~0;
}

static int session_client_load(session_t *ss)
{
    session_opt_t *so = ss->so;
    char hmac[HMAC_HEX_SIZE], buf[COOKIE_MAX_SIZE];
    const char *cli_ebuf, *cli_hmac, *cli_mtime, *cli_iv;
    ssize_t c;

    dbg_err_if (ss == NULL);

    /* extract session data, mtime and hmac from cookies */
    cli_ebuf = request_get_cookie(ss->rq, KL1_CLISES_DATA);
    cli_mtime = request_get_cookie(ss->rq, KL1_CLISES_MTIME);
    cli_hmac = request_get_cookie(ss->rq, KL1_CLISES_HMAC);
    cli_iv = request_get_cookie(ss->rq, KL1_CLISES_IV);

    //nop_err_if(cli_ebuf == NULL || cli_mtime == NULL || cli_hmac == NULL);
    dbg_err_if(cli_ebuf == NULL || cli_mtime == NULL || cli_hmac == NULL);
    /* cli_iv may be NULL */

    /* calc the HMAC */
    dbg_err_if(session_client_hmac(&so->hmac_ctx, hmac, HMAC_HEX_SIZE, 
        cli_ebuf, ss->id, cli_mtime, ss->so->encrypt ? cli_iv : NULL));

    /* compare HMACs */
    if(strcmp(hmac, cli_hmac))
    {
        session_remove(ss); /* remove all bad stale data */
        warn_err("HMAC don't match, rejecting session data");
    }

    /* hash ckeched. decode/uncompress/decrypt session data */

    /* hex decode and save current cipher IV */
    dbg_err_if((c = u_hexncpy(ss->so->cipher_iv, cli_iv, strlen(cli_iv), 
        HEXCPY_DECODE)) <= 0);

    /* set client provided mtime */
    ss->mtime = strtoul(cli_mtime, NULL, 0);

    dbg_err_if(strlen(cli_ebuf) > COOKIE_MAX_SIZE);

    /* hex decode session data */
    dbg_err_if((c = u_hexncpy(buf, cli_ebuf, strlen(cli_ebuf), 
        HEXCPY_DECODE)) <= 0);

    /* load session data from the buffer */
    dbg_err_if(session_prv_load_from_buf(ss, buf, c));

    return 0;
err:
    return ~0;
}

static int session_client_term(session_t *ss)
{
    u_unused_args(ss);
    return 0;
}

static int session_client_remove(session_t *ss)
{
    dbg_err_if (ss == NULL);
    
    /* removes all clises-related cookies */
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_DATA, NULL, 0, NULL, 
        NULL, 0));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_MTIME, NULL, 0, NULL, 
        NULL, 0));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_HMAC, NULL, 0, NULL, 
        NULL, 0));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_IV, NULL, 0, NULL, 
        NULL, 0));

    return 0;
err:
    return ~0;
}

int session_client_create(session_opt_t *so, request_t *rq, response_t *rs, 
        session_t **pss)
{
    session_t *ss = NULL;

    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    dbg_err_if (pss == NULL);
    dbg_err_if (so == NULL);

    ss = u_zalloc(sizeof(session_t));
    dbg_err_if(ss == NULL);

    ss->load = session_client_load;
    ss->save = session_client_save;
    ss->remove = session_client_remove;
    ss->term = session_client_term;
    ss->mtime = time(0);
    ss->so = so;

    dbg_err_if(session_prv_init(ss, rq, rs));

    *pss = ss;

    return 0;
err:
    if(ss)
        session_free(ss);
    return ~0;
}

/* this function will be called once by the server during startup */
int session_client_module_init(u_config_t *config, session_opt_t *so)
{
    u_config_t *c;
    const char *v;

    /* config may be NULL */
    dbg_err_if (so == NULL);
    
    /* default */
    so->hash = EVP_sha1(); 

    /* always enable encryption for client-side sessions */
    if(!so->encrypt)
        warn("encryption is required for client side session");
    so->encrypt = 1;

    if(config && !u_config_get_subkey(config, "client", &c))
    {
        if((v = u_config_get_subkey_value(c, "hash_function")) != NULL)
        {
            if(!strcasecmp(v, "md5"))
                so->hash = EVP_md5();
            else if(!strcasecmp(v, "sha1"))
                so->hash = EVP_sha1();
#ifdef SSL_OPENSSL
            else if(!strcasecmp(v, "ripemd160"))
                so->hash = EVP_ripemd160();
#endif
            else
                warn_err("config error: bad hash_function");
        } 
    }

#ifdef SSL_OPENSSL
    /* initialize OpenSSL HMAC stuff */
    HMAC_CTX_init(&so->hmac_ctx);

    /* gen HMAC key */
    dbg_err_if(!RAND_bytes(so->hmac_key, HMAC_KEY_LEN));

    /* init HMAC with our key and chose hash algorithm */
    HMAC_Init_ex(&so->hmac_ctx, so->hmac_key, HMAC_KEY_LEN, so->hash, NULL);
#endif

#ifdef SSL_CYASSL
    /* gen HMAC key */
    dbg_err_if(!RAND_bytes(so->hmac_key, HMAC_KEY_LEN));

    if(strcmp(so->hash, "MD5") == 0)
        HmacSetKey(&so->hmac_ctx, MD5, so->hmac_key, HMAC_KEY_LEN);
    else if(strcmp(so->hash, "SHA") == 0)
        HmacSetKey(&so->hmac_ctx, SHA, so->hmac_key, HMAC_KEY_LEN);
#endif

    return 0;
err:
    return ~0;
}


