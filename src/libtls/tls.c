/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: tls.c,v 1.9 2006/02/25 18:32:40 tat Exp $
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <strings.h>
#ifdef HAVE_LIBOPENSSL
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <u/libu.h>
#include <klone/tls.h>
#include <klone/utils.h>
#include <klone/tlsprv.h>

static int tls_sid = 1;
static int tls_inited = 0; 

/* private methods */
static int  tls_init (void);
static int  tls_context (SSL_CTX **);
static int  tls_load_creds (SSL_CTX *, tls_ctx_args_t *);
static int  tls_gendh_params (SSL_CTX *, const char *);
static int  tls_gen_eph_rsa (SSL_CTX *);
static void tls_rand_seed (void);
static int  tls_sid_context (SSL_CTX *, int *);
static DH   *tls_load_dh_param (const char *);
static int  tls_no_passphrase_cb (char *, int, int, void *);
static int  tls_init_ctx_args (tls_ctx_args_t *);
static int  tls_set_ctx_vdepth (u_config_t *, tls_ctx_args_t *);
static int  tls_set_ctx_vmode (u_config_t *, tls_ctx_args_t *);
static int  tls_check_ctx (tls_ctx_args_t *);
static void tls_free_ctx_args (tls_ctx_args_t *cargs);


/* load SSL_CTX args from configuration */
int tls_load_ctx_args (u_config_t *cfg, tls_ctx_args_t **cargs)
{
    dbg_return_if (!cfg || !cargs, ~0);

    dbg_err_if (!(*cargs = u_zalloc(sizeof(tls_ctx_args_t))));

    (void) tls_init_ctx_args(*cargs);

    (*cargs)->cert = u_config_get_subkey_value(cfg, "cert_file");
    (*cargs)->key = u_config_get_subkey_value(cfg, "key_file");
    (*cargs)->certchain = u_config_get_subkey_value(cfg, "certchain_file");
    (*cargs)->ca = u_config_get_subkey_value(cfg, "ca_file");
    (*cargs)->dh = u_config_get_subkey_value(cfg, "dh_file");
    dbg_err_if (tls_set_ctx_vdepth(cfg, *cargs));
    dbg_err_if (tls_set_ctx_vmode(cfg, *cargs));

    /* check cargs consistency against the supplied values */
    dbg_err_if (tls_check_ctx(*cargs));

    return 0;

err:
    if (*cargs)
        tls_free_ctx_args(*cargs);
    return ~0;
}


/* initialize 'parent' SSL context */
SSL_CTX *tls_init_ctx (tls_ctx_args_t *cargs)
{
    SSL_CTX *c = NULL;

    dbg_return_if (!cargs, NULL);

    /* global init */
    dbg_err_if (tls_init());

    /* create ctx */
    dbg_err_if (tls_context(&c));

    /* don't ask for unlocking passphrases */
    SSL_CTX_set_default_passwd_cb(c, tls_no_passphrase_cb);
    
    /* set key and certs against the SSL context */
    dbg_err_if (tls_load_creds(c, cargs));
 
    /* (possibly) generate DH parameters and load into SSL_CTX */
    dbg_err_if (tls_gen_eph_rsa(c));

    /* generate RSA ephemeral parameters and load into SSL_CTX */
    dbg_err_if (tls_gendh_params(c, cargs->dh));

    /* set the session id context */
    dbg_err_if (tls_sid_context(c, &tls_sid));
    
    return c;

err:
    if (c)
        SSL_CTX_free(c);
    return NULL;
}

static int tls_sid_context (SSL_CTX *c, int *sid)
{
    if (!c || !sid)
        return ~0;
    
    /* every time tls_init_ctx() is called, move on the session id context */
    (*sid)++;
    dbg_err_if (!SSL_CTX_set_session_id_context(c, (void *) sid, sizeof(*sid)));
    
    return 0;

err:
    dbg("%s", tls_get_error()); 

    return ~0;
}

