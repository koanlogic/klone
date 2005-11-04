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
#include <klone/ses_prv.h>
#include <u/libu.h>
#include "conf.h"

#ifdef HAVE_LIBOPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <klone/ccipher.h>
#endif

enum { DEFAULT_SESSION_EXPIRATION = 60*20 }; /* 20 minutes */
static const char SID_NAME[] = "klone_sid";

typedef struct save_cb_params_s
{
    session_t *ss;
    io_t *io;
} save_cb_params_t;

int session_module_term(session_opt_t *so)
{
    if(so)
        u_free(so);

    return 0;
}

int session_module_init(u_config_t *config, session_opt_t **pso)
{
    session_opt_t *so = NULL;
    u_config_t *c;
    const char *v;
    int max_age;

    so = u_zalloc(sizeof(session_opt_t));
    dbg_err_if(so == NULL);

    /* defaults values */
    so->type = SESSION_TYPE_FILE;
    so->max_age = DEFAULT_SESSION_EXPIRATION;

    if(u_config_get_subkey(config, "session", &c))
    {
        /* no 'session' subsection, defaults will be used */
        *pso = so;
        return 0; 
    }

    /* set session type */
    if((v = u_config_get_subkey_value(c, "type")) != NULL)
    {
        if(!strcasecmp(v, "memory")) {
            so->type = SESSION_TYPE_MEMORY;
        } else if(!strcasecmp(v, "file")) {
            so->type = SESSION_TYPE_FILE;
        #ifdef HAVE_LIBOPENSSL
        } else if(!strcasecmp(v, "client")) {
            so->type = SESSION_TYPE_CLIENT;
        #endif
        } else
            warn_err("config error: bad session type");
    }

    /* set max_age */
    if((v = u_config_get_subkey_value(c, "max_age")) != NULL)
        max_age = MAX(atoi(v) * 60, 60); /* min value: 1 min */

    /* per-type configuration init */
    if(so->type == SESSION_TYPE_MEMORY)
        dbg_err_if(session_mem_module_init(c, so));
    else if(so->type == SESSION_TYPE_FILE)
        dbg_err_if(session_file_module_init(c, so));
    #ifdef HAVE_LIBOPENSSL
    else if(so->type == SESSION_TYPE_CLIENT)
        dbg_err_if(session_client_module_init(c, so));
    #endif

    *pso = so;

    return 0;
err:
    if(so)
        u_free(so);
    return ~0;
}


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
    } else
        dbg_err_if(session_set_id(ss, sid));

    return 0;
err:
    return ~0;
}

