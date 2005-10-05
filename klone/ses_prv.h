#ifndef _KLONE_SESPRV_H_
#define _KLONE_SESPRV_H_
#include <klone/session.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/config.h>
#include <klone/vars.h>
#include <klone/http.h>
#include "conf.h"

#ifdef HAVE_LIBOPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

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

/* hmac and cipher key size */
enum { 
    HMAC_KEY_SIZE = 64, 
    #ifdef HAVE_LIBOPENSSL
    CIPHER_KEY_SIZE = EVP_MAX_KEY_LENGTH, CIPHER_IV_SIZE = EVP_MAX_IV_LENGTH
    #else
    CIPHER_KEY_SIZE = 64, CIPHER_IV_SIZE = 64
    #endif
 };

/* session runtime parameters */
typedef struct session_opt_s
{
    /* session related options */
    int type;       /* type of sessions (file, memory, client-side)  */
    int max_age;    /* max allowed age of sessions                   */
    int encrypt;    /* >0 when client-side session encryption is on  */
    int compress;   /* >0 when client-side session compression is on */
#ifdef HAVE_LIBOPENSSL
    HMAC_CTX hmac_ctx;  /* openssl HMAC context                      */
    const EVP_MD *hash; /* client-side session HMAC hash algorithm   */
    const EVP_CIPHER *cipher; /* encryption cipher algorithm         */
    char hmac_key[HMAC_KEY_SIZE]; /* session HMAC secret key         */
    char cipher_key[CIPHER_KEY_SIZE];   /* cipher secret key         */
    char cipher_iv[CIPHER_IV_SIZE];     /* cipher Init Vector        */
    EVP_CIPHER_CTX cipher_enc_ctx;      /* encrypt context           */
    EVP_CIPHER_CTX cipher_dec_ctx;      /* decrypt context           */
#endif
    char path[PATH_MAX + 1]; /* session save path                    */
} session_opt_t;

struct session_s
{
    vars_t *vars;               /* variable list                              */
    request_t *rq;              /* request bound to this session              */
    response_t *rs;             /* response bound to this session             */
    char filename[PATH_MAX];    /* session filename                           */
    char id[MD5_DIGEST_BUFSZ];  /* session ID                                 */
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
int session_prv_load(session_t *, io_t *);
int session_prv_save_var(var_t *, io_t *);

/* init/term funcs */
int session_module_init(config_t *config, session_opt_t **pso);
int session_module_term(session_opt_t *so);

#endif
