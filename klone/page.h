/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: page.h,v 1.8 2006/04/22 13:14:46 tat Exp $
 */

#ifndef _KLONE_PAGE_H_
#define _KLONE_PAGE_H_

#include "klone_conf.h"
#ifdef HAVE_STDINT
#include <stdint.h>
#endif /* HAVE_STDINT */
#include <u/libu.h>
#include <klone/response.h>
#include <klone/request.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*page_run_t)(request_t*, response_t*);

typedef enum page_type_e { 
    PAGE_TYPE_UNKNOWN, 
    PAGE_TYPE_STATIC, 
    PAGE_TYPE_DYNAMIC 
} page_type_t;

/* static content page */
typedef struct page_static_s
{
    size_t size;
    unsigned char *data;
} page_static_t;

/* dyunamic content page */
typedef struct page_dynamic_s
{
    page_run_t run; /* run page code func pointer  */
} page_dynamic_t;

/* define page list */
LIST_HEAD(pages_s, page_s);

struct page_s
{
    const char *uri;        /* *.kl1 file name              */
    const char *mime_type;  /* default mime type            */
    page_type_t type;       /* page type PAGE_TYPE_XXX      */
    void *sd;               /* static or dyn page context   */
    LIST_ENTRY(page_s) np;  /* next & prev pointers         */
};

#define PAGE_STATIC_INIT(uri, mime, type, ptr) \
    { uri, mime, type, ptr, LIST_ENTRY_NULL } 

typedef struct page_s page_t;
typedef struct pages_s pages_t; 

#ifdef __cplusplus
}
#endif 

#endif
