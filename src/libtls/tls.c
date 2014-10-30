/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: tls.c,v 1.21 2008/07/10 08:56:13 tat Exp $
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <strings.h>
#ifdef SSL_ON
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#ifdef SSL_OPENSSL
#include <openssl/x509_vfy.h>
#endif
#include <u/libu.h>
#include <klone/tls.h>
#include <klone/utils.h>
#include <klone/tlsprv.h>

static int tls_sid = 1;
static int tls_inited = 0; 

/* private methods */
static int tls_context (SSL_CTX **);
static int tls_load_x509_creds (SSL_CTX *, tls_ctx_args_t *);
static void tls_rand_seed (void);
static int tls_sid_context (SSL_CTX *, int *);
#ifdef SSL_OPENSSL
static int tls_gen_eph_rsa (SSL_CTX *);
static int tls_gendh_params (SSL_CTX *, const char *);
static DH *tls_load_dh_param (const char *);
#endif
static int tls_no_passphrase_cb (char *, int, int, void *);
static int tls_init_ctx_args (tls_ctx_args_t *);
static int tls_set_ctx_vdepth (u_config_t *, tls_ctx_args_t *);
static int tls_set_ctx_crlopts (u_config_t *, tls_ctx_args_t *);
static int tls_set_ctx_vmode (u_config_t *, tls_ctx_args_t *);
static int tls_check_ctx (tls_ctx_args_t *);
static void tls_free_ctx_args (tls_ctx_args_t *cargs);
static int tls_load_ctx_args (u_config_t *cfg, tls_ctx_args_t **cargs);
static SSL_CTX *tls_init_ctx (tls_ctx_args_t *cargs);
static int cb_vfy (int ok, X509_STORE_CTX *store_ctx);
#ifdef SSL_OPENSSL_PSK
static int tls_set_ctx_psk_hash (u_config_t *, tls_ctx_args_t *);
#endif

SSL_CTX *tls_load_init_ctx (u_config_t *cfg)
{
    SSL_CTX *ctx = NULL;
    tls_ctx_args_t *cargs = NULL;

    dbg_return_if (cfg == NULL, NULL);

    dbg_err_if (tls_load_ctx_args(cfg, &cargs));
    dbg_err_if ((ctx = tls_init_ctx(cargs)) == NULL);

    tls_free_ctx_args(cargs);

    return ctx;
err:
    if (cargs)
        tls_free_ctx_args(cargs);
    if (ctx)
        SSL_CTX_free(ctx);
    return NULL;
}

/* load SSL_CTX args from configuration */
static int tls_load_ctx_args (u_config_t *cfg, tls_ctx_args_t **pcargs)
{
    tls_ctx_args_t *cargs = NULL;

    dbg_return_if (cfg == NULL, ~0);
    dbg_return_if (pcargs == NULL, ~0);

    cargs = u_zalloc(sizeof(tls_ctx_args_t));
    dbg_err_if (cargs == NULL);

    (void) tls_init_ctx_args(cargs);

    cargs->cert = u_config_get_subkey_value(cfg, "cert_file");
    cargs->key = u_config_get_subkey_value(cfg, "key_file");
    cargs->certchain = u_config_get_subkey_value(cfg, "certchain_file");
    cargs->ca = u_config_get_subkey_value(cfg, "ca_file");
    cargs->dh = u_config_get_subkey_value(cfg, "dh_file");
    cargs->crl = u_config_get_subkey_value(cfg, "crl_file");
#ifdef SSL_OPENSSL_PSK
    /* handle 'pskdb_file', 'psk_hint' and 'psk_hash' keywords */
    cargs->pskdb = u_config_get_subkey_value(cfg, "pskdb_file");
    cargs->psk_hint = u_config_get_subkey_value(cfg, "psk_hint");
    dbg_err_if (tls_set_ctx_psk_hash(cfg, cargs));
#endif
    dbg_err_if (tls_set_ctx_crlopts(cfg, cargs));
    dbg_err_if (tls_set_ctx_vdepth(cfg, cargs));
    dbg_err_if (tls_set_ctx_vmode(cfg, cargs));

    /* check cargs consistency against the supplied values */
    crit_err_ifm (tls_check_ctx(cargs), 
            "error validating SSL configuration options");

    *pcargs = cargs;

    return 0;
err:
    if (cargs)
        tls_free_ctx_args(cargs);
    return ~0;
}

