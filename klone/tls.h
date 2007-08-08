/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: tls.h,v 1.10 2007/08/08 22:42:51 tho Exp $
 */

#ifndef _KLONE_TLS_H_
#define _KLONE_TLS_H_

#include "klone_conf.h"
#include <u/libu.h>
#ifdef HAVE_LIBOPENSSL
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

SSL_CTX *tls_load_init_ctx (u_config_t *);

#ifdef __cplusplus
}
#endif 

#endif /* HAVE_LIBOPENSSL */

#endif /* !_KLONE_TLS_H */
