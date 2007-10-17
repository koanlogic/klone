/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: supplier.h,v 1.6 2007/10/17 22:58:35 tat Exp $
 */

#ifndef _KLONE_SUPPLIER_H_
#define _KLONE_SUPPLIER_H_

#include <klone/http.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/page.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct supplier_s
{
    const char *name;       /* descriptive name          */
    int (*init)(void);
    void (*term)(void);
    int (*is_valid_uri)(http_t *h, const char *buf, size_t len, time_t *mtime);
    int (*serve)(request_t *, response_t*);
} supplier_t;

#ifdef __cplusplus
}
#endif 

#endif
