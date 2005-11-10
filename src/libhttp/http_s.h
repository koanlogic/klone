#ifndef _KLONE_HTTP_S_H_
#define _KLONE_HTTP_S_H_
#include <klone/klone.h>
#include <klone/broker.h>
#include <klone/ses_prv.h>
#include <u/libu.h>
#include "conf.h"

#ifdef HAVE_LIBOPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif 

enum { HTTP_DEFAULT_IDLE_TIMEOUT = 10 };

struct http_s 
{
    u_config_t *config;     /* server config                                 */
    broker_t *broker;       /* pages broker                                  */
    int ssl;                /* >0 when SSL is enabled                        */
#ifdef HAVE_LIBOPENSSL
    SSL_CTX* ssl_ctx;       /* OpenSSL context                               */
#endif
    /* toplevel configuration options */
    const char *server_sig; /* server signature                              */
    const char *dir_root;   /* base html directory                           */
    const char *index;      /* user-provided index page                      */
    int idle_timeout;       /* max # of secs the server wait for the request */
    int send_enc_deflate;   /* >0 if sending deflated content is not disabled*/
    /* session options struct                        */
    session_opt_t *sess_opt;
};

#endif
