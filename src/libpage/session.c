/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef SSL_ON
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <klone/ccipher.h>
#endif 
#include <u/libu.h>
#include <klone/session.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/vars.h>
#include <klone/utils.h>
#include <klone/ses_prv.h>
#include <klone/codecs.h>

enum { DEFAULT_SESSION_EXPIRATION = 60*20 }; /* 20 minutes */
static const char SID_NAME[] = "klone_sid";


struct save_cb_params_s
{
    session_t *ss;
    io_t *io;
};

typedef struct save_cb_params_s save_cb_params_t;

int session_module_term(session_opt_t *so)
{
    U_FREE(so);

    return 0;
}

int session_module_init(u_config_t *config, session_opt_t **pso)
{
    int iv;
    session_opt_t *so = NULL;
    u_config_t *c = NULL;
    const char *v;

    dbg_err_if (config == NULL);
    dbg_err_if (pso == NULL);

    dbg_err_sif ((so = u_zalloc(sizeof(session_opt_t))) == NULL);

    /* defaults values */
    so->type = SESSION_TYPE_FILE;
    so->max_age = DEFAULT_SESSION_EXPIRATION;
    so->compress = 0;
    so->encrypt = 0;
    (void) u_strlcpy(so->name, SID_NAME, sizeof so->name);

    /* In case no 'session' subsection is found, defaults will be used. */

    if(!u_config_get_subkey(config, "session", &c))
    {
        /* set session type */
        if((v = u_config_get_subkey_value(c, "type")) != NULL)
        {
            if(!strcasecmp(v, "memory"))
                so->type = SESSION_TYPE_MEMORY;
            else if(!strcasecmp(v, "file"))
                so->type = SESSION_TYPE_FILE;
#ifdef SSL_ON
            else if(!strcasecmp(v, "client"))
                so->type = SESSION_TYPE_CLIENT;
#endif
            else
               warn_err("config error: bad session type \'%s\'", v);
        }

        /* set max_age */
        if((v = u_config_get_subkey_value(c, "max_age")) != NULL)
        {
            dbg_err_ifm (u_atoi(v, &iv), "config error: bad max_age \'%s\'", v);
            so->max_age = U_MAX(iv * 60, 60); /* silently uplift to 1 min */
        }

        /* set compression flag */
        dbg_err_if(u_config_get_subkey_value_b(c, "compress", 0,
            &so->compress));

        /* set encryption flag */
        dbg_err_if(u_config_get_subkey_value_b(c, "encrypt", 0, &so->encrypt));

        /* set cookie name */
        if ((v = u_config_get_subkey_value(c, "sid_name")) != NULL)
            dbg_err_if (u_strlcpy(so->name, v, sizeof so->name));

#ifndef HAVE_LIBZ
        if(so->compress)
            warn_err("config error: compression is enabled but libz is not "
                     "linked");
#endif

#ifndef SSL_ON
        if(so->encrypt)
            warn_err("config error: encryption is enabled but no SSL "
                     "lib is linked");
#else

#ifdef SSL_OPENSSL
        /* init cipher EVP algo, the random key and IV */
        so->cipher = EVP_aes_256_cbc(); /* use AES-256 in CBC mode */

        EVP_add_cipher(so->cipher);

        /* key and iv for client-side session */
        dbg_err_if(!RAND_bytes(so->cipher_key, CIPHER_KEY_LEN));
        dbg_err_if(!RAND_pseudo_bytes(so->cipher_iv, CIPHER_IV_LEN));

        /* create a random key and iv to crypt the SESSION_KEY_VAR variable */
        dbg_err_if(!RAND_bytes(so->session_key, CIPHER_KEY_LEN));
        dbg_err_if(!RAND_pseudo_bytes(so->session_iv, CIPHER_IV_LEN));
#endif

#ifdef SSL_CYASSL
        /* init cipher EVP algo, the random key and IV */
        so->cipher = EVP_aes_256_cbc(); /* use AES-256 in CBC mode */

        /* key and iv for client-side session */
        dbg_err_if(!RAND_bytes(so->cipher_key, CIPHER_KEY_LEN));
        dbg_err_if(!RAND_bytes(so->cipher_iv, CIPHER_IV_LEN));

        /* create a random key and iv to crypt the SESSION_KEY_VAR variable */
        dbg_err_if(!RAND_bytes(so->session_key, CIPHER_KEY_LEN));
        dbg_err_if(!RAND_bytes(so->session_iv, CIPHER_IV_LEN));
#endif

#endif
    } /* if "session" exists */

    /* per-type configuration init */
    if(so->type == SESSION_TYPE_MEMORY)
        warn_err_ifm(session_mem_module_init(c, so), 
            "in-memory session engine init error");
    else if(so->type == SESSION_TYPE_FILE)
        warn_err_ifm(session_file_module_init(c, so), 
            "file session engine init error");
#ifdef SSL_ON
    else if(so->type == SESSION_TYPE_CLIENT)
        warn_err_ifm(session_client_module_init(c, so),
            "client-side session engine init error");
#endif

    *pso = so;

    return 0;
err:
    u_free(so);
    return ~0;
}

