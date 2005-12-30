/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: emb.h,v 1.11 2005/12/30 17:21:53 tat Exp $
 */

#ifndef _KLONE_EMB_H_
#define _KLONE_EMB_H_

#include "klone_conf.h"
#include <sys/stat.h>
#ifdef HAVE_STDINT
#include <stdint.h>
#endif /* HAVE_STDINT */
#include <u/libu.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/session.h>
#include <klone/io.h>
#include <klone/codecs.h>
#include <klone/utils.h>

#ifdef __cplusplus
extern "C" {
#endif 

/* supported embedded resource type */
enum {
    ET_FILE,                /* embedded file                */
    ET_PAGE                 /* dynamic web page             */
};

/* define resource list */
LIST_HEAD(emblist_s, embres_s);

/* common struct for embedded resources */
typedef struct embres_s
{
    LIST_ENTRY(embres_s) np;/* next & prev pointers         */
    const char *filename;   /* emb resource file name       */
    int type;               /* emb resource type (ET_*)     */
} embres_t;

/* embedded file */
typedef struct embfile_s
{
    embres_t res;           /* any emb resource must start with a embres_t    */
    size_t size;            /* size of the data block                         */
    uint8_t *data;          /* file data                                      */
    int comp;               /* if data is compressed                          */
    int encrypted;          /* if data is encrypted                           */
    time_t mtime;           /* time of last modification                      */
    const char *mime_type;  /* guessed mime type                              */
    size_t file_size;       /* size of the source file (not compressed)       */
} embfile_t;

/* embedded dynamic klone page */
typedef struct embpage_s
{
    embres_t res;           /* any emb resource must start with a embres_t  */
    void (*run)(request_t*, response_t*, session_t*);   /* page code        */
} embpage_t;

int emb_init();
int emb_term();
int emb_register(embres_t *r);
int emb_unregister(embres_t *r);
int emb_lookup(const char *filename, embres_t **pr);
int emb_count();
int emb_getn(size_t n, embres_t **pr);
int emb_open(const char *file, io_t **pio);

#ifdef __cplusplus
}
#endif 

#endif
