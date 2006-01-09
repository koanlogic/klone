/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: http.h,v 1.9 2006/01/09 12:38:37 tat Exp $
 */

#ifndef _KLONE_HTTP_H_
#define _KLONE_HTTP_H_

#include <u/libu.h>

#ifdef __cplusplus
extern "C" {
#endif 

/** \file */

/** HTTP Methods */
enum http_method_e
{ 
    HM_UNKNOWN,   /**< unknown value */
    HM_GET,       /**< retrieve data at URI */
    HM_HEAD,      /**< ~HM_GET with headers only */
    HM_POST,      /**< create new object subordinate to specified object */
    HM_PUT,       /**< data in body is to be stored under URL */
    HM_DELETE     /**< deletion request at given URL */
};

struct http_s;
typedef struct http_s http_t;

struct session_opt_s;

u_config_t *http_get_config(http_t* http);
struct session_opt_s *http_get_session_opt(http_t* http);

int http_alias_resolv(http_t *h, char *dst, const char *filename, size_t sz);
const char* http_get_status_desc(int status);

#ifdef __cplusplus
}
#endif 

#endif
