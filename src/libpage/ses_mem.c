#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <klone/session.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/vars.h>
#include <klone/utils.h>
#include <klone/emb.h>
#include <klone/str.h>
#include <klone/debug.h>
#include <klone/ses_prv.h>

static int ses_count;
static size_t ses_size;

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

/* add an embedded file to the list of available resources */
static int session_mem_add(session_t *ss, char *buf, size_t sz)
{
    embfile_t *e = NULL;

    e = (embfile_t*)u_calloc(sizeof(embfile_t));
    dbg_err_if(e == NULL);

    e->res.type = ET_FILE;
    e->res.filename = u_strdup(ss->filename);
    e->size = e->file_size = sz;
    e->data = buf;
    e->comp = 0;
    e->mtime = time(0);

    dbg_err_if(emb_register((embres_t*)e));

    return 0;
err:
    if(e)
        u_free(e);
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
    dbg_err_if(session_mem_add(ss, buf, sz));

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
    embfile_t *e;
    io_t *io = NULL;

    /* find the file into the embfs */
    dbg_err_if(emb_lookup(ss->filename, (embres_t**)&e));

    /* copy stored mtime */
    ss->mtime = e->mtime;

    /* build an io_t around it */
    dbg_err_if(io_mem_create(e->data, e->size, 0, &io));

    /* load data */
    dbg_err_if(session_prv_load(ss, io));

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int session_mem_term(session_t *ss)
{
    /* nothing to do */
    U_UNUSED_ARG(ss);
    return 0;
}

static int session_mem_remove(session_t *ss)
{
    embfile_t *e = NULL;

    /* find the file bound to this session */
    dbg_err_if(emb_lookup(ss->filename, (embres_t**)&e));

    /* detach from embfs */
    dbg_err_if(emb_unregister((embres_t*)e));

    /* free embfile buffers */
    u_free(e->res.filename);
    u_free(e->data);

    u_free(e);

    return 0;
err:
    if(e)
    {
        if(e->data)
            u_free(e->data);
        u_free(e);
    }
    return ~0;
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
