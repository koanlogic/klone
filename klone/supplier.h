/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: supplier.h,v 1.4 2005/12/30 17:21:53 tat Exp $
 */

#ifndef _KLONE_SUPPLIER_H_
#define _KLONE_SUPPLIER_H_

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
    int (*is_valid_uri)(const char *buf, size_t len, time_t *mtime);
    int (*serve)(request_t *, response_t*);
} supplier_t;

#ifdef __cplusplus
}
#endif 

#endif
