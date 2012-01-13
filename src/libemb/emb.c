/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: emb.c,v 1.23 2010/06/02 07:46:57 tho Exp $
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

static int listadd (const void *val, const void *arg);

int emb_init(void)
{
    u_hmap_opts_t *hopts = NULL;

    if(init++ == 0)
    {
        dbg_err_if (u_hmap_opts_new(&hopts));

        /* no free function needed for static values */
        dbg_err_if (u_hmap_opts_set_val_freefunc(hopts, NULL));

        dbg_err_if (u_hmap_easy_new(hopts, &embmap));
        hopts = NULL;

        /* call autogen external function (cannot be called more then once!) */
        u_dbg("registering embedded resources");
        register_pages();
    }

    return 0;
 
err:
    if (hopts)
        u_hmap_opts_free(hopts);

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
        u_dbg("registering %s (%s)", res->filename, 
                ((embfile_t*)res)->comp ? "compressed" : "uncompressed");
    else 
        u_dbg("registering %s", res->filename);

    dbg_err_if (u_hmap_easy_put(embmap, res->filename, (const void *) res));
    
    return 0;
err:
    return ~0;
}

int emb_unregister(embres_t *res)
{
    dbg_err_if(init == 0 || res == NULL);

    if (embmap)
        dbg_err_if (u_hmap_easy_del(embmap, res->filename));

    return 0;
err:
    return ~0;
}

int emb_lookup(const char *filename, embres_t **pr)
{
    embres_t *res;

    dbg_err_if (init == 0);
    dbg_err_if (filename == NULL);
    dbg_err_if (filename[0] == '\0');
    dbg_err_if (pr == NULL);

    res = u_hmap_easy_get(embmap, filename);
    /* dbg_err_ifm (res == NULL, "%s not found", filename); */
    nop_err_if (res == NULL);

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

int emb_list (char ***plist)
{
    size_t i;
    ssize_t nelems;
    char **list = NULL;

    /* See how many elements are registered. */
    dbg_err_if ((nelems = u_hmap_count(embmap)) <= 0);

    /* Create the array of strings needed to hold the list. */
    list = u_malloc(((size_t) nelems + 1) * sizeof(char *));
    dbg_err_sif (list == NULL);

    for (i = 0; i <= (size_t) nelems; i++)
        list[i] = NULL;

    /* Walk through the embfs to collect res names. */
    u_hmap_foreach_arg(embmap, listadd, list);

    /* Copy out result. */
    *plist = list;

    return 0;
err:
    if (list)
        emb_list_free(list);
    return ~0;
}

void emb_list_free (char **list)
{
    size_t i;

    if (list == NULL)
        return;

    for (i = 0; list[i] != NULL; i++)
        u_free(list[i]);

    u_free(list);

    return;
}

int emb_to_ubuf(const char *res_name, u_buf_t **pubuf)
{
    int c;
    char buf[1024];
    io_t *tmp = NULL;
    u_buf_t *ubuf = NULL;

    dbg_err_if(res_name == NULL);
    dbg_err_if(pubuf == NULL);

    dbg_err_if(emb_open(res_name, &tmp));

    dbg_err_if(u_buf_create(&ubuf));

    for (;;)
    {
        c = io_read(tmp, buf, sizeof(buf));

        if (c == 0)     /* EOF */
            break;
        else if (c < 0) /* read error */
            goto err;

        dbg_err_if (u_buf_append(ubuf, buf, c));
    }

    io_free(tmp); tmp = NULL;

    *pubuf = ubuf;

    return 0;
err:
    if (tmp) 
        io_free(tmp);
    if (ubuf)  
        u_buf_free(ubuf);

    return ~0;
}

/* Here 'val' is an embres_t object and 'arg' the res names' list. */
static int listadd (const void *val, const void *arg)
{
    size_t i;
    const char **list = (const char **) arg;
    const embres_t *res = (const embres_t *) val;

    /* Skip over already filled slots. */
    for (i = 0; list[i] != NULL; i++)
        ;

    /* Attach the resource filename. */
    dbg_err_sif ((list[i] = u_strdup(res->filename)) == NULL);

    return 0;
err:
    return ~0;
}

