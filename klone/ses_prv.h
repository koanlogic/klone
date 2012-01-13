/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ses_prv.h,v 1.19 2009/05/31 18:50:27 tho Exp $
 */

#ifndef _KLONE_SESPRV_H_
#define _KLONE_SESPRV_H_

#include "klone_conf.h"
#ifdef SSL_OPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif
#include <u/libu.h>
#include <klone/session.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/vars.h>
#include <klone/http.h>
#include <klone/atom.h>
#include <klone/md5.h>
#ifdef SSL_CYASSL
#include <config.h>
#include <types.h>
#include <ctc_hmac.h>
#include <openssl/evp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SESSION_KEY_VAR      "KLONE_CIPHER_KEY"

typedef int (*session_load_t)(session_t*);
typedef int (*session_save_t)(session_t*);
typedef int (*session_remove_t)(session_t*);
typedef int (*session_term_t)(session_t*);

/* session type */
enum { 
    SESSION_TYPE_UNKNOWN, 
    SESSION_TYPE_FILE, 
    SESSION_TYPE_MEMORY, 
    SESSION_TYPE_CLIENT
};

enum { 
    SESSION_ID_LENGTH = MD5_DIGEST_LEN,         /* sid length       */
    SESSION_ID_BUFSZ = 1 + SESSION_ID_LENGTH    /* sid buffer size  */
};

/* hmac and cipher key size */
enum { 
    HMAC_KEY_LEN = 64, 
    #ifdef SSL_OPENSSL
    CIPHER_KEY_LEN = EVP_MAX_KEY_LENGTH, 
    CIPHER_KEY_BUFSZ = 2* EVP_MAX_KEY_LENGTH,  /* key + padding */
    CIPHER_IV_LEN = EVP_MAX_IV_LENGTH
    #else
    CIPHER_KEY_LEN = 32, CIPHER_KEY_BUFSZ = 64, CIPHER_IV_LEN = 16
    #endif
 };

#ifdef SSL_CYASSL
typedef Hmac HMAC_CTX;
#endif

/* session runtime parameters */
typedef struct session_opt_s
{
    /* common session options */
    int type;       /* type of sessions (file, memory, client-side)  */
    int max_age;    /* max allowed age of sessions                   */
    int encrypt;    /* >0 when client-side session encryption is on  */
    int compress;   /* >0 when client-side session compression is on */
    char name[128]; /* cookie name                                   */

    /* file session options/struct                                   */
    char path[U_FILENAME_MAX]; /* session save path                  */
    unsigned char session_key[CIPHER_KEY_BUFSZ]; /* session secret key */
    unsigned char session_iv[CIPHER_IV_LEN];   /* session init vect */

    /* in-memory session options/struct                              */
    atoms_t *atoms; /* atom list used to store in-memory sessions    */
    size_t max_count;   /* max # of in-memory sessions               */
    size_t mem_limit;   /* max (total) size of in-memory sessions    */

    #ifdef SSL_ON
    char keyvar[128]; /* name of the session variable w/ the description key */
    const EVP_CIPHER *cipher; /* encryption cipher algorithm         */
    unsigned char cipher_key[CIPHER_KEY_BUFSZ]; /* cipher secret key  */
    unsigned char cipher_iv[CIPHER_IV_LEN];   /* cipher Init Vector */
    /* client-side options/structs                                   */
    HMAC_CTX hmac_ctx;  /* openssl HMAC context                      */
    const EVP_MD *hash; /* client-side session HMAC hash algorithm   */
    char hmac_key[HMAC_KEY_LEN]; /* session HMAC secret key         */
    #endif
} session_opt_t;

struct session_s
{
    vars_t *vars;               /* variable list                              */
    request_t *rq;              /* request bound to this session              */
    response_t *rs;             /* response bound to this session             */
    char filename[U_FILENAME_MAX];/* session filename                         */
    char id[SESSION_ID_BUFSZ];  /* session ID                                 */
    int removed;                /* >0 if the calling session has been deleted */
    int mtime;                  /* last modified time                         */
    session_load_t load;        /* ptr to the driver load function            */
    session_save_t save;        /* ptr to the driver save function            */
    session_remove_t remove;    /* ptr to the driver remove function          */
    session_term_t term;        /* ptr to the driver term function            */
    session_opt_t *so;          /* runtime option                             */
};

/* main c'tor */
int session_create(session_opt_t*, request_t*, response_t*, session_t**);

/* driver c'tor */
int session_client_create(session_opt_t*, request_t*, response_t*, session_t**);
int session_file_create(session_opt_t*, request_t*, response_t*, session_t**);
int session_mem_create(session_opt_t*, request_t*, response_t*, session_t**);

/* private functions */
int session_prv_init(session_t *, request_t *, response_t *);
int session_prv_load_from_io(session_t *, io_t *);
int session_prv_save_to_io(session_t*, io_t *);
int session_prv_save_var(var_t *, void*);
int session_prv_calc_maxsize(var_t *v, void *p);
int session_prv_save_to_buf(session_t *ss, char **pbuf, size_t *psz);
int session_prv_load_from_buf(session_t *ss, char *buf, size_t size);
int session_prv_set_id(session_t *ss, const char *sid);
int session_priv_set_id(session_t *ss, const char *sid); /* backward comp. */

/* init/term funcs */
int session_module_init(u_config_t *config, session_opt_t **pso);
int session_file_module_init(u_config_t *config, session_opt_t *pso);
int session_mem_module_init(u_config_t *config, session_opt_t *pso);
int session_client_module_init(u_config_t *config, session_opt_t *pso);
int session_module_term(session_opt_t *so);
int session_module_term(session_opt_t *so);

#ifdef __cplusplus
}
#endif 

#endif
