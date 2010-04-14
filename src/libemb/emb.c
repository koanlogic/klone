/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: emb.c,v 1.20 2010/04/14 19:26:16 stewy Exp $
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
    u_hmap_opts_t hopts;

    if(init++ == 0)
    {
        u_hmap_opts_init(&hopts);

        /* no free function needed for static values */
        dbg_err_if (u_hmap_opts_set_val_freefunc(&hopts, NULL));

        dbg_err_if (u_hmap_easy_new(&hopts, &embmap));

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

    U_FREEF(embmap, u_hmap_easy_free);

    return 0;
err:
    return ~0;
}

int emb_register(embres_t *res)
{
    dbg_err_if(init == 0 || res == NULL);

    if(res->type == ET_FILE) 
        dbg("registering %s (%s)", res->filename, 
                ((embfile_t*)res)->comp ? "compressed" : "uncompressed");
    else 
        dbg("registering %s", res->filename);

    dbg_err_if (u_hmap_easy_put(embmap, res->filename, (const void *) res));
    
    return 0;
err:
    return ~0;
}

int emb_unregister(embres_t *res)
{
    dbg_err_if(init == 0 || res == NULL);

    dbg_err_if (u_hmap_easy_del(embmap, res->filename));

    return 0;
err:
    return ~0;
}

int emb_lookup(const char *filename, embres_t **pr)
{
    embres_t *res;

    dbg_err_if (init == 0);
    dbg_err_if (filename == NULL || filename[0] == 0);
    dbg_err_if (pr == NULL);

    res = u_hmap_easy_get(embmap, filename);
    dbg_err_if (res == NULL);

    *pr = res;

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