/* initialize 'parent' SSL context */
static SSL_CTX *tls_init_ctx (tls_ctx_args_t *cargs)
{
    SSL_CTX *c = NULL;

    dbg_return_if (cargs == NULL, NULL);

    /* global init */
    dbg_err_if (tls_init());

    /* create SSL CTX from where all the SSL sessions will be cloned */
    dbg_err_if (tls_context(&c));

    /* don't ask for unlocking passphrases: this assumes that all 
     * credentials are stored in clear text */
    SSL_CTX_set_default_passwd_cb(c, tls_no_passphrase_cb);

    /* NOTE: configuration has been sanitized earlier by tls_check_ctx, 
     * so we can be reasonably sure that one (or both) of PSK or X.509 
     * credentials are in place. */

    if (cargs->cert)
        /* set key and certs against the SSL context */
        dbg_err_if (tls_load_x509_creds(c, cargs));

#ifdef SSL_OPENSSL_PSK
    if (cargs->pskdb)
        /* load psk DB and set psk callback */
        dbg_err_if (tls_psk_init(c, cargs));
#endif

#ifdef SSL_OPENSSL
    /* generate RSA ephemeral parameters and load into SSL_CTX */
    dbg_err_if (tls_gen_eph_rsa(c));

    /* (possibly) generate DH parameters and load into SSL_CTX */
    dbg_err_if (tls_gendh_params(c, cargs->dh));
#endif

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
    int rc;

    dbg_return_if (c == NULL, ~0); 
    dbg_return_if (sid == NULL, ~0); 
    
    /* every time tls_init_ctx() is called, move on the session id context */
    (*sid)++;

    rc = SSL_CTX_set_session_id_context(c, (void *) sid, sizeof(int));
    dbg_err_ifm (rc == 0, "error setting sid: %s", tls_get_error());
    
    return 0;
err:
    return ~0;
}

static int tls_context (SSL_CTX **pc)
{
    SSL_CTX *c = NULL;

    dbg_return_if (pc == NULL, ~0);

    c = SSL_CTX_new(TLSv1_server_method());
    dbg_err_ifm (c == NULL, "error creating SSL CTX: %s", tls_get_error());

    *pc = c;

    return 0;
err:
    return ~0;
}

/* XXX very primitive */
char *tls_get_error (void)
{
    unsigned long e = ERR_get_error();
    return ERR_error_string(e, NULL);
}

/* if skey is NULL, assume private key in scert, ca can be NULL */
static int tls_load_x509_creds (SSL_CTX *c, tls_ctx_args_t *cargs)
{
    dbg_return_if (c == NULL, ~0);
    dbg_return_if (cargs == NULL, ~0);
    dbg_return_if (cargs->cert == NULL, ~0);

    /* if key file unspecified assume key+cert are bundled */
    if (!cargs->key)
        cargs->key = cargs->cert;
    
    /* set ca if supplied */
    if (cargs->ca)
        crit_err_ifm(tls_load_verify_locations(c, cargs->ca),
                "error loading CA certificate from %s", cargs->ca);

#ifdef SSL_OPENSSL
    /* explicitly set the list of CAs for which we accept certificates */
    if (cargs->ca && cargs->vmode != SSL_VERIFY_NONE)
        SSL_CTX_set_client_CA_list(c, tls_load_client_CA_file(cargs->ca));
#endif

    /* load server certificate */
    crit_err_ifm (tls_use_certificate_file(c, cargs->cert, 
                SSL_FILETYPE_PEM) <= 0, 
            "error loading server certificate from %s", cargs->cert);

    /* load private key (perhaps from the cert file) */
    crit_err_ifm (tls_use_PrivateKey_file(c, cargs->key, SSL_FILETYPE_PEM) <= 0,
            "error loading the private key from %s", cargs->key);

    /* check skey consistency against scert */
    crit_err_ifm (!SSL_CTX_check_private_key(c),
            "the given private key doesn't seem to belong "
            "to the server certificate");

    /* load optional server certficate chain */
    if (cargs->certchain)
        crit_err_ifm (tls_use_certificate_chain(c, cargs->certchain, 
                    0, NULL) < 0, 
                "error loading server certificate chain");

    /* load optional CRL file + opts into args */
    if (cargs->crl)
        crit_err_ifm (tls_use_crls(c, cargs), "error loading CA CRL file");

    /* set SSL verify mode (no, optional, required) and callbacks */
    SSL_CTX_set_verify(c, cargs->vmode, cb_vfy);

    /* set verification depth */
    if (cargs->depth > 0)
    {
#ifdef SSL_OPENSSL
        SSL_CTX_set_verify_depth(c, cargs->depth);
#else
        warn("certificate verification depth not supported");
#endif
    }

    return 0;
err:
    return ~0;
}

