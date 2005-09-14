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

/*
    cookies:
        kl1_mtime = time_t
        kl1_sesN = data cookies (b64(encrypted session data))
        kl1_mac  = HMAC hash (kl1_ses0..kl1_sesN + kl1_mtime)

    FIXME HMAC deve essere della roba criptata o di quella in chiaro?

    struct session_client_s
    {
        time_t mtime;
        char data[]; // encrypted + base64
        HMAC mac;        // base64
    };
 */

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

static int session_client_save(session_t *ss)
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

    /* TODO [compress |] encrypt | b64 buf and store it into cookies */
    /* TODO store mtime in a cookies */
    /* TODO calc MAC hash of the data buf + mtime and store it in a cookie */

    u_free(buf);

    return 0;
err:
    if(buf)
        u_free(buf);
    if(io)
        io_free(io);
    return ~0;
}

static int session_client_load(session_t *ss)
{
    io_t *io = NULL;
    char *data = NULL;
    size_t size;

    /* TODO decrypt cookies */
    /* TODO verify MAC hash */
    /* TODO gen an io_t from decrypted (and verified) data and load from it */


    /* copy stored mtime */
    // ss->mtime = e->mtime;

    /* build an io_t around it */
    dbg_err_if(io_mem_create(data, size, 0, &io));

    /* load data */
    dbg_err_if(session_prv_load(ss, io));

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int session_client_term(session_t *ss)
{
    /* nothing to do */
    return 0;
}

static int session_client_remove(session_t *ss)
{
    /* TODO remove session related cookies */

    return 0;
err:
    return ~0;
}

int session_client_create(config_t *config, request_t *rq, response_t *rs, 
        session_t **pss)
{
    session_t *ss = NULL;

    ss = u_calloc(sizeof(session_t));
    dbg_err_if(ss == NULL);

    ss->load = session_client_load;
    ss->save = session_client_save;
    ss->remove = session_client_remove;
    ss->term = session_client_term;
    ss->mtime = time(0);

    dbg_err_if(session_prv_init(ss, rq, rs));

    dbg_err_if(u_path_snprintf(ss->filename, PATH_MAX, "%s.ss", ss->id));

    *pss = ss;

    return 0;
err:
    if(ss)
        session_free(ss);
    return ~0;
}
