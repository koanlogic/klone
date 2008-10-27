/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: emb.c,v 1.19 2008/10/27 21:28:04 tat Exp $
 */

#include <klone/emb.h>
#include <klone/io.h>
#include <klone/codecs.h>
#include <u/libu.h>
#include <u/toolbox/hmap.h>

/* these are klone-site autogen functions */
void register_pages(void);
void unregister_pages(void);

static u_hmap_t *embmap = NULL;     /* hashmap of embedded resources */
static int init = 0;

int emb_init(void)
{
    if(init++ == 0)
    {
        dbg_err_if (u_hmap_new(NULL, &embmap));

        /* call autogen external function (cannot be called more then once!) */
        dbg("registering embedded resources");
        register_pages();
    }

    return 0;
 
err:
    return ~0;
}

int emb_term(void)
{
    dbg_err_if(init == 0);

    unregister_pages();

    if (embmap) {
        u_hmap_free(embmap);
        embmap = NULL;
    }

    return 0;
err:
    return ~0;
}

int emb_register(embres_t *res)
{
    u_hmap_o_t *obj = NULL;

    dbg_err_if(init == 0 || res == NULL);

    if(res->type == ET_FILE) 
        dbg("registering %s (%s)", res->filename, 
                ((embfile_t*)res)->comp ? "compressed" : "uncompressed");
    else 
        dbg("registering %s", res->filename);

    obj = u_hmap_o_new((void *) res->filename, res); 
    dbg_err_if (obj == NULL);

    dbg_err_if (u_hmap_put(embmap, obj, NULL)); 
    
    return 0;
err:
    return ~0;
}

int emb_unregister(embres_t *res)
{
    dbg_err_if(init == 0 || res == NULL);

    dbg_err_if (u_hmap_del(embmap, (void *) res->filename, NULL));

    return 0;
err:
    return ~0;
}

int emb_lookup(const char *filename, embres_t **pr)
{
    embres_t *res;
    u_hmap_o_t *obj = NULL;

    dbg_err_if (init == 0);
    dbg_err_if (filename == NULL || filename[0] == 0);
    dbg_err_if (pr == NULL);

    nop_err_if (u_hmap_get(embmap, (void *) filename, &obj));

    *pr = obj->val;

    return 0;
err:
    /* not found */
    return ~0;
}

int emb_open(const char *file, io_t **pio)
{
    embfile_t *e = NULL;
    codec_t *gzip = NULL;
    io_t *io;

    dbg_return_if (pio == NULL, ~0);
    dbg_return_if (file == NULL, ~0);
    
    dbg_err_if(emb_lookup(file, (embres_t**)&e) || e->res.type != ET_FILE);

    dbg_err_if(io_mem_create((char*)e->data, e->size, 0, &io));

#ifdef HAVE_LIBZ
    if(e->comp)
    {
        dbg_err_if(codec_gzip_create(GZIP_UNCOMPRESS, &gzip));
        dbg_err_if(io_codec_add_tail(io, (codec_t*)gzip));
        gzip = NULL;
    }
#endif

    *pio = io;

    return 0;
err:
    if(gzip)
        codec_free(gzip);
    return ~0;
}