static int cb_vfy (int ok, X509_STORE_CTX *store_ctx)
{
    int e;
    X509 *x;
    char buf[1024];
    
    if (ok)
        return ok;

    e = X509_STORE_CTX_get_error(store_ctx);
    x = store_ctx->current_cert;

    /* at present just put a note in the log.
     * the idea is that here we can catch CRL specific errors and, based 
     * on the value of crl_opts directive, use different accept/reject 
     * policies.  e.g. return ok in case X509_V_ERR_CRL_HAS_EXPIRED, etc. */
    u_info("%s; current certificate subject is %s", 
            X509_verify_cert_error_string(e), 
            X509_NAME_oneline(X509_get_subject_name(x), buf, sizeof buf));

    return 0;
}

int tls_init (void)
{
    if (tls_inited)
        return 0;

    SSL_load_error_strings();
    dbg_err_if (!SSL_library_init());
    tls_rand_seed(); 
    tls_inited++;

    return 0;

err:
    u_dbg("%s", tls_get_error()); 
    return ~0;
}

static void tls_rand_seed (void)
{
#ifdef SSL_OPENSSL
    struct timeval tv;
    tls_rand_seed_t seed;

    (void) gettimeofday(&tv, NULL);
    
    seed.pid = getpid();
    seed.t1 = tv.tv_sec; 
    seed.t2 = tv.tv_usec;
    seed.stack = (void *) &seed;

    RAND_seed((const void *) &seed, sizeof seed);
#endif
}

#ifdef SSL_OPENSSL
/* generate RSA ephemeral parameters and load'em into SSL_CTX */
static int tls_gen_eph_rsa(SSL_CTX *c)
{
    RSA *eph_rsa = NULL;

    dbg_return_if (c == NULL, ~0);

    dbg_err_if (!(eph_rsa = RSA_generate_key(512, RSA_F4, 0, NULL)));
    dbg_err_if (!SSL_CTX_set_tmp_rsa(c, eph_rsa));
    RSA_free(eph_rsa); /* eph_rsa is dup'ed by SSL_CTX_set_tmp_rsa() */

    return 0;
err:
    u_dbg("%s", tls_get_error());
    if (eph_rsa)
        RSA_free(eph_rsa);    

    return ~0;
}
#endif

#ifdef SSL_OPENSSL
/* generate DH ephemeral parameters and load'em into SSL_CTX */
static int tls_gendh_params(SSL_CTX *c, const char *dhfile)
{
    DH *eph_dh = NULL;

    dbg_return_if (c == NULL, ~0);

    eph_dh = dhfile ? tls_load_dh_param(dhfile) : get_dh1024(); 
    dbg_err_if (!(eph_dh));

    dbg_err_if (!SSL_CTX_set_tmp_dh(c, eph_dh));
    DH_free(eph_dh);

#if 0
    /* Avoid small subgroup attacks (if p and g are strong primes
     * this is not strictly necessary).  This is said to have a negligible (?)
     * impact during negotiation phase. TODO: test it ! */
    (void) SSL_CTX_set_options(c, SSL_OP_SINGLE_DH_USE); */
#endif /* 0 */

    return 0;
err:
    u_dbg("%s", tls_get_error());
    if (eph_dh)
        DH_free(eph_dh);

    return ~0;
}

static DH *tls_load_dh_param (const char *res_name)
{
    DH *dh = NULL;
    BIO *bio = NULL;

    dbg_return_if (res_name == NULL, NULL);

    /* say return_if here instead of err_if because bio_from_emb()
     * could have failed for a non-openssl error */
    dbg_return_if (!(bio = tls_get_file_bio(res_name)), NULL);
    dbg_err_if (!(dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL)));

    BIO_free(bio);

    return dh;
err:
    u_dbg("%s", tls_get_error());
    if (bio) 
        BIO_free(bio);

    return NULL;
}
#endif

static int tls_no_passphrase_cb (char *buf, int num, int w, void *arg)
{
    u_unused_args(buf, num, w, arg);

    return -1;
}

