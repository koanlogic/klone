#include "klone_conf.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef HAVE_LIBOPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <klone/ccipher.h>
#endif /* HAVE_LIBOPENSSL */
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
    so->compress = 0;
    so->encrypt = 0;

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
           warn_err("config error: bad session type (typo or missing library)");
    }

    /* set max_age */
    if((v = u_config_get_subkey_value(c, "max_age")) != NULL)
        max_age = MAX(atoi(v) * 60, 60); /* min value: 1 min */

    /* set compression flag */
    dbg_err_if(u_config_get_subkey_value_b(c, "compress", 0, &so->compress));

    /* set encryption flag */
    dbg_err_if(u_config_get_subkey_value_b(c, "encrypt", 0, &so->encrypt));

    #ifndef HAVE_LIBZ
    if(so->compress)
        warn_err("config error: compression is enabled but libz is not "
                 "linked");
    #endif

    #ifndef HAVE_LIBOPENSSL
    if(so->encrypt)
        warn_err("config error: encryption is enabled but OpenSSL is not "
                 "linked");
    #else
    /* init cipher EVP algo, the random key and IV */
    so->cipher = EVP_aes_256_cbc(); /* use AES-256 in CBC mode */

    EVP_add_cipher(so->cipher);

    /* key and iv for client-side session */
    dbg_err_if(!RAND_bytes(so->cipher_key, CIPHER_KEY_SIZE));
    dbg_err_if(!RAND_pseudo_bytes(so->cipher_iv, CIPHER_IV_SIZE));

    /* create a random key and iv to crypt the KLONE_CIPHER_KEY variable */
    dbg_err_if(!RAND_bytes(so->session_key, CIPHER_KEY_SIZE));
    dbg_err_if(!RAND_pseudo_bytes(so->session_iv, CIPHER_IV_SIZE));

    #endif

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

