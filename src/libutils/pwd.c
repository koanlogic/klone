/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: pwd.c,v 1.3 2008/03/20 14:43:15 tho Exp $
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/emb.h>
#include <klone/utils.h>

static int __emb_open (const char *fqn, void **pio);
static char *__emb_load (char *str, int size, void *io);
static void __emb_close (void *io);
static int u_pwd_init_embfs (const char *fqn, u_pwd_hash_cb_t cb_hash, 
        size_t hash_len, int in_memory, u_pwd_t **ppwd);

/**
 *  \brief  Create a pwd instance from a master password db stored at \p fqn
 *
 *  \param  fqn         path name of the master password db (will be searched
 *                      in embfs and then in the file system 
 *  \param  hashed      boolean to tell if passwords are hashed or cleartext
 *  \param  in_memory   boolean to tell if a copy of the password db is also
 *                      kept in memory as an hash map (meaningful only for 
 *                      on-disk password db's)
 *  \param  ppwd        pwd instance as a result argument
 *
 *  \return \c 0 on success, \c ~0 on error
 */ 
int u_pwd_init_agnostic (const char *fqn, int hashed, int in_memory, 
        u_pwd_t **ppwd)
{
    int where;
    u_pwd_hash_cb_t hfn = NULL;
    size_t hlen = 0;

    dbg_return_if (fqn == NULL, ~0); 
    dbg_return_if (ppwd == NULL, ~0); 

    dbg_err_if (u_path_where_art_thou(fqn, &where));

    if (hashed)
    {
        hfn = u_md5;
        hlen = MD5_DIGEST_BUFSZ;
    }

    switch (where)
    {
        case U_PATH_IN_EMBFS:
            return u_pwd_init_embfs(fqn, hfn, hlen, in_memory, ppwd);
        case U_PATH_IN_FS:
            return u_pwd_init_file(fqn, hfn, hlen, in_memory, ppwd);
        default:
            dbg_err("%s: resource not found !", fqn);
    }

    /* not reached */

err:
    return ~0;
}

static int __emb_open (const char *fqn, void **pio)
{
    io_t *io = NULL;

    dbg_err_if (emb_open(fqn, &io));

    *pio = (void *) io;

    return 0;
err:
    return ~0;
}

static void __emb_close (void *io)
{
    dbg_if (io_close((io_t *) io));
    return;
}

static char *__emb_load (char *str, int size, void *io)
{
    dbg_err_if (io_gets((io_t *) io, str, size) <= 0);

    return str;
err:
    return NULL;
}

static int u_pwd_init_embfs (const char *fqn, u_pwd_hash_cb_t cb_hash, 
        size_t hash_len, int in_memory, u_pwd_t **ppwd)
{
    return u_pwd_init(fqn, __emb_open, __emb_load, __emb_close, NULL, 
            cb_hash, hash_len, in_memory, ppwd);
}
