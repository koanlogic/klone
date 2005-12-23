/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ses_prv.h,v 1.16 2005/12/23 10:14:57 tat Exp $
 */

#ifndef _KLONE_SESPRV_H_
#define _KLONE_SESPRV_H_

#include "klone_conf.h"
#ifdef HAVE_LIBOPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif /* HAVE_LIBOPENSSL */
#include <u/libu.h>
#include <klone/session.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/vars.h>
#include <klone/http.h>
#include <klone/atom.h>
#include <klone/md5.h>

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
    HMAC_KEY_SIZE = 64, 
    #ifdef HAVE_LIBOPENSSL
    CIPHER_KEY_SIZE = EVP_MAX_KEY_LENGTH, 
    CIPHER_IV_SIZE = EVP_MAX_IV_LENGTH
    #else
    CIPHER_KEY_SIZE = 64, CIPHER_IV_SIZE = 64
    #endif
 };

/* session runtime parameters */
typedef struct session_opt_s
{
    /* common session options */
    int type;       /* type of sessions (file, memory, client-side)  */
    int max_age;    /* max allowed age of sessions                   */
    int encrypt;    /* >0 when client-side session encryption is on  */
    int compress;   /* >0 when client-side session compression is on */
    #ifdef HAVE_LIBOPENSSL
    const EVP_CIPHER *cipher; /* encryption cipher algorithm         */
    unsigned char cipher_key[CIPHER_KEY_SIZE]; /* cipher secret key  */
    unsigned char cipher_iv[CIPHER_IV_SIZE];   /* cipher Init Vector */
    #endif

    /* file session options/struct                                   */
    char path[U_FILENAME_MAX]; /* session save path                  */
    unsigned char session_key[CIPHER_KEY_SIZE]; /* session secret key*/
    unsigned char session_iv[CIPHER_IV_SIZE];   /* session init vect */

    /* in-memory session options/struct                              */
    atoms_t *atoms; /* atom list used to store in-memory sessions    */
    size_t max_count;   /* max # of in-memory sessions               */
    size_t mem_limit;   /* max (total) size of in-memory sessions    */

    /* client-side options/structs                                   */
    #ifdef HAVE_LIBOPENSSL
    HMAC_CTX hmac_ctx;  /* openssl HMAC context                      */
    const EVP_MD *hash; /* client-side session HMAC hash algorithm   */
    char hmac_key[HMAC_KEY_SIZE]; /* session HMAC secret key         */
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

/* init/term funcs */
int session_module_init(u_config_t *config, session_opt_t **pso);
int session_file_module_init(u_config_t *config, session_opt_t *pso);
int session_mem_module_init(u_config_t *config, session_opt_t *pso);
int session_client_module_init(u_config_t *config, session_opt_t *pso);
int session_module_term(session_opt_t *so);
int session_module_term(session_opt_t *so);

#endif
