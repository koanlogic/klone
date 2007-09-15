/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: http_s.h,v 1.6 2007/09/15 16:36:12 tat Exp $
 */

#ifndef _KLONE_HTTP_S_H_
#define _KLONE_HTTP_S_H_
#include "klone_conf.h"
#include <u/libu.h>
#include <klone/os.h>
#include <klone/broker.h>
#include <klone/ses_prv.h>

#ifdef HAVE_LIBOPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif 

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
    int send_enc_deflate;   /* >0 if sending deflated content is not disabled*/
    /* session options struct                        */
    session_opt_t *sess_opt;
};

#endif
