/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: tls.h,v 1.11 2008/07/10 08:56:13 tat Exp $
 */

#ifndef _KLONE_TLS_H_
#define _KLONE_TLS_H_

#include "klone_conf.h"
#include <u/libu.h>
#ifdef SSL_ON
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

int tls_init (void);
SSL_CTX *tls_load_init_ctx (u_config_t *);

#ifdef __cplusplus
}
#endif 

#endif /* SSL_ON */

#endif /* !_KLONE_TLS_H */
