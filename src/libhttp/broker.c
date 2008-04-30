/*
 * Copyright (c) 2005, 2006, 2007 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: broker.c,v 1.20 2008/04/30 13:00:00 tat Exp $
 */

#include <u/libu.h>
#include <klone/supplier.h>
#include <klone/broker.h>
#include <klone/request.h>
#include <klone/http.h>
#include "klone_conf.h"

enum { MAX_SUP_COUNT = 8 }; /* max number of suppliers */

extern supplier_t sup_emb;
#ifdef ENABLE_SUP_CGI
extern supplier_t sup_cgi;
#endif
#ifdef ENABLE_SUP_FS
extern supplier_t sup_fs;
#endif

struct broker_s
{
    supplier_t *sup_list[MAX_SUP_COUNT + 1];
};

int broker_is_valid_uri(broker_t *b, http_t *h, request_t *rq, const char *buf,
        size_t len)
{
    int i;
    time_t mtime;

    dbg_goto_if (b == NULL, notfound);
    dbg_goto_if (buf == NULL, notfound);
    
    for(i = 0; b->sup_list[i]; ++i)
        if(b->sup_list[i]->is_valid_uri(h, rq, buf, len, &mtime))
            return 1; /* found */

notfound:
    return 0;
}

int broker_serve(broker_t *b, http_t *h, request_t *rq, response_t *rs)
{
    const char *file_name;
    int i;
    time_t mtime, ims;

    dbg_err_if (b == NULL);
    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    
    file_name = request_get_resolved_filename(rq);
    for(i = 0; b->sup_list[i]; ++i)
    {   
        if(b->sup_list[i]->is_valid_uri(h, rq, file_name, strlen(file_name), 
                    &mtime) )
        {
            ims = request_get_if_modified_since(rq);
            if(ims && ims >= mtime)
            {
                response_set_status(rs, HTTP_STATUS_NOT_MODIFIED); 
                dbg_err_if(response_print_header(rs));
            } else {
                dbg_err_if(b->sup_list[i]->serve(rq, rs));

                /* if the user explicitly set the status from a kl1 return 0 */
                if(response_get_status(rs) >= 400 && b->sup_list[i] != &sup_emb)
                    return response_get_status(rs);
            }

            return 0; /* page successfully served */
        }
    }

    response_set_status(rs, HTTP_STATUS_NOT_FOUND); 
    dbg("404, file not found: %s", request_get_filename(rq));

err:
    return HTTP_STATUS_NOT_FOUND; /* page not found */
}

int broker_create(broker_t **pb)
{
    broker_t *b = NULL;
    int i;

    dbg_err_if (pb == NULL);

    b = u_zalloc(sizeof(broker_t));
    dbg_err_if(b == NULL);

    i = 0;
    b->sup_list[i++] = &sup_emb;

#ifdef ENABLE_SUP_CGI
    b->sup_list[i++] = &sup_cgi;
#else
#ifndef OS_WIN
    warn("CGI support disabled, use --enable_cgi to enable it");
#endif
#endif

#ifdef ENABLE_SUP_FS
    b->sup_list[i++] = &sup_fs;
#else
    warn("File system support disabled, use --enable_fs to enable it");
#endif

    b->sup_list[i++] = NULL;

    for(i = 0; b->sup_list[i]; ++i)
        dbg_err_if(b->sup_list[i]->init());

    *pb = b;

    return 0;
err:
    if(b)
        broker_free(b);
    return ~0;
}

int broker_free(broker_t *b)
{
    int i;

    if (b)
    {
        for(i = 0; b->sup_list[i]; ++i)
            b->sup_list[i]->term();

        U_FREE(b);
    }

    return 0;
}

