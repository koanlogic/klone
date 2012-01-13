/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: session.h,v 1.10 2009/10/23 14:08:28 tho Exp $
 */

#ifndef _KLONE_SESSION_H_
#define _KLONE_SESSION_H_

#include <u/libu.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/vars.h>

#ifdef __cplusplus
extern "C" {
#endif

struct session_s;
typedef struct session_s session_t;

int session_free(session_t*);
int session_remove(session_t*);
int session_clean(session_t*);
int session_age(session_t*);

vars_t *session_get_vars(session_t*);
const char *session_get(session_t*, const char*);
const char *session_get_id (session_t *ss);
int session_set(session_t*, const char*, const char*);
int session_del(session_t*, const char*);
int session_load(session_t *ss);
int session_save(session_t *ss);
int session_save_to_io(session_t*, const char*);

#ifdef SSL_ON
int session_set_cipher_key(session_t*, const char*, size_t);
int session_get_cipher_key(session_t*, char*, size_t*);
#endif

#ifdef __cplusplus
}
#endif 

#endif