int session_prv_calc_maxsize(var_t *v, void *p)
{
    const char *value = NULL;
    size_t *psz = (size_t*)p;

    dbg_err_if (v == NULL);
    dbg_err_if (var_get_name(v) == NULL);
    dbg_err_if (psz == NULL);

#ifdef SSL_ON
    if(*psz == 0)
    {   /* a block plus the padding block */
        *psz = CODEC_CIPHER_BLOCK_LEN * 2;
    }
#endif

    /* name= */
    *psz += 3 * strlen(var_get_name(v)) + 3;

    /* value */
    if((value = var_get_value(v)) != NULL)
        *psz += 3 * strlen(value) + 1; /* worse case (i.e. longest string) */

    return 0;
err:
    return ~0;
}

int session_prv_load_from_buf(session_t *ss, char *buf, size_t size)
{
    io_t *io = NULL;

    dbg_err_if (ss == NULL);
    dbg_err_if (buf == NULL);

    /* build an io_t around the buffer */
    dbg_err_if(io_mem_create(buf, size, 0, &io));

    /* load data */
    dbg_err_if(session_prv_load_from_io(ss, io));

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

int session_prv_save_to_buf(session_t *ss, char **pbuf, size_t *psz)
{
    io_t *io = NULL;
    char *buf = NULL;
    size_t sz = 0;

    dbg_err_if (ss == NULL);
    dbg_err_if (pbuf == NULL);
    dbg_err_if (psz == NULL);
 
    /* calc the maximum session data size (exact calc requires url encoding and
       codec transformation knowledge) */
    vars_foreach(ss->vars, session_prv_calc_maxsize, (void*)&sz);

    /* alloc a big-enough block to save the session data */
    buf = u_malloc(sz);
    dbg_err_if(buf == NULL);

    /* create a big-enough in-memory io object */
    dbg_err_if(io_mem_create(buf, sz, 0, &io));

    /* save the session to the in-memory io */
    dbg_err_if(session_prv_save_to_io(ss, io));

    /* remove all codecs to get the right size of 'buf'. we need to remove 
       the codecs because some of them buffer data until last codec->flush() 
       is called (and it's not possible to flush codecs without removing them */
    dbg_err_if(io_codecs_remove(io));

    /* get the number of bytes written to the io (so to 'buf') */
    sz = io_tell(io);

    io_free(io);
    io = NULL;

    *pbuf = buf;
    *psz = sz;

    return 0;
err:
    if(io)
        io_free(io);
    U_FREE(buf);
    return ~0;
}

/* Supplied 'id' must be SESSION_ID_LENGTH long and made of hex digits only. */
static int session_is_good_id(const char *id)
{
    const char *p;

    dbg_return_if (id == NULL, 0);
    dbg_return_if (strlen(id) != SESSION_ID_LENGTH, 0);

    for (p = id; *p != '\0'; ++p)
    {
        if (!isxdigit(*p))
            return 0;
    }

    return 1; /* good */
}

static int session_set_filename(session_t *ss)
{
    const char *a = NULL;

    dbg_return_if (ss->id[0] == '\0', ~0);

    dbg_err_if((a = request_get_addr(ss->rq)) == NULL);

    dbg_err_if(u_path_snprintf(ss->filename, U_FILENAME_MAX, 
            U_PATH_SEPARATOR, "%s/klone_sess_%s_%s", ss->so->path, ss->id, a));

    return 0;
err:
    return ~0;
}

static int session_gen_id(session_t *ss)
{
    char buf[256 + 1];  /* handle worst case, i.e. (64bit * 4) + '\0' */
    struct timeval tv;

    dbg_err_if (ss == NULL);

    /* Initialize sid to empty string. */
    ss->id[0] = '\0';

    /* SID is MD5(mix(now, process_pid, random)) */
    dbg_err_sif (gettimeofday(&tv, NULL) == -1);

    dbg_err_if (u_snprintf(buf, sizeof buf, "%lu%u%lu%d", 
                (unsigned long) tv.tv_sec, (unsigned int) getpid(), 
                (unsigned long) tv.tv_usec, rand()));

    dbg_err_if (u_md5(buf, strlen(buf), ss->id));

    /* Remove previous SID, if any. */ 
    dbg_err_if (response_set_cookie(ss->rs, ss->so->name, NULL, 
                0, NULL, NULL, 0));

    /* Set the cookie ID. */
    dbg_err_if (response_set_cookie(ss->rs, ss->so->name, ss->id, 
                0, NULL, NULL, 0));

    return 0;
err:
    return ~0;
}

int session_prv_set_id(session_t *ss, const char *sid)
{
    dbg_return_if (ss == NULL, ~0);

    /* set or generate a session id */
    if (sid && session_is_good_id(sid))
        dbg_err_if (u_snprintf(ss->id, sizeof ss->id, "%s", sid));
    else
        dbg_err_if (session_gen_id(ss));

    /* set the filename accordingly */
    dbg_err_if (session_set_filename(ss));

    return 0;
err:
    return ~0;
}

int session_priv_set_id(session_t *ss, const char *sid)
{
    return session_prv_set_id(ss, sid); /* backward compatibility */
}

int session_load(session_t *ss)
{
    dbg_return_if (ss == NULL, ~0);
    dbg_return_if (ss->load == NULL, ~0);

    return ss->load(ss);
}

int session_save(session_t *ss)
{
    dbg_return_if (ss == NULL, ~0);
    dbg_return_if (ss->save == NULL, ~0);

    /* No vars set: if cookie empty it's a no-op, otherwise remove stored
     * cookies. */
    if (vars_count(ss->vars) == 0)
        return (ss->id[0] == '\0') ? 0 : session_remove(ss);

    /* new user, need to save some vars */
    if (ss->id[0] == '\0')
    {
        /* generate a new SID and set session filename accordingly */
        dbg_err_if (session_prv_set_id(ss, NULL)); 
    }

    return ss->save(ss);
err:
    return ~0;
}

int session_remove(session_t *ss)
{
    dbg_return_if (ss == NULL, ~0);
    dbg_return_if (ss->remove == NULL, ~0);

    /* remove the cookie */
    response_set_cookie(ss->rs, ss->so->name, NULL, 0, NULL, NULL, 0);

    ss->removed = 1;

    return ss->remove(ss);
}

int session_prv_init(session_t *ss, request_t *rq, response_t *rs)
{
    const char *sid;

    dbg_err_if (ss == NULL);
    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    
    dbg_err_if(vars_create(&ss->vars));

    ss->rq = rq;
    ss->rs = rs;

    /* if the client has a SID set and it's a good one then use it */
    sid = request_get_cookie(ss->rq, ss->so->name);
    if(sid)
        dbg_err_if(session_prv_set_id(ss, sid));

    return 0;
err:
    return ~0;
}

int session_prv_load_from_io(session_t *ss, io_t *io)
{
    u_string_t *line = NULL;
    var_t *v = NULL;
    codec_t *unzip = NULL, *decrypt = NULL;
    unsigned char key[CODEC_CIPHER_KEY_BUFSZ];
    size_t ksz;

    dbg_return_if (ss == NULL, ~0);
    dbg_return_if (io == NULL, ~0);

#ifdef SSL_ON
    if(ss->so->encrypt)
    {
        dbg_err_if(codec_cipher_create(CIPHER_DECRYPT, ss->so->cipher, 
            ss->so->cipher_key, ss->so->cipher_iv, &decrypt)); 
        dbg_err_if(io_codec_add_tail(io, decrypt));
        decrypt = NULL; /* io_t owns it after io_codec_add_tail */
    }
#else
    u_unused_args(key, ksz);
#endif

#ifdef HAVE_LIBZ
    if(ss->so->compress)
    {
        dbg_err_if(codec_gzip_create(GZIP_UNCOMPRESS, &unzip));
        dbg_err_if(io_codec_add_tail(io, unzip));
        unzip = NULL; /* io_t owns it after io_codec_add_tail */
    }
#endif

    dbg_err_if(u_string_create(NULL, 0, &line));

    while(u_getline(io, line) == 0)
    {
        if(u_string_len(line))
        {
            dbg_err_if(vars_add_urlvar(ss->vars, u_string_c(line), &v));

#ifdef SSL_ON
            if(!strcmp(var_get_name(v), SESSION_KEY_VAR))
            {
                /* decrypt key and save it to key */
                memset(key, 0, sizeof(key));
				ksz = sizeof(key);
                dbg_ifb(u_cipher_decrypt(EVP_aes_256_cbc(), ss->so->session_key,
                    ss->so->session_iv, key, &ksz, 
                    var_get_value(v), var_get_value_size(v)))
                {
                    v = vars_get(ss->vars, SESSION_KEY_VAR);
                    vars_del(ss->vars, v);
                } else {
                    /* save it to the var list */
                    dbg_err_if(var_set_bin_value(v, key, ksz));
                }

            }
#endif
        }
    }

    /* remove set codecs and flush */
    io_codecs_remove(io);

    u_string_free(line);

    return 0;
err:
    if(io)
        io_codecs_remove(io);
    if(decrypt)
        codec_free(decrypt);
    if(unzip)
        codec_free(unzip);
    if(line)
        u_string_free(line);
    return ~0;
}

int session_free(session_t *ss)
{
    if (ss)
    { 
        if(!ss->removed)
            dbg_if(session_save(ss));

        /* driver cleanup */
        dbg_if(ss->term(ss));

        if(ss->vars)
            vars_free(ss->vars);

        U_FREE(ss);
    }

    return 0;
}

/** 
 * \ingroup session
 * \brief   Get session variables
 *  
 * Return a vars_t containing the session variables.
 *
 * \param ss  session object
 *  
 * \return the variables' list of the given \p ss (may be \c NULL)
 */
vars_t *session_get_vars(session_t *ss)
{
    dbg_return_if (ss == NULL, NULL);

    return ss->vars;
}

/**
 * \ingroup session
 * \brief   Get session variable with given name
 *
 * Return a string representation of variable in \p ss with given \p name.
 *
 * \param ss    session object
 * \param name  session variable name
 * 
 * \return the variable value corresponding to the given \p name (may be 
 *         \c NULL)
 */
const char *session_get(session_t *ss, const char *name)
{
    var_t *v;

    dbg_return_if (ss == NULL, NULL);
    dbg_return_if (name == NULL, NULL);
    
    v = vars_get(ss->vars, name);
    return v ? var_get_value(v): NULL;
}

/** 
 * \ingroup session
 * \brief   Get session id string
 *  
 * Return a string carrying the session id.
 *
 * \param ss  session object
 *  
 * \return a string carrying the session id of the given \p ss (may be \c NULL)
 */
const char *session_get_id (session_t *ss)
{
    dbg_return_if (ss == NULL, NULL);

    return ss->id;
}

/** 
 * \ingroup session
 * \brief   Set session variable with given name to a value
 *  
 * Put variable with \p name and \p value into \p ss.
 *
 * \param ss     session object
 * \param name   session variable name
 * \param value  session variable value
 *  
 * \return \c 0 if successful, non-zero on error
 */
int session_set(session_t *ss, const char *name, const char *value)
{
    var_t *v = NULL;

    dbg_err_if (ss == NULL);
    dbg_err_if (name == NULL);
    dbg_err_if (value == NULL);
    dbg_err_if (strlen(name) == 0);

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
 * \ingroup session
 * \brief   Get the amount of time a session has been inactive
 *  
 * Return the number of seconds since the session was last modified.
 *
 * \param ss  session object
 *  
 * \return
 *  - the number of seconds since last modification
 *  - \c -1 on error
 */
int session_age(session_t *ss)
{
    time_t now;

    dbg_return_if (ss == NULL, -1);

    now = time(0);

    /* ss->mtime must has been set into session_X_create funcs */
    return (int)(now - ss->mtime); /* in seconds */
}

/** 
 * \ingroup session
 * \brief   Remove all session variables
 *  
 * Remove all session variables from \p ss.
 *
 * \param ss  session object
 *  
 * \return \c 0 if successful, non-zero on error
 */
int session_clean(session_t *ss)
{
    var_t *v = NULL;

    dbg_err_if (ss == NULL);

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
 * \ingroup session
 * \brief   Delete session variable given a name
 *  
 * Delete session variable \p name in \p ss.
 *
 * \param ss    session object
 * \param name  session variable name
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int session_del(session_t *ss, const char *name)
{
    var_t *v = NULL;

    dbg_err_if (ss == NULL);
    dbg_err_if (name == NULL);
    
    dbg_err_if((v = vars_get(ss->vars, name)) == NULL);
    dbg_err_if(vars_del(ss->vars, v));
    var_free(v);

    return 0;
err:
    return ~0;
}

#ifdef SSL_ON
/**
 * \ingroup session
 * \brief   Set the key used to decrypt encrypted embedded content resources
 *  
 * The provided key will be stored in the user session and it will be used to
 * decrypt all embedded encrypted content.
 *
 * The server will return HTTP status 430 to notify the client that a key is
 * needed. Use "error.430" in the config file to create a page the user will
 * use to provide the key.
 *
 * \param ss    session object
 * \param data  key binary buffer
 * \param sz    key size
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int session_set_cipher_key(session_t *ss, const char *data, size_t sz)
{
    var_t *v = NULL;

    dbg_err_if(ss == NULL);
    dbg_err_if(ss->vars == NULL);
    dbg_err_if(data == NULL);
    dbg_err_if(sz == 0);

    /* remove the old key if it has been set */
    if((v = vars_get(ss->vars, SESSION_KEY_VAR)) != NULL)
    {
        dbg_err_if(vars_del(ss->vars, v));
        v = NULL;
    }

    /* may contain \0 values; treat it as opaque binary data */
    dbg_err_if(var_bin_create(SESSION_KEY_VAR, data, sz, &v));

    dbg_err_if(vars_add(ss->vars, v));
    v = NULL;

    return 0;
err:
    if(v)
        var_free(v);
    return ~0;
}

/**
 * \ingroup session
 * \brief   Return the key used to decrypt encrypted embedded content resources
 *  
 * Copies the key set with session_set_cipher_key to the given buffer.
 *
 * \p psz is a value-result and must be set to the size of the buffer before
 * calling the function; on return will contain the size of the key copied to
 * \p buf.
 *
 * \param ss    session object
 * \param buf   output buffer
 * \param psz   value-result; contains the size if \p buf on entrance and the written bytes on exit
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int session_get_cipher_key(session_t *ss, char *buf, size_t *psz)
{
    var_t *v = NULL;
    size_t vsize;

    dbg_err_if(ss == NULL);
    dbg_err_if(ss->vars == NULL);
    dbg_err_if(buf == NULL);
    dbg_err_if(psz == 0);
    dbg_err_if(*psz == 0);

    nop_err_if((v = vars_get(ss->vars, SESSION_KEY_VAR)) == NULL);

    vsize = var_get_value_size(v);

    dbg_err_if(vsize >= *psz);

    memcpy(buf, var_get_value(v), vsize);

    *psz = vsize;

    return 0;
err:
    if(v)
        var_free(v);
    return ~0;
}

#endif

int session_prv_save_to_io(session_t *ss, io_t *out)
{
    save_cb_params_t prm; 
    codec_t *zip = NULL, *cencrypt = NULL;

    dbg_err_if (ss == NULL);
    dbg_err_if (out == NULL);

#ifdef HAVE_LIBZ
    if(ss->so->compress)
    {
        dbg_err_if(codec_gzip_create(GZIP_COMPRESS, &zip));
        dbg_err_if(io_codec_add_tail(out, zip));
        zip = NULL; /* io_t owns it after io_codec_add_tail */
    }
#endif

#ifdef SSL_ON
    if(ss->so->encrypt)
    {
        dbg_err_if(codec_cipher_create(CIPHER_ENCRYPT, ss->so->cipher, 
            ss->so->cipher_key, ss->so->cipher_iv, &cencrypt));
        dbg_err_if(io_codec_add_tail(out, cencrypt));
        cencrypt = NULL; /* io_t owns it after io_codec_add_tail */
    }
#endif

    /* pass io and session pointers to the callback function */
    prm.io = out;
    prm.ss = ss;

    vars_foreach(ss->vars, session_prv_save_var, (void*)&prm);

    /* remove all codecs and flush */
    io_codecs_remove(out);

    return 0;
err:
    if(out)
        io_codecs_remove(out);
    if(zip)
        codec_free(zip);
    if(cencrypt)
        codec_free(cencrypt);
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
    unsigned char ekey[CODEC_CIPHER_KEY_BUFSZ]; /* key + padding block */
    unsigned char pkey[CODEC_CIPHER_KEY_BUFSZ];
    size_t nsz, vsz, eksz, pksz;
    int rc = ~0;

    dbg_err_if (v == NULL);
    /* dbg_err_if (vp == NULL); */

    memset(sname, 0, NAMESZ);
    memset(svalue, 0, VALSZ);

    /* buffers must be at least three times the src data to URL-encode  */
    nsz = 1 + 3 * strlen(var_get_name(v));  /* name buffer size  */
    vsz = 1 + 3 * var_get_value_size(v);    /* value buffer size */

#ifdef SSL_ON
    vsz += CODEC_CIPHER_BLOCK_LEN; /* encryption may enlarge the content up 
                                       to CODEC_CIPHER_BLOCK_LEN -1         */
#else
    u_unused_args(ekey, eksz);
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

#ifdef SSL_ON
        if(!strcmp(var_get_name(v), SESSION_KEY_VAR))
        {
			memset(pkey, 0, sizeof(pkey)); /* plain text key */
			memset(ekey, 0, sizeof(ekey)); /* encrypted text key */

			/* copy the actual key to a zero-ed buffer of size key (i.e. if the
			 * key is shorted then the buffer the trailing bytes will be 0s */
			pksz = var_get_value_size(v);

            err_err_ifm(pksz != CODEC_CIPHER_KEY_LEN,
                    "bad encryption key; it must be %u bytes long",
                    CODEC_CIPHER_KEY_LEN);

			memcpy(pkey, var_get_value(v), pksz);

			eksz = sizeof(ekey);;

            /* encrypt the key and save it to ekey */
            dbg_err_if(u_cipher_encrypt(EVP_aes_256_cbc(), 
                pprm->ss->so->session_key, pprm->ss->so->session_iv, 
                ekey, &eksz, pkey, pksz));

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

    dbg_err_if (so == NULL);
    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    dbg_err_if (pss == NULL);

    switch(so->type)
    {
    case SESSION_TYPE_FILE:
        dbg_err_if(session_file_create(so, rq, rs, &ss));
        break;
    case SESSION_TYPE_MEMORY:
        dbg_err_if(session_mem_create(so, rq, rs, &ss));
        break;
#ifdef SSL_ON
    case SESSION_TYPE_CLIENT:
        dbg_err_if(session_client_create(so, rq, rs, &ss));
        break;
#endif
    default:
        warn_err("bad session type");
    }

    /* may fail if the session does not exist */
    if(ss->id[0] != '\0')
    {
        (void) session_load(ss);

        if (session_age(ss) > so->max_age)
        {
            u_dbg("session %s expired", ss->id);
            (void) session_clean(ss);  /* remove all session variables */
            (void) session_remove(ss); /* remove the session itself    */
        }
    } 

    *pss = ss;

    return 0;
err:
    if(ss)
        session_free(ss);
    return ~0;
}

