/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ses_mem.c,v 1.28 2007/12/07 16:37:56 tat Exp $
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <u/libu.h>
#include <klone/session.h>
#include <klone/request.h>
#include <klone/context.h>
#include <klone/response.h>
#include <klone/vars.h>
#include <klone/utils.h>
#include <klone/atom.h>
#include <klone/ses_prv.h>
#include <klone/ppc.h>
#include <klone/ppc_cmd.h>

enum { SESSION_FILENAME_MAX_LENGTH = 256 };

struct enc_ses_mem_s
{
    time_t mtime;                                   /* modification time    */
    char filename[SESSION_FILENAME_MAX_LENGTH];     /* session filename     */
    size_t size;                                    /* data size            */
    char data[1];                                   /* data block           */
};

typedef struct enc_ses_mem_s enc_ses_mem_t;

static int so_atom_delete_oldest(session_opt_t *so)
{
    atom_t *atom, *oldest;
    size_t count, i;

    dbg_err_if (so == NULL);

    count = atoms_count(so->atoms);
    nop_err_if(count == 0);

    dbg_err_if(atoms_getn(so->atoms, 0, &oldest));
    for(i = 1; i < count; ++i)
    {
        dbg_err_if(atoms_getn(so->atoms, i, &atom));

        /* save if older */
        if(atom->arg <= oldest->arg)
            oldest = atom;
    }

    dbg_err_if(atoms_remove(so->atoms, oldest));

    return 0;
err:
    return ~0;
}

static int session_delete_oldest(session_opt_t *so)
{
    ppc_t *ppc;

    dbg_err_if (so == NULL);

    if(ctx->pipc)
    {   /* children context, delete parent atom */
        ppc = server_get_ppc(ctx->server);
        dbg_err_if(ppc == NULL);

        dbg_err_if(ppc_write(ppc, ctx->pipc, PPC_CMD_MSES_DELOLD, "", 1) < 0);

        /* delete it from the local copy of atom list */
        dbg_err_if(so_atom_delete_oldest(so));
    } else {
        /* parent context */
        dbg_err_if(so_atom_delete_oldest(so));
    }

    return 0;
err:
    return ~0;
}

static int so_atom_remove(session_opt_t *so, const char *id)
{
    atom_t *atom = NULL;

    dbg_err_if (so == NULL);
    dbg_err_if (id == NULL);

    /* find the atom bound to this session */
    if(atoms_get(so->atoms, id, &atom))
        return 0;

    /* remove it from the list */
    dbg_err_if(atoms_remove(so->atoms, atom));

    atom_free(atom);

    return 0;
err:
    return ~0;
}


static int so_atom_add(session_opt_t *so, const char *id, char *buf, 
    size_t size, void *arg)
{
    atom_t *atom = NULL, *old = NULL;
    size_t new_size, old_found = 0;

    dbg_err_if (so == NULL);
    dbg_err_if (id == NULL);
    dbg_err_if (buf == NULL);

    /* get the old atom associated to this id */
    if(!atoms_get(so->atoms, id, &old))
        old_found = 1;

    /* delete the oldest session if there are already max_count sessions */
    if(so->max_count && atoms_count(so->atoms) - old_found >= so->max_count)
        dbg_err_if(session_delete_oldest(so));

    /* delete the oldest session(s) if we are using already mem_limit bytes */
    if(so->mem_limit)
    {
        warn_err_ifm(size > so->mem_limit, 
            "session size is bigger the memory.limit, save aborted...");
        for(;;)
        {
            /* new_size = size of all atoms + size of the atom we're going to
               add - the size of the atom (if found) we're going to remove */
            new_size = atoms_size(so->atoms) + size - (old ? old->size : 0);
            if(atoms_count(so->atoms) && new_size > so->mem_limit)
                dbg_err_if(session_delete_oldest(so));
            else
                break;
        }
    }

    /* create a new atom */
    dbg_err_if(atom_create(id, buf, size, arg, &atom));

    /* add it to the list */
    dbg_err_if(atoms_add(so->atoms, atom));

    /* remove the old atom associated to this id */
    if(old)
    {
        dbg_err_if(atoms_remove(so->atoms, old));
        atom_free(old);
    }

    return 0;
err:
    if(atom)
        atom_free(atom);
    return ~0;
}


/* [parent] remove a sessioin*/
static int session_cmd_remove(ppc_t *ppc, int fd, unsigned char cmd, 
    char *data, size_t size, void *vso)
{
    session_opt_t *so = vso;

    u_unused_args(ppc, fd, cmd, size);

    dbg_err_if (data == NULL);
    dbg_err_if (vso == NULL);

    dbg_err_if(so_atom_remove(so, data /* filename */));

    return 0;
err:
    return ~0;
}