static int tls_init_ctx_args (tls_ctx_args_t *cargs)
{
    dbg_return_if (!cargs, ~0);

    cargs->cert = NULL;
    cargs->key = NULL;
    cargs->ca = NULL;
    cargs->dh = NULL;
    cargs->crl = NULL;
#ifdef SSL_OPENSSL_PSK
    cargs->pskdb = NULL;
#endif
    cargs->crlopts = 0;
    cargs->depth = 1;
    cargs->vmode = SSL_VERIFY_NONE;

    return 0;
}

static int tls_set_ctx_vdepth (u_config_t *cfg, tls_ctx_args_t *cargs)
{
    u_config_t    *k;
    
    dbg_return_if (!cfg || !cargs, ~0);

    if (!u_config_get_subkey(cfg, "verify_depth", &k))
        cargs->depth = atoi(u_config_get_value(k));    

    return 0;
}

#ifdef SSL_OPENSSL_PSK
static int tls_set_ctx_psk_hash (u_config_t *cfg, tls_ctx_args_t *cargs)
{
    int rc; 

    dbg_return_if (cfg == NULL, ~0);
    dbg_return_if (cargs == NULL, ~0);

    /* default value is 0 (i.e. cleartext) */
    rc = u_config_get_subkey_value_b(cfg, "psk_hash", 0, &cargs->psk_is_hashed);
    dbg_return_ifm (rc, ~0, "bad value given to psk_hash directive");

    return 0;
}
#endif

static int tls_set_ctx_crlopts (u_config_t *cfg, tls_ctx_args_t *cargs)
{
    const char *v;
    
    dbg_return_if (cfg == NULL, ~0);
    dbg_return_if (cargs == NULL, ~0);

    v = u_config_get_subkey_value(cfg, "crl_opts");

    if (v == NULL)
    {
        cargs->crlopts = 0;
        return 0;
    }

#ifdef SSL_OPENSSL
    if (!strcasecmp(v, "check_all"))
        cargs->crlopts = X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL;
    else
        warn_err("unknown value %s for 'crl_opts' directive", v);
#else
    warn_err("CRLs not supported");
#endif

    return 0;
err:
    return ~0;
}

/* 'verify_mode' and 'verify_client' are aliases
 * the former is deprecated and retained only for compatibility with klone 1 */
static int tls_set_ctx_vmode (u_config_t *cfg, tls_ctx_args_t *cargs)
{
    const char *v;
    
    dbg_return_if (cfg == NULL, ~0);
    dbg_return_if (cargs == NULL, ~0);
    
    /* try 'verify_mode' directive first then 'verify_client' */
    if  ((v = u_config_get_subkey_value(cfg, "verify_mode")) == NULL)
        v = u_config_get_subkey_value(cfg, "verify_client");

    if (v == NULL || !strcasecmp(v, "no"))  /* unset == none */
        cargs->vmode = SSL_VERIFY_NONE;
    else if (!strcasecmp(v, "optional"))
        cargs->vmode = SSL_VERIFY_PEER;
    else if (!strcasecmp(v, "require"))
        cargs->vmode = SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    else
        warn_err("unknown verification value:\'%s\'", v);
    
    return 0;
err:
    return ~0;
}

static int tls_check_ctx (tls_ctx_args_t *cargs)
{
    dbg_return_if (cargs == NULL, ~0);

#ifdef SSL_OPENSSL_PSK
    /* if no PSK password file is set check for certificate/key */
    if (cargs->pskdb == NULL)
    {
#endif
        /* cert_file is a MUST */
        crit_err_ifm (!cargs->cert || strlen(cargs->cert) == 0, 
            "missing cert_file option parameter");

        /* if private key file is missing, assume the key is inside cert_file */
        warn_ifm (cargs->key == NULL, 
            "missing cert key option, assuming the key is inside cert_file");

        /* if verify_mode == "required" the CA file MUST be present */
        if (cargs->vmode & SSL_VERIFY_PEER)
            crit_err_ifm (cargs->ca == NULL, 
                "SSL verify is required but CA certificate file is missing");

#ifdef SSL_OPENSSL_PSK
    }
#endif

#ifdef SSL_OPENSSL
    /* if 'crl_file' was given, set crlopts at least to verify the client
     * certificate against the supplied CRL */
    if (cargs->crl && cargs->crlopts == 0)
        cargs->crlopts = X509_V_FLAG_CRL_CHECK;
#endif

    return 0;
err:
    return ~0;
}


static void tls_free_ctx_args (tls_ctx_args_t *cargs)
{
    KLONE_FREE(cargs);
    return;
}
#endif /* SSL_ON */
