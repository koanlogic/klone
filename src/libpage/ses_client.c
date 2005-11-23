/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ses_client.c,v 1.23 2005/11/23 17:27:02 tho Exp $
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <u/libu.h>
#include <klone/session.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/vars.h>
#include <klone/utils.h>
#include <klone/emb.h>
#include <klone/ses_prv.h>
#include <klone/codecs.h>

#define KL1_CLISES_DATA     "KL1_CLISES_DATA"
#define KL1_CLISES_MTIME    "KL1_CLISES_MTIME"
#define KL1_CLISES_HMAC     "KL1_CLISES_HMAC"

enum { HMAC_HEX_SIZE = 2*EVP_MAX_MD_SIZE + 1 };

/* calc the hmac of data+sid+mtime */
static int session_client_hmac(HMAC_CTX *ctx, char *hmac, size_t hmac_sz, 
    const char *data, const char *sid, const char *mtime)
{
    char mac[EVP_MAX_MD_SIZE];
    int mac_len;

    /* hmac must be at least 'EVP_MAX_MD_SIZE*2 + 1' (it will be hex encoded) */
    dbg_err_if(hmac_sz < EVP_MAX_MD_SIZE*2 + 1);

    /* calc HMAC hash of the data buf + mtime (reuse stored key and md) */
    HMAC_Init_ex(ctx, NULL, 0, NULL, NULL); 
    HMAC_Update(ctx, data, strlen(data));
    HMAC_Update(ctx, sid, strlen(sid));
    HMAC_Update(ctx, mtime, strlen(mtime));
    HMAC_Final(ctx, mac, &mac_len);

    /* encode the hash */
    dbg_err_if(u_hexncpy(hmac, mac, mac_len, URLCPY_ENCODE) <= 0);

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
    char *buf = NULL;
    size_t sz;

    /* save the session data to freshly alloc'd buf of size sz */
    dbg_err_if(session_prv_save_to_buf(ss, &buf, &sz));

    warn_err_ifm(sz > COOKIE_MAX_SIZE, 
                "session data too big for client-side sessions");

    /* hex-encode the buffer */
    dbg_err_if(u_hexncpy(ebuf, buf, sz, URLCPY_ENCODE) <= 0);

    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_DATA, ebuf, 0, NULL, 
        NULL, 0));

    /* set mtime cookie */
    ss->mtime = time(0);
    dbg_err_if(u_snprintf(mtime, MTIME_SIZE, "%lu", ss->mtime));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_MTIME, mtime, 0, NULL, 
        NULL, 0));

    /* calc the HMAC */
    dbg_err_if(session_client_hmac(&so->hmac_ctx, hmac, HMAC_HEX_SIZE, ebuf, 
        ss->id, mtime));

    /* store the hash in a cookie */
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_HMAC, hmac, 0, NULL, 
        NULL, 0));

    return 0;
err:
    return ~0;
}

static int session_client_load(session_t *ss)
{
    session_opt_t *so = ss->so;
    char hmac[HMAC_HEX_SIZE], buf[COOKIE_MAX_SIZE];
    const char *cli_ebuf, *cli_hmac, *cli_mtime;
    ssize_t c;

    /* extract session data, mtime and hmac from cookies */
    cli_ebuf = request_get_cookie(ss->rq, KL1_CLISES_DATA);
    cli_mtime = request_get_cookie(ss->rq, KL1_CLISES_MTIME);
    cli_hmac = request_get_cookie(ss->rq, KL1_CLISES_HMAC);

    dbg_err_if(cli_ebuf == NULL || cli_mtime == NULL || cli_hmac == NULL);

    /* calc the HMAC */
    dbg_err_if(session_client_hmac(&so->hmac_ctx, hmac, HMAC_HEX_SIZE, 
        cli_ebuf, ss->id, cli_mtime));

    /* compare HMACs */
    if(strcmp(hmac, cli_hmac))
    {
        session_remove(ss); /* remove all bad stale data */
        warn_err("HMAC don't match, rejecting session data");
    }

    /* hash ckeched. decode/uncompress/decrypt session data */

    /* set client provided mtime */
    ss->mtime = strtoul(cli_mtime, NULL, 0);

    dbg_err_if(strlen(cli_ebuf) > COOKIE_MAX_SIZE);

    /* hex decode session data */
    dbg_err_if((c = u_hexncpy(buf, cli_ebuf, strlen(cli_ebuf), 
        URLCPY_DECODE)) <= 0);

    c--; /* ignore last '\0' that hexncpy adds */

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
    /* removes all clises-related cookies */
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_DATA, NULL, 0, NULL, 
        NULL, 0));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_MTIME, NULL, 0, NULL, 
        NULL, 0));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_HMAC, NULL, 0, NULL, 
        NULL, 0));

    return 0;
err:
    return ~0;
}

int session_client_create(session_opt_t *so, request_t *rq, response_t *rs, 
        session_t **pss)
{
    session_t *ss = NULL;

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

    /* defaults */
    so->hash = EVP_sha1(); 

    /* always enable encryption for client-side sessions */
    if(!so->encrypt)
        warn("encryption is required for client side session");
    so->encrypt = 1;

    dbg_err_if(u_config_get_subkey(config, "client", &c));

    if((v = u_config_get_subkey_value(c, "hash_function")) != NULL)
    {
        if(!strcasecmp(v, "md5"))
            so->hash = EVP_md5();
        else if(!strcasecmp(v, "sha1"))
            so->hash = EVP_sha1();
        else if(!strcasecmp(v, "ripemd160"))
            so->hash = EVP_ripemd160();
        else
            warn_err("config error: bad hash_function");
    } 

    /* initialize OpenSSL HMAC stuff */
    HMAC_CTX_init(&so->hmac_ctx);

    /* gen HMAC key */
    dbg_err_if(!RAND_bytes(so->hmac_key, HMAC_KEY_SIZE));

    /* init HMAC with our key and chose hash algorithm */
    HMAC_Init_ex(&so->hmac_ctx, so->hmac_key, HMAC_KEY_SIZE, so->hash, NULL);

    return 0;
err:
    return ~0;
}


