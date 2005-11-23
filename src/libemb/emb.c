/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: emb.c,v 1.11 2005/11/23 19:31:09 tho Exp $
 */

#include <klone/emb.h>
#include <klone/io.h>
#include <klone/codecs.h>
#include <u/libu.h>

/* these are klone-site autogen functions */
int register_pages(void);
int unregister_pages(void);

static struct emblist_s list;   /* list of emb resources     */
static size_t nres;             /* num of emb resources      */
static int init = 0;

int emb_init(void)
{
    if(init++ == 0)
    {
        LIST_INIT(&list); /* init resource list */

        /* call autogen external function (cannot be called more then once!) */
        dbg("registering embedded resources");
        register_pages();
    }

    return 0;
}

int emb_term(void)
{
    dbg_err_if(init == 0);

    unregister_pages();

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

    LIST_INSERT_HEAD(&list, res, np);
    nres++;

    return 0;
err:
    return ~0;
}

int emb_unregister(embres_t *res)
{
    dbg_err_if(init == 0 || res == NULL);

    LIST_REMOVE(res, np);
    nres--;

    return 0;
err:
    return ~0;
}

int emb_lookup(const char *filename, embres_t **pr)
{
    embres_t *res;

    dbg_err_if (init == 0);
    dbg_err_if (filename == NULL || !strlen(filename));
    dbg_err_if (pr == NULL);

    LIST_FOREACH(res, &list, np)
    {
        if(strcmp(filename, res->filename))
            continue;

        /* save found resource pointer */
        *pr = res;

        return 0; /* found */
    }

err:
    /* not found */
    return ~0;
}

int emb_count(void)
{
    dbg_err_if (init == 0);

    return nres;
err:
    return -1;
}

int emb_getn(size_t n, embres_t **pr)
{
    embres_t *res = NULL;

    dbg_err_if (init == 0);
    dbg_err_if (n >= nres);
    dbg_err_if (pr == NULL);

    LIST_FOREACH(res, &list, np)
    {
        if(n-- == 0)
            break;
    }

    *pr = res;

    return 0;
err:
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

    dbg_err_if(io_mem_create(e->data, e->size, 0, &io));

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