static int tls_context (SSL_CTX **c)
{
    if (!c)
        return ~0;

    dbg_err_if (!(*c = SSL_CTX_new(SSLv23_server_method())));

    return 0;

err:
    dbg("%s", tls_get_error()); 
    *c = NULL;  /* reset pointer (useless) */

    return ~0;
}

/* XXX very primitive */
char *tls_get_error (void)
{
    unsigned long   e;

    e = ERR_get_error();
    return ERR_error_string(e, NULL);
}

void tls_dbg_openssl_err (void)
{
    enum { BUFSZ = 256 };
    unsigned long e;
    char buf[BUFSZ];

    while ((e = ERR_get_error()))
        warn("%s", ERR_error_string(e, buf));

    return;
}

/* if skey is NULL, assume private key in scert, ca can be NULL */
static int tls_load_creds (SSL_CTX *c, tls_ctx_args_t *cargs)
{
    dbg_return_if (!c || !cargs || !cargs->cert, ~0);

    if (!cargs->key)
        cargs->key = cargs->cert;
    
    /* set ca if supplied */
    if (cargs->ca)
        dbg_err_if (tls_load_verify_locations(c, cargs->ca));

    /* explicitly set the list of CAs for which we accept certificates */
    if (cargs->ca && cargs->vmode != SSL_VERIFY_NONE)
        SSL_CTX_set_client_CA_list(c, tls_load_client_CA_file(cargs->ca));

    /* load server certificate */
    dbg_err_if (tls_use_certificate_file(c, cargs->cert, 
                                         SSL_FILETYPE_PEM) <= 0);

    /* load private key (perhaps from the cert file) */
    dbg_err_if (tls_use_PrivateKey_file(c, cargs->key, SSL_FILETYPE_PEM) <= 0);

    /* check skey consistency against scert */
    dbg_err_if (!SSL_CTX_check_private_key(c));

    /* load optional server certficate chain */
    if (cargs->certchain)
        dbg_err_if (tls_use_certificate_chain(c, cargs->certchain, 
                                              0, NULL) < 0);

    /* set SSL verify mode (no, optional, required) and depth */
    SSL_CTX_set_verify(c, cargs->vmode, NULL);
    if (cargs->depth > 0)
        SSL_CTX_set_verify_depth(c, cargs->depth);

    return 0;

err:
    dbg("%s", tls_get_error()); 
    return ~0;
}

static int tls_init (void)
{
    if (tls_inited)
        return 0;

    SSL_load_error_strings();
    dbg_err_if (!SSL_library_init());
    tls_rand_seed(); 
    tls_inited++;

    return 0;

err:
    dbg("%s", tls_get_error()); 
    return ~0;
}

static void tls_rand_seed (void)
{
    struct timeval  tv;
    tls_rand_seed_t seed;

    (void) gettimeofday(&tv, NULL);
    
    seed.pid = getpid();
    seed.t1 = tv.tv_sec; 
    seed.t2 = tv.tv_usec;
    seed.stack = (void *) &seed;

    RAND_seed((const void *) &seed, sizeof seed);
}

/* generate RSA ephemeral parameters and load'em into SSL_CTX */
static int tls_gen_eph_rsa(SSL_CTX *c)
{
    RSA *eph_rsa = NULL;

    dbg_return_if (!c, ~0);

    dbg_err_if (!(eph_rsa = RSA_generate_key(512, RSA_F4, 0, NULL)));
    dbg_err_if (!SSL_CTX_set_tmp_rsa(c, eph_rsa));
    RSA_free(eph_rsa); /* eph_rsa is dup'ed by SSL_CTX_set_tmp_rsa() */

    return 0;

err:
    dbg("%s", tls_get_error());
    if (eph_rsa)
        RSA_free(eph_rsa);    

    return ~0;
}