int session_prv_calc_maxsize(var_t *v, void *p)
{
    const char *value = NULL;
    size_t *psz = (size_t*)p;

    dbg_err_if(v == NULL || var_get_name(v) == NULL || psz == NULL);

    #ifdef HAVE_LIBOPENSSL
    if(*psz == 0)
    {   /* first time here */
        *psz = CODEC_CIPHER_BLOCK_SIZE;
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
    if(buf)
        u_free(buf);
    return ~0;
}

static int session_is_good_id(const char *id)
{
    const char *p;
    size_t len;

    dbg_return_if(id == NULL, 0);

    dbg_ifb((len = strlen(id)) != MD5_DIGEST_LEN)
        return 0; /* wrong length */

    for(p = id; len; --len, ++p)
    {
        /* if is hex */
        if(! ((*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f') || 
              (*p >= '0' && *p <= '9')) )
        return 0; /* not safe */
    }

    return 1; /* good */
}

static int session_gen_id(session_t *ss)
{
    char buf[256];
    struct timeval tv;

    /* gen a new one */
    gettimeofday(&tv, NULL);

    dbg_err_if(u_snprintf(buf, 255, "%lu%d%lu%d", tv.tv_sec, getpid(), 
        tv.tv_usec, rand()));

    /* return the md5 (in hex) buf */
    dbg_err_if(u_md5(buf, strlen(buf), ss->id));

    /* set the ID cookie */
    dbg_err_if(response_set_cookie(ss->rs, SID_NAME, ss->id, 0, NULL, 
        NULL, 0));

    return 0;
err:
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

    if(!strlen(ss->id))
        dbg_err_if(session_gen_id(ss));

    return ss->save(ss);
err:
    return ~0;
}

int session_remove(session_t *ss)
{
    dbg_return_if(ss->remove == NULL, ~0);

    ss->removed = 1;

    return ss->remove(ss);
}

int session_prv_init(session_t *ss, request_t *rq, response_t *rs)
{
    const char *sid;
    addr_t *addr;

    dbg_err_if(vars_create(&ss->vars));

    ss->rq = rq;
    ss->rs = rs;


    sid = request_get_cookie(ss->rq, SID_NAME);
    if(sid && session_is_good_id(sid))
    {   
        dbg_err_if(u_snprintf(ss->id, MD5_DIGEST_BUFSZ, "%s", sid));
        ss->id[MD5_DIGEST_LEN] = 0;

        dbg_err_if((addr = request_get_addr(rq)) == NULL);
        switch(addr->type)
        {
        case ADDR_IPV4:
            dbg_err_if(u_path_snprintf(ss->filename, U_FILENAME_MAX, 
                "%s/klone_sess_%s_%lu", ss->so->path, ss->id, 
                addr->sa.sin.sin_addr));
            break;
        case ADDR_IPV6:
            /* FIXME: add ipv6 address in session filename */
            dbg_err_if(u_path_snprintf(ss->filename, U_FILENAME_MAX, 
                "%s/klone_sess_%s", ss->so->path, ss->id));
            break;
        #ifdef OS_UNIX
        case ADDR_UNIX:
            /* FIXME: add unix address in session filename */
            dbg_err_if(u_path_snprintf(ss->filename, U_FILENAME_MAX, 
                "%s/klone_sess_%s", ss->so->path, ss->id));
            break;
        #endif
        }
    }

    return 0;
err:
    return ~0;
}

int session_prv_load_from_io(session_t *ss, io_t *io)
{
    u_string_t *line = NULL;
    var_t *v = NULL;
    codec_t *unzip = NULL, *decrypt = NULL;
    unsigned char key[CODEC_CIPHER_KEY_SIZE];
    size_t ksz;


    #ifdef HAVE_LIBOPENSSL
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

            #ifdef HAVE_LIBOPENSSL
            if(!strcmp(var_get_name(v), "KLONE_CIPHER_KEY"))
            {
                /* decrypt key and save it to key */
                memset(key, 0, sizeof(key));
                dbg_ifb(u_cipher_decrypt(EVP_aes_256_cbc(), ss->so->session_key,
                    ss->so->session_iv, key, &ksz, 
                    var_get_value(v), var_get_value_size(v)))
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


/*
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
 * \brief   Get session variables
 *  
 * Return a vars_t containing the session variables.
 *
 * \param ss  session object
 *  
 * \return
 *  - the variables' list of the given \a ss
 */
vars_t *session_get_vars(session_t *ss)
{
    return ss->vars;
}

/**
 * \brief   Get session variable with given name
 *
 * Return a string representation of variable in \a ss with given \a name.
 *
 * \param ss    session object
 * \param name  session variable name
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
 * \brief   Set session variable with given name to a value
 *  
 * Put variable with \a name and \a value into \a ss.
 *
 * \param ss     session object
 * \param name   session variable name
 * \param value  session variable value
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

    now = time(0);

    /* ss->mtime must has been set into session_X_create funcs */
    return (int)(now - ss->mtime); /* in seconds */
}

/** 
 * \brief   Remove all session variables
 *  
 * Remove all session variables from \a ss.
 *
 * \param ss  session object
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
 * \brief   Delete session variable given a name
 *  
 * Delete session variable \a name in \a ss.
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
    codec_t *zip = NULL, *cencrypt = NULL;

    dbg_err_if(ss == NULL || out == NULL);

    #ifdef HAVE_LIBZ
    if(ss->so->compress)
    {
        dbg_err_if(codec_gzip_create(GZIP_COMPRESS, &zip));
        dbg_err_if(io_codec_add_tail(out, zip));
        zip = NULL; /* io_t owns it after io_codec_add_tail */
    }
    #endif

    #ifdef HAVE_LIBOPENSSL
    if(ss->so->encrypt)
    {
        dbg_err_if(codec_cipher_create(CIPHER_ENCRYPT, ss->so->cipher, 
            ss->so->cipher_key, ss->so->cipher_iv, &cencrypt));
        dbg_err_if(io_codec_add_tail(out, cencrypt));
        cencrypt = NULL; /* io_t owns it after io_codec_add_tail */
    }
    #endif

    /* pass io and session poiters to the callback function */
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
    unsigned char ekey[CODEC_CIPHER_KEY_SIZE + CODEC_CIPHER_BLOCK_SIZE + 1]; 
    size_t eksz, nsz, vsz;
    int rc = ~0;

    /* buffers must be at least three times the src data to URL-encode  */
    nsz = 1 + 3 * strlen(var_get_name(v));  /* name buffer size  */
    vsz = 1 + 3 * var_get_value_size(v);    /* value buffer size */

    #ifdef HAVE_LIBOPENSSL
    vsz += CODEC_CIPHER_BLOCK_SIZE; /* encryption may enlarge the content up 
                                       to CODEC_CIPHER_BLOCK_SIZE -1         */
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

        #ifdef HAVE_LIBOPENSSL
        if(!strcmp(var_get_name(v), "KLONE_CIPHER_KEY"))
        {
            /* encrypt key and save it to ekey */
            dbg_err_if(u_cipher_encrypt(EVP_aes_256_cbc(), 
                pprm->ss->so->session_key, pprm->ss->so->session_iv, 
                ekey, &eksz, var_get_value(v), var_get_value_size(v)));

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