/* [parent] delete oldest session */
static int session_cmd_delold(ppc_t *ppc, int fd, unsigned char cmd, 
    char *data, size_t size, void *vso)
{
    session_opt_t *so = vso;

    u_unused_args(ppc, fd, cmd, data, size);

    dbg_err_if (vso == NULL);

    dbg_err_if (so_atom_delete_oldest(so));

    return 0;
err:
    return ~0;
}

/* [parent] save a session */
static int session_cmd_save(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size, void *vso)
{
    session_opt_t *so = vso;
    enc_ses_mem_t *esm = (enc_ses_mem_t*)data;

    u_unused_args(ppc, fd, cmd, size);

    dbg_err_if (vso == NULL);
    dbg_err_if (data == NULL);

    dbg_err_if(so_atom_add(so, esm->filename, esm->data, esm->size, 
        (void*)esm->mtime));

    return 0;
err:
    return ~0;
}

/* [parent] get session data */
static int session_cmd_get(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size, void *vso)
{
    enum { BUFSZ = 4096 };
    session_opt_t *so = vso;
    enc_ses_mem_t *esm = NULL;
    atom_t *atom = NULL;
    char buf[BUFSZ];
    size_t esm_size;

    u_unused_args(cmd, size);

    dbg_err_if (ppc == NULL);
    dbg_err_if (vso == NULL);
    dbg_err_if (data == NULL);
    dbg_err_if (strlen(data) > SESSION_FILENAME_MAX_LENGTH);

    /* find the atom whose name is stored into 'data' buffer */
    nop_err_if(atoms_get(so->atoms, data, &atom));

    /* if the buffer on the stack is big enough use it, otherwise alloc a 
       bigger one on the heap */
    if((esm_size = sizeof(enc_ses_mem_t) + atom->size) > BUFSZ)
    {
        esm = u_malloc(1 + esm_size);
        dbg_err_if(esm == NULL);
    } else
        esm = (enc_ses_mem_t*)buf;
        
    /* fill the enc_ses_mem_t struct */
    esm->mtime = (time_t)atom->arg;
    u_strlcpy(esm->filename, data, sizeof esm->filename);
    esm->size = atom->size;
    memcpy(esm->data, atom->data, atom->size);

    dbg_err_if(ppc_write(ppc, fd, PPC_CMD_RESPONSE_OK, (char*)esm,
        esm_size) <= 0);

    if(esm && esm != (void*)buf)
        U_FREE(esm);

    return 0;
err:
    if(ppc)
        ppc_write(ppc, fd, PPC_CMD_RESPONSE_ERROR, (char *)"", 1);
    if(esm && esm != (void *)buf)
        U_FREE(esm);
    return ~0;
}

/* add an atom to the list of global atoms */
static int session_mem_add(session_opt_t *so, const char *filename, char *buf, 
    size_t size, time_t mtime)
{
    atom_t *atom = NULL;
    enc_ses_mem_t *esm = NULL;
    ppc_t *ppc;
    size_t esize;

    dbg_err_if (so == NULL);
    dbg_err_if (filename == NULL);
    dbg_err_if (buf == NULL);

    if(ctx->pipc)
    {   /* children context */
        ppc = server_get_ppc(ctx->server);
        dbg_err_if(ppc == NULL);

        /* build the encoded parameters structure */
        esize = size + sizeof(enc_ses_mem_t);
        esm = (enc_ses_mem_t*)u_malloc(esize);
        dbg_err_if(esm == NULL);

        /* fill esm fields */
        esm->mtime = time(0);
        esm->size = size;
        dbg_err_if(strlen(filename) > SESSION_FILENAME_MAX_LENGTH); 
        u_strlcpy(esm->filename, filename, sizeof esm->filename);
        memcpy(esm->data, buf, size);

        /* send the command request */
        dbg_err_if(ppc_write(ppc, ctx->pipc, PPC_CMD_MSES_SAVE, 
            (char*)esm, esize) < 0);

        U_FREE(esm);

        /* add it to the local copy of atom list */
        dbg_err_if(so_atom_add(so, filename, buf, size, (void*)mtime));

    } else {
        /* parent context */
        dbg_err_if(so_atom_add(so, filename, buf, size, (void*)mtime));
    }

    return 0;
err:
    U_FREE(esm);
    if(atom)
        atom_free(atom);
    return ~0;
}


static int session_mem_save(session_t *ss)
{
    char *buf = NULL;
    size_t sz;

    dbg_err_if (ss == NULL);
    
    /* save the session data to freshly alloc'd buf of size sz */
    dbg_err_if(session_prv_save_to_buf(ss, &buf, &sz));

    /* add the session to the in-memory session list */
    dbg_err_if(session_mem_add(ss->so, ss->filename, buf, sz, time(0)));

    U_FREE(buf);

    return 0;
err:
    U_FREE(buf);
    return ~0;
}

