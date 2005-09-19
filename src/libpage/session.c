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
#include <klone/str.h>
#include <klone/debug.h>
#include <klone/ses_prv.h>
#include "conf.h"

enum { DEFAULT_SESSION_EXPIRATION = 60*20 }; /* 20 minutes */
static const char SID_NAME[] = "klone_sid";

/** 
 *  \ingroup Chttp
 *  \{
 */

/**
 *          \defgroup session HTTP session handling
 *          \{
 *              \par
 */

int session_load(session_t *ss)
{
    dbg_return_if(ss->load == NULL, ~0);

    return ss->load(ss);
}

int session_save(session_t *ss)
{
    dbg_return_if(ss->save == NULL, ~0);

    if(vars_count(ss->vars) == 0)
        return 0; /* nothing to save */

    return ss->save(ss);
}

int session_remove(session_t *ss)
{
    dbg_return_if(ss->remove == NULL, ~0);

    ss->removed = 1;

    return ss->remove(ss);
}

int session_set_id(session_t *ss, const char *id)
{
    char buf[256];
    time_t now;

    if(id == NULL)
    {   /* gen a new one */
        time(&now);

        dbg_err_if(u_snprintf(buf, 255, "%u%d%d", now, getpid(), rand()));

        dbg_err_if(u_md5(buf, strlen(buf), ss->id));
    } else {
        dbg_err_if(u_snprintf(ss->id, MD5_DIGEST_BUFSZ, "%s", id));
        ss->id[MD5_DIGEST_LEN] = 0;
    }

    return 0;
err:
    return ~0;
}

int session_prv_init(session_t *ss, request_t *rq, response_t *rs)
{
    enum { DEFAULT_EXPIRE_TIME = 60*20 }; /* 20 minutes */
    const char *sid;

    dbg_err_if(vars_create(&ss->vars));

    ss->rq = rq;
    ss->rs = rs;

    if((sid = request_get_cookie(rq, SID_NAME)) == NULL)
    {
        dbg_err_if(session_set_id(ss, NULL));
        dbg_err_if(response_set_cookie(rs, SID_NAME, ss->id, 0, NULL, NULL, 0));
        dbg("sid: %s", ss->id);
    } else
        dbg_err_if(session_set_id(ss, sid));

    return 0;
err:
    return ~0;
}

int session_prv_load(session_t *ss, io_t *io)
{
    string_t *line = NULL;

    dbg_err_if(string_create(NULL, 0, &line));

    while(u_getline(io, line) == 0)
        if(string_len(line))
            dbg_err_if(vars_add_urlvar(ss->vars, string_c(line)));

    string_free(line);

    return 0;
err:
    if(line)
        string_free(line);
    return ~0;
}


/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param ss  parameter \a ss description
 *
 * \return
 *  - \c 0  always
 */
