/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: broker.c,v 1.8 2005/11/23 23:38:38 tho Exp $
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/supplier.h>
#include <klone/broker.h>
#include <klone/request.h>

enum { AVAIL_SUP_COUNT = 2 }; /* number of existing supplier types */

extern supplier_t sup_emb;
extern supplier_t sup_fs;

struct broker_s
{
    supplier_t *sup_list[AVAIL_SUP_COUNT + 1];
};

int broker_is_valid_uri(broker_t *b, const char *buf, size_t len)
{
    int i;
    time_t mtime;

    for(i = 0; b->sup_list[i]; ++i)
        if(b->sup_list[i]->is_valid_uri(buf, len, &mtime))
            return 1; /* found */

    return 0;
}

int broker_serve(broker_t *b, request_t *rq, response_t *rs)
{
    const char *file_name;
    int i;
    time_t mtime, ims;

    file_name = request_get_resolved_filename(rq);
    for(i = 0; b->sup_list[i]; ++i)
    {   
        if(b->sup_list[i]->is_valid_uri(file_name, strlen(file_name), &mtime) )
        {
            ims = request_get_if_modified_since(rq);
            if(ims && ims >= mtime)
            {
                response_set_status(rs, HTTP_STATUS_NOT_MODIFIED); 
                dbg_err_if(response_print_header(rs));
            } else
                dbg_err_if(b->sup_list[i]->serve(rq, rs));

            return 0;
        }
    }

    response_set_status(rs, HTTP_STATUS_NOT_FOUND); 

    return ~0; /* page not found */
err:
    return ~0;
}

static u_config_t* broker_get_request_config(request_t *rq)
{
    u_config_t *config = NULL;
    http_t *http;

    http = request_get_http(rq);
    if(http)
        config = http_get_config(http);
    
    return config;
}

int broker_create(broker_t **pb)
{
    broker_t *b = NULL;
    int i;

    b = u_zalloc(sizeof(broker_t));
    dbg_err_if(b == NULL);

    i = 0;
    b->sup_list[i++] = &sup_emb;
#if ENABLE_SUP_FS
    b->sup_list[i++] = &sup_fs;
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

    for(i = 0; b->sup_list[i]; ++i)
        b->sup_list[i]->term();

    U_FREE(b);

    return 0;
}