/* generate DH ephemeral parameters and load'em into SSL_CTX */
static int tls_gendh_params(SSL_CTX *c, const char *dhfile)
{
    DH  *eph_dh = NULL;

    dbg_return_if (!c, ~0);

    eph_dh = dhfile ? tls_load_dh_param(dhfile) : get_dh1024(); 
    dbg_err_if (!(eph_dh));

    dbg_err_if (!SSL_CTX_set_tmp_dh(c, eph_dh));

#if 0
    /* Avoid small subgroup attacks (if p and g are strong primes
     * this is not strictly necessary).  This is said to have a negligible (?)
     * impact during negotiation phase. TODO: test it ! */
    (void) SSL_CTX_set_options(c, SSL_OP_SINGLE_DH_USE); */
#endif /* 0 */

    return 0;

err:
    dbg("%s", tls_get_error());
    if (eph_dh)
        DH_free(eph_dh);

    return ~0;
}

static DH *tls_load_dh_param (const char *res_name)
{
    DH  *dh = NULL;
    BIO *bio = NULL;

    dbg_return_if (!res_name, NULL);

    /* XXX say return_if here instead of err_if because bio_from_emb()
     * could have failed for a non-openssl error */
    dbg_return_if (!(bio = tls_get_file_bio(res_name)), NULL);

    dbg_err_if (!(dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL)));

    BIO_free(bio);
    return dh;

err:
    dbg("%s", tls_get_error());
    if (bio) 
        BIO_free(bio);

    return NULL;
}

static int tls_no_passphrase_cb (char *buf, int num, int w, void *arg)
{
    /* avoid gcc complains */
    buf = NULL;
    arg = NULL;
    num = w = 0;

    return -1;
}

static int tls_init_ctx_args (tls_ctx_args_t *cargs)
{
    dbg_return_if (!cargs, ~0);

    cargs->cert  = NULL;
    cargs->key   = NULL;
    cargs->ca    = NULL;
    cargs->dh    = NULL;
    cargs->depth = 1;
    cargs->vmode = SSL_VERIFY_NONE;

    return 0;
}

static int tls_set_ctx_vdepth (u_config_t *cfg, tls_ctx_args_t *cargs)
{
    u_config_t    *k;
    
    dbg_return_if (!cfg || !cargs, ~0);

    if (!u_config_get_subkey(cfg, "verify_depth", &k))
    {
        cargs->depth = atoi(u_config_get_value(k));    
    } 

    /* XXX check consistent values for the verification chain's depth ? */

    return 0;
}

static int tls_set_ctx_vmode (u_config_t *cfg, tls_ctx_args_t *cargs)
{
    const char  *v;
    
    dbg_return_if (!cfg || !cargs, ~0);
    
    if (!(v = u_config_get_subkey_value(cfg, "verify_mode")))
       return 0;    /* will use the default (none) */ 

    if (!strcasecmp(v, "no"))
        cargs->vmode = SSL_VERIFY_NONE;
    else if (!strcasecmp(v, "optional"))
        cargs->vmode = SSL_VERIFY_PEER;
    else if (!strcasecmp(v, "require"))
        cargs->vmode = SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    else {
        dbg("unknown verification value:\'%s\'", v);
        return ~0;
    }
    
    return 0;
}

static int tls_check_ctx (tls_ctx_args_t *cargs)
{
    dbg_return_if (!cargs, ~0);

    /* cert_file is a MUST */
    dbg_err_if (!cargs->cert || strlen(cargs->cert) == 0);

    /* if private key file is missing, assume the key is inside cert_file */
    dbg_if (!cargs->key);

    /* if verify_mode == "required" the CA file MUST be present */
    if (cargs->vmode & SSL_VERIFY_PEER)
        dbg_err_if (!cargs->ca);

    return 0;
err:
    return ~0;
}


static void tls_free_ctx_args (tls_ctx_args_t *cargs)
{
    KLONE_FREE(cargs);
    return;
}
#endif /* HAVE_LIBOPENSSL */