static int session_mem_load(session_t *ss)
{
    enum { BUFSZ = 4096 };
    atom_t *atom;
    char buf[BUFSZ];
    size_t size;
    enc_ses_mem_t *esm;
    ppc_t *ppc;
    unsigned char cmd;

    dbg_err_if (ss == NULL);
    nop_err_if (ss->filename == NULL || strlen(ss->filename) == 0);

    /* in fork and iterative model we can get the session from the current
       address space, in prefork we must ask the parent for a fresh copy of 
       the session */
    if(ctx->backend && ctx->backend->model == SERVER_MODEL_PREFORK)
    {   /* get the session data through ppc */
        ppc = server_get_ppc(ctx->server);
        dbg_err_if(ppc == NULL);

        /* send a get request */
        dbg_err_if(ppc_write(ppc, ctx->pipc, PPC_CMD_MSES_GET, ss->filename, 
            strlen(ss->filename) + 1) < 0);

        /* get the response from the parent */
        dbg_err_if((size = ppc_read(ppc, ctx->pipc, &cmd, buf, BUFSZ)) <= 0);

        nop_err_if(cmd != PPC_CMD_RESPONSE_OK);

        /* load session from esm */
        esm = (enc_ses_mem_t*)buf;
        ss->mtime = esm->mtime;
        dbg_err_if(session_prv_load_from_buf(ss, esm->data, esm->size));

    } else {
        /* find the file into the atom list */
        if(atoms_get(ss->so->atoms, ss->filename, &atom))
            return ~0; /* not found */

        /* copy stored mtime */
        ss->mtime = (time_t)atom->arg;

        /* load session from atom->data */
        dbg_err_if(session_prv_load_from_buf(ss, atom->data, atom->size));
    }

    return 0;
err:
    return ~0;
}

static int session_mem_remove(session_t *ss)
{
    ppc_t *ppc;

    dbg_err_if (ss == NULL);
    
    if(ctx->pipc)
    {   /* children context */
        ppc = server_get_ppc(ctx->server);
        dbg_err_if(ppc == NULL);

        dbg_err_if(ppc_write(ppc, ctx->pipc, PPC_CMD_MSES_REMOVE, ss->filename, 
            strlen(ss->filename) + 1) < 0);

        /* remove the session-atom from the (local copy) atom list */
        dbg_err_if(so_atom_remove(ss->so, ss->filename));
    } else {
        /* parent context */
        dbg_err_if(so_atom_remove(ss->so, ss->filename));
    }

    return 0;
err:
    return ~0;
}

static int session_mem_term(session_t *ss)
{
    /* nothing to do */
    u_unused_args(ss);
    return 0;
}

int session_mem_create(session_opt_t *so, request_t *rq, response_t *rs, 
        session_t **pss)
{
    session_t *ss = NULL;

    dbg_err_if (so == NULL);
    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    dbg_err_if (pss == NULL);

    ss = u_zalloc(sizeof(session_t));
    dbg_err_if(ss == NULL);

    ss->load = session_mem_load;
    ss->save = session_mem_save;
    ss->remove = session_mem_remove;
    ss->term = session_mem_term;
    ss->mtime = time(0);
    ss->so = so;

    dbg_err_if(session_prv_init(ss, rq, rs));

    *pss = ss;

    return 0;
err:
    if(ss)
        session_free(ss);
    return ~0;
}

/* this function will be called once by the server during startup */
int session_mem_module_init(u_config_t *config, session_opt_t *so)
{
    ppc_t *ppc;
    u_config_t *c;
    const char *v;

    /* config may be NULL */
    dbg_err_if (so == NULL);

    /* defaults */
    so->max_count = 0;  /* no limits */
    so->mem_limit = 0;  /* no limits */

    if(config && u_config_get_subkey(config, "memory", &c) == 0)
    {
        if((v = u_config_get_subkey_value(c, "max_count")) != NULL)
            so->max_count = atoi(v);

        if((v = u_config_get_subkey_value(c, "limit")) != NULL)
            so->mem_limit = atoi(v);
    }

    /* setup ppc parent <-> child channel */
    ppc = server_get_ppc(ctx->server);
    dbg_err_if(ppc == NULL);

    /* create an atom list to store in-memory session data */
    dbg_err_if(atoms_create(&so->atoms));

    /* register PPC commands callbacks */
    dbg_err_if(ppc_register(ppc, PPC_CMD_MSES_SAVE, session_cmd_save, so));
    dbg_err_if(ppc_register(ppc, PPC_CMD_MSES_GET, session_cmd_get, so));
    dbg_err_if(ppc_register(ppc, PPC_CMD_MSES_DELOLD, session_cmd_delold, so));
    dbg_err_if(ppc_register(ppc, PPC_CMD_MSES_REMOVE, session_cmd_remove, so));

    return 0;
err:
    return ~0;
}