int session_prv_load(session_t *ss, io_t *io)
{
    u_string_t *line = NULL;
    var_t *v = NULL;
    unsigned char key[CODEC_CIPHER_KEY_SIZE];
    size_t ksz;

    dbg_err_if(u_string_create(NULL, 0, &line));

    while(u_getline(io, line) == 0)
    {
        if(u_string_len(line))
        {
            dbg_err_if(vars_add_urlvar(ss->vars, u_string_c(line), &v));

            #ifdef HAVE_LIBOPENSSL
            if(!strcmp(var_get_name(v), "KLONE_CIPHER_KEY"))
            {
                /* decrypt key and save it to key */
                memset(key, 0, sizeof(key));
                dbg_ifb(u_cipher_decrypt(EVP_aes_256_cbc(), ss->so->session_key,
                    NULL, key, &ksz, var_get_value(v), var_get_value_size(v)))
                {
                    v = vars_get(ss->vars, "KLONE_CIPHER_KEY");
                    vars_del(ss->vars, v);
                } else {
                    /* save it to the var list */
                    dbg_err_if(var_set_bin_value(v, key, ksz));
                }
            }
            #endif
        }
    }

    u_string_free(line);

    return 0;
err:
    if(line)
        u_string_free(line);
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

    now = time(0);

    /* ss->mtime must has been set into session_X_create funcs */
    return (int)(now - ss->mtime); /* in seconds */
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

int session_prv_save_to_io(session_t *ss, io_t *out)
{
    save_cb_params_t prm; 

    /* pass io and session poiters to the callback function */
    prm.io = out;
    prm.ss = ss;

    vars_foreach(ss->vars, session_prv_save_var, (void*)&prm);

    return 0;
err:
    return ~0;
}

/* save a var_t (text or binary) to the session io_t */
int session_prv_save_var(var_t *v, void *vp)
{
    enum { NAMESZ = 256, VALSZ = 4096 };
    char sname[NAMESZ], svalue[VALSZ];
    char *uname = sname, *uvalue = svalue;
    save_cb_params_t *pprm = (save_cb_params_t*)vp;
    /* encrypted key buffer */
    unsigned char ekey[CODEC_CIPHER_KEY_SIZE + CODEC_CIPHER_BLOCK_SIZE + 1]; 
    size_t eksz, nsz, vsz;
    int rc = ~0;

    /* buffers must be at least three times the src data to URL-encode  */
    nsz = 1 + 3 * strlen(var_get_name(v));  /* name buffer size  */
    #ifndef HAVE_LIBOPENSSL
    vsz = 1 + 3 * var_get_value_size(v);    /* value buffer size */
    #else
    vsz = 1 + 3 * (var_get_value_size(v) + CODEC_CIPHER_BLOCK_SIZE);
    #endif

    /* if the buffer on the stack is too small alloc a bigger one */
    if(NAMESZ <= nsz)
        dbg_err_if((uname = u_zalloc(nsz)) == NULL);

    /* url encode name */
    dbg_err_if(u_urlncpy(uname, var_get_name(v), strlen(var_get_name(v)), 
        URLCPY_ENCODE) <= 0);

    if(var_get_value(v))
    {
        /* if the buffer on the stack is too small alloc a bigger one */
        if(VALSZ <= vsz)
            dbg_err_if((uvalue = u_zalloc(vsz)) == NULL);

        #ifdef HAVE_LIBOPENSSL
        if(!strcmp(var_get_name(v), "KLONE_CIPHER_KEY"))
        {
            /* encrypt key and save it to ekey */
            dbg_err_if(u_cipher_encrypt(EVP_aes_256_cbc(), 
                pprm->ss->so->session_key, NULL, ekey, &eksz, 
                var_get_value(v), var_get_value_size(v)));

            /* save it to the var list */
            dbg_err_if(var_set_bin_value(v, ekey, eksz));
        }
        #endif

        dbg_err_if(u_urlncpy(uvalue, var_get_value(v), var_get_value_size(v), 
            URLCPY_ENCODE) <= 0);

        dbg_err_if(io_printf(pprm->io, "%s=%s\n", uname, uvalue) < 0);
    } else 
        dbg_err_if(io_printf(pprm->io, "%s=\n", uname) < 0);

    rc = 0; /* success */
err:
    /* free heap buffers */
    if(uname && uname != sname)
        u_free(uname);

    if(uvalue && uvalue != svalue)
        u_free(uvalue);

    return rc;
}

int session_create(session_opt_t *so, request_t *rq, response_t *rs, 
    session_t **pss)
{
    session_t *ss = NULL;

    dbg_err_if(so == NULL || rq == NULL || rs == NULL || pss == NULL);

    switch(so->type)
    {
    case SESSION_TYPE_FILE:
        dbg_err_if(session_file_create(so, rq, rs, &ss));
        break;
    case SESSION_TYPE_MEMORY:
        dbg_err_if(session_mem_create(so, rq, rs, &ss));
        break;
    #ifdef HAVE_LIBOPENSSL
    case SESSION_TYPE_CLIENT:
        dbg_err_if(session_client_create(so, rq, rs, &ss));
        break;
    #endif
    default:
        warn_err("bad session type");
    }

    /* may fail if the session does not exist */
    session_load(ss);

    dbg_ifb(session_age(ss) > so->max_age)
    {
        session_clean(ss);  /* remove all session variables */
        session_remove(ss); /* remove the session itself    */
    }

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
