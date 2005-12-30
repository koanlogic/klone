/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: session.h,v 1.7 2005/12/30 17:21:53 tat Exp $
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
int session_set(session_t*, const char*, const char*);
int session_del(session_t*, const char*);
int session_save_to_io(session_t*, const char*);

#ifdef __cplusplus
}
#endif 

#endif