int session_free(session_t *ss)
{
    if(!ss->removed)
        dbg_if(session_save(ss));

    /* driver cleanup */
    dbg_if(ss->term(ss));

    if(ss->vars)
        vars_free(ss->vars);

    u_free(ss);

    return 0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param ss  parameter \a ss description
 *  
 * \return
 *  - the variables' list of the given \a ss
 */
vars_t *session_get_vars(session_t *ss)
{
    return ss->vars;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param ss    parameter \a ss description
 * \param name  parameter \a name description
 * 
 * \return
 *  - the variable value corresponding to the given \a name
 */
const char *session_get(session_t *ss, const char *name)
{
    var_t *v;

    v = vars_get(ss->vars, name);
    return v ? var_get_value(v): NULL;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param ss     parameter \a ss description
 * \param name   parameter \a name description
 * \param value  parameter \a value description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int session_set(session_t *ss, const char *name, const char* value)
{
    var_t *v = NULL;

    if((v = vars_get(ss->vars, name)) == NULL)
    {
        /* add a new session variable */
        dbg_err_if(var_create(name, value, &v));

        dbg_err_if(vars_add(ss->vars, v));
    } else {
        /* update an existing var */
        dbg_ifb(var_set_value(v, value))
            return ~0;
    }

    return 0;
err:
    if(v)
        var_free(v);
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param ss  parameter \a path description
 *  
 * \return
 *  - the number of seconds ...
 *  - \c -1 on error
 */
int session_age(session_t *ss)
{
    time_t now;
    struct stat st;

    now = time(0);

    /* ss->mtime must has been set into session_X_create funcs */
    return (int)(now - ss->mtime); /* in seconds */
err:
    return -1;
}

/** 
 * \brief   Remove all session variables
 *  
 * Detailed function descrtiption.
 *
 * \param ss  parameter \a ss description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int session_clean(session_t *ss)
{
    var_t *v = NULL;

    while((v = vars_getn(ss->vars, 0)) != NULL)
    {
        dbg_err_if(vars_del(ss->vars, v));
        var_free(v);
    }

    return 0;
err:
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param ss    parameter \a ss description
 * \param name  parameter \a name description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int session_del(session_t *ss, const char *name)
{
    var_t *v = NULL;

    dbg_err_if((v = vars_get(ss->vars, name)) == NULL);

    dbg_err_if(vars_del(ss->vars, v));

    var_free(v);

    return 0;
err:
    return ~0;
}

int session_prv_save_var(var_t *v, io_t *out)
{
    char *vbuf = NULL, *nbuf = NULL;
    const char *value = NULL, *name = NULL;
    size_t vsz, nsz;

    dbg_err_if(v == NULL || var_get_name(v) == NULL || out == NULL);

    name = var_get_name(v);

    if((value = var_get_value(v)) != NULL)
    {
        vsz = 3 * strlen(value) + 1; 
        nsz = 3 * strlen(name) + 1; 

        /* alloc buffers to store encoded version of name and value */
        vbuf = u_malloc(vsz);
        dbg_err_if(vbuf == NULL);

        nbuf = u_malloc(nsz);
        dbg_err_if(nbuf == NULL);

        /* encode name & value */
        dbg_err_if(u_urlncpy(vbuf, value, vsz, URLCPY_ENCODE));
        dbg_err_if(u_urlncpy(nbuf, name, nsz, URLCPY_ENCODE));

        io_printf(out, "%s=%s\n", nbuf, vbuf);

        u_free(nbuf);
        u_free(vbuf);
    } else
        io_printf(out, "%s=\n", var_get_name(v));

    return 0;
err:
    if(nbuf)
        u_free(nbuf);
    if(vbuf)
        u_free(vbuf);
    return ~0;
}

int session_create(config_t *config, request_t *rq, response_t *rs, 
    session_t **pss)
{
    const char *session_driver = NULL;
    config_t *c = NULL;
    session_t *ss = NULL;
    int max_age;

    dbg_err_if(config == NULL || rq == NULL || rs == NULL || pss == NULL);

    session_driver = config_get_subkey_value(config, "driver");
    if(session_driver && strcasecmp(session_driver, "memory") == 0)
    {   /* store sessions in memory */
        dbg_err_if(session_mem_create(config, rq, rs, &ss));
    } else if(session_driver && strcasecmp(session_driver, "file") == 0) {
        /* store sessions in the file system */
        dbg_err_if(session_file_create(config, rq, rs, &ss));
    #ifdef HAVE_LIBSSL
    } else if(session_driver && strcasecmp(session_driver, "client") == 0) {
        /* client-side sessions */
        dbg_err_if(session_client_create(config, rq, rs, &ss));
    #endif
    } else
        warn_err("config error: bad or missing session driver");

    /* may fail if session does not exist */
    session_load(ss);

    /* check if the session has expired */
    max_age = DEFAULT_SESSION_EXPIRATION;
    if(!config_get_subkey(config, "max_age", &c))
        max_age = MAX(atoi(config_get_value(c))*60, 60); /* min value: 1 min */

    if(session_age(ss) > max_age)
        session_clean(ss); /* remove all session variables */

    *pss = ss;

    return 0;
err:
    if(ss)
        session_free(ss);
    return ~0;
}


/**
 *          \}
 */

/**
 *  \}
 */
