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
#include <klone/str.h>
#include <klone/atom.h>
#include <klone/debug.h>
#include <klone/ses_prv.h>
#include <klone/ppc.h>


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

/* add an atom to the list of global atoms */
static int session_mem_add(atoms_t *as, const char *filename, char *buf, 
    size_t size, time_t mtime)
{
    atom_t *atom = NULL;
    enc_ses_mem_t *esm = NULL;
    ppc_t *ppc;
    size_t esize;

    // FIXME run checks for maxsize and/or maxcount

    if(ctx->pipc)
    {   /* children context */
        ppc = server_get_ppc(ctx->server);
        dbg_err_if(ppc == NULL);

        esize = size + sizeof(enc_ses_mem_t);
        esm = (enc_ses_mem_t*)u_malloc(esize);
        dbg_err_if(esm == NULL);

        esm->mtime = time(0);
        esm->size = size;
        strncpy(esm->filename, filename, PATH_MAX);
        memcpy(esm->data, buf, size);

        dbg_err_if(ppc_write(ppc, ctx->pipc, 's', esm, esize) < 0);

        u_free(esm);
    } else {
        /* parent context */

        /* create a new atom */
        dbg_err_if(atom_create(filename, buf, size, mtime, &atom));

        /* add it to the list */
        dbg_err_if(atoms_add(as, atom));
    }

    return 0;
err:
    if(esm)
        u_free(esm);
    if(atom)
        atom_free(atom);
    return ~0;
}

static int session_cmd_save_mem(ppc_t *ppc, unsigned char cmd, char *data, 
    size_t size, void *vso)
{
    session_opt_t *so = (session_opt_t*)vso;
    enc_ses_mem_t *esm = (enc_ses_mem_t*)data;

    dbg(__FUNCTION__);

    dbg_err_if(vso == NULL || data == NULL);

    so = (session_opt_t*)vso;
    esm = (enc_ses_mem_t*)data;

    dbg_err_if(session_mem_add(so->atoms, esm->filename, esm->data, esm->size, 
        esm->mtime));

    return 0;
err:
    return ~0;
}


static int session_mem_save(session_t *ss)
{
    io_t *io = NULL;
    size_t sz = 0;
    char *buf = NULL;

    /* delete previous data */
    session_remove(ss);

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
    dbg_err_if(session_mem_add(ss->so->atoms, ss->filename,  buf, sz, time(0)));

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
    dbg_err_if(atoms_get(ss->so->atoms, ss->filename, &atom));

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
    atom_t *atom;

    /* find the atom bound to this session */
    dbg_err_if(atoms_get(ss->so->atoms, ss->filename, &atom));

    /* remove it from the list */
    dbg_err_if(atoms_remove(ss->so->atoms, atom));

    atom_free(atom);

    return 0;
err:
    return ~0;
}

static int session_mem_term(session_t *ss)
{
    /* nothing to do */
    U_UNUSED_ARG(ss);
    return 0;
}

int session_mem_create(session_opt_t *so, request_t *rq, response_t *rs, 
        session_t **pss)
{
    session_t *ss = NULL;

    ss = u_calloc(sizeof(session_t));
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
int session_mem_module_init(config_t *config, session_opt_t *so)
{
    ppc_t *ppc;

    dbg(__FUNCTION__);

    ppc = server_get_ppc(ctx->server);
    dbg_err_if(ppc == NULL);

    /* attach an atom list to so the store in-memory session data */
    dbg_err_if(atoms_create(&so->atoms));

    dbg_err_if(ppc_register(ppc, 's', session_cmd_save_mem, (void*)so));

    /* session_mem_save() will call server_rpc_send(server, 's', data, size) */

    return 0;
err:
    return ~0;
}

