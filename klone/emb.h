/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: emb.h,v 1.18 2010/06/01 20:20:51 tho Exp $
 */

#ifndef _KLONE_EMB_H_
#define _KLONE_EMB_H_

#include "klone_conf.h"
#include <sys/stat.h>
#ifdef HAVE_STDINT
#include <stdint.h>
#endif /* HAVE_STDINT */
#include <u/libu.h>
#include <klone/klone.h>
#include <klone/dypage.h>

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
    unsigned char *data;    /* file data                                      */
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
    dypage_fun_t fun;
} embpage_t;

int emb_init(void);
int emb_term(void);
int emb_register(embres_t *r);
int emb_unregister(embres_t *r);
int emb_lookup(const char *filename, embres_t **pr);
int emb_count(void);
int emb_getn(size_t n, embres_t **pr);
int emb_open(const char *file, io_t **pio);

int emb_list (char ***plist);
void emb_list_free (char **list);
int emb_to_ubuf(const char *res_name, u_buf_t **pubuf);

#ifdef __cplusplus
}
#endif 

#endif
