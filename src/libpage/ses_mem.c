#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
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
#include <u/libu.h>


typedef struct enc_ses_mem_s
{
    time_t mtime;               /* modification time    */
    char filename[PATH_MAX];    /* session filename     */
    size_t size;                /* data size            */
    char data[1];               /* data block           */
} enc_ses_mem_t;

static int session_calc_maxsize(var_t *v, size_t *psz)
{
    const char *value = NULL;

    dbg_err_if(v == NULL || var_get_name(v) == NULL || psz == NULL);

    if((value = var_get_value(v)) != NULL)
        *psz += 3 * strlen(value) + 1; /* worse case (i.e. longest string) */
    else
        *psz += strlen(var_get_name(v))+ 2;

    return 0;
err:
    return ~0;
}

static int so_atom_delete_oldest(session_opt_t *so)
{
    atom_t *atom, *oldest;
    size_t count, i;

    count = atoms_count(so->atoms);
    dbg_err_if(count == 0);

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
    size_t size, void* arg)
{
    atom_t *atom = NULL, *old = NULL;
    size_t new_size, old_found = 0;

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
            "session size is bigger the mem_limit, save aborted...");
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

    dbg_err_if(so == NULL);

    dbg("deleting oldest session...");
    dbg_err_if(so_atom_delete_oldest(so));

    return 0;
err:
    return ~0;
}

/* [parent] save a session */
static int session_cmd_save(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size, void *vso)
{
    session_opt_t *so = vso;
    enc_ses_mem_t *esm = (enc_ses_mem_t*)data;;

    u_unused_args(ppc, fd, cmd, size);

    dbg_err_if(vso == NULL || data == NULL);

    dbg_err_if(so_atom_add(so, esm->filename, esm->data, esm->size, 
        (void*)esm->mtime));

    return 0;
err:
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
        strncpy(esm->filename, filename, PATH_MAX);
        memcpy(esm->data, buf, size);

        /* send the command request */
        dbg_err_if(ppc_write(ppc, ctx->pipc, PPC_CMD_MSES_SAVE, 
            (char*)esm, esize) < 0);

        u_free(esm);

        /* add it to the local copy of atom list */
        dbg_err_if(so_atom_add(so, filename, buf, size, (void*)mtime));

    } else {
        /* parent context */
        dbg_err_if(so_atom_add(so, filename, buf, size, (void*)mtime));
    }

    return 0;
err:
    if(esm)
        u_free(esm);
    if(atom)
        atom_free(atom);
    return ~0;
}


static int session_mem_save(session_t *ss)
{
    io_t *io = NULL;
    size_t sz = 0;
    char *buf = NULL;

    /* delete previous data */
    //session_remove(ss);

    /* calc the maximum session data size (exact calc requires url encoding) */
    vars_foreach(ss->vars, session_calc_maxsize, &sz);

    /* alloc a block to save the session */
    buf = u_malloc(sz);
    dbg_err_if(buf == NULL);

    /* create a big-enough in-memory io object */
    dbg_err_if(io_mem_create(buf, sz, 0, &io));

    vars_foreach(ss->vars, session_prv_save_var, io);

    io_free(io);

    /* don't free buf that will be used by the embfs */
    dbg_err_if(session_mem_add(ss->so, ss->filename,  buf, sz, time(0)));

    u_free(buf);

    return 0;
err:
    if(buf)
        u_free(buf);
    if(io)
        io_free(io);
    return ~0;
}


static int session_mem_load(session_t *ss)
{
    atom_t *atom;
    io_t *io = NULL;

    /* find the file into the atom list */
    if(atoms_get(ss->so->atoms, ss->filename, &atom))
        return ~0; /* not found */

    /* copy stored mtime */
    ss->mtime = (time_t)atom->arg;

    /* build an io_t around it */
    dbg_err_if(io_mem_create(atom->data, atom->size, 0, &io));

    /* load data */
    dbg_err_if(session_prv_load(ss, io));

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int session_mem_remove(session_t *ss)
{
    ppc_t *ppc;

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

    ss = u_zalloc(sizeof(session_t));
    dbg_err_if(ss == NULL);

    ss->load = session_mem_load;
    ss->save = session_mem_save;
    ss->remove = session_mem_remove;
    ss->term = session_mem_term;
    ss->mtime = time(0);
    ss->so = so;

    dbg_err_if(session_prv_init(ss, rq, rs));

    dbg_err_if(u_path_snprintf(ss->filename, PATH_MAX, "%s.ss", ss->id));

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

    /* defaults */
    so->max_count = 0;  /* no limits */
    so->mem_limit = 0;  /* no limits */

    /* get configuration parameters */
    dbg_err_if(u_config_get_subkey(config, "memory", &c));

    if((v = u_config_get_subkey_value(c, "max_count")) != NULL)
        so->max_count = atoi(v);

    if((v = u_config_get_subkey_value(c, "mem_limit")) != NULL)
        so->mem_limit = atoi(v);

    /* setup ppc parent <-> child channel */
    ppc = server_get_ppc(ctx->server);
    dbg_err_if(ppc == NULL);

    /* create an atom list to store in-memory session data */
    dbg_err_if(atoms_create(&so->atoms));

    /* register session_cmd_saveto be called on 's' ppc command */
    dbg_err_if(ppc_register(ppc, PPC_CMD_MSES_SAVE, session_cmd_save, 
        (void*)so));

    /* register session_cmd_delold to be called on 'd' ppc command */
    dbg_err_if(ppc_register(ppc, PPC_CMD_MSES_DELOLD, session_cmd_delold, 
        (void*)so));

    /* register session_cmd_remove to be called on 'r' ppc command */
    dbg_err_if(ppc_register(ppc, PPC_CMD_MSES_REMOVE, session_cmd_remove, 
        (void*)so));

    return 0;
err:
    return ~0;
}

