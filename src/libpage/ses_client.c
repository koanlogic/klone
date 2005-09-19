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
#include <openssl/hmac.h>

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

#define KL1_CLISES_DATA     "KL1_CLISES_DATA"
#define KL1_CLISES_MTIME    "KL1_CLISES_MTIME"
#define KL1_CLISES_HMAC     "KL1_CLISES_HMAC"

enum { HMAC_KEY_SIZE = 128 };
static int g_module_ready;          /* >0 if module_init has been called*/
static char g_key[HMAC_KEY_SIZE];   /* HMAC secret key                  */
static HMAC_CTX *g_hmac_ctx;        /* HMAC context                     */



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
    enum { MTIMES_SZ = 32 };
    io_t *io = NULL;
    size_t sz = 0;
    char *buf = NULL, mtimes[MTIMES_SZ];
    char bhmac[EVP_MAX_MD_SIZE], hmac[EVP_MAX_MD_SIZE*3];
    int bhmac_len;

    session_remove(ss);

    /* calc the maximum session data size (exact calc requires url encoding) */
    vars_foreach(ss->vars, (vars_cb_t)session_calc_maxsize, &sz);

    /* alloc a block to save the session */
    buf = u_malloc(sz);
    dbg_err_if(buf == NULL);

    /* create a big-enough in-memory io object */
    dbg_err_if(io_mem_create(buf, sz, 0, &io));

    /* TODO set gzip codec if requested */

    vars_foreach(ss->vars, session_prv_save_var, io);

    /* TODO [encrypt |] b64 buf and store it into cookies */

    buf[sz] = 0; // FIXME be sure that is zero-term
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_DATA, buf, 0, NULL, 
        NULL, 0));

    dbg_err_if(u_snprintf(mtimes, MTIMES_SZ, "%lu", ss->mtime));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_MTIME, mtimes, 0, NULL, 
        NULL, 0));

    /* TODO calc MAC hash of the data buf + mtime and store it in a cookie */
    HMAC_Init_ex(g_hmac_ctx, NULL, 0, NULL, NULL); /* reuse stored key and md */
    HMAC_Update(g_hmac_ctx, buf, strlen(buf));
    HMAC_Update(g_hmac_ctx, mtimes, strlen(mtimes));
    HMAC_Final(g_hmac_ctx, bhmac, &bhmac_len);

    dbg_err_if(u_urlncpy(hmac, bhmac, bhmac_len, URLCPY_ENCODE));

    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_HMAC, hmac, 0, NULL, 
        NULL, 0));

    io_free(io); /* session data stored in buf */
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
    ss = ss;
    return 0;
}

static int session_client_remove(session_t *ss)
{
    /* removes all clises-related cookies */
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_DATA, NULL, 0, NULL, 
        NULL, 0));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_MTIME, NULL, 0, NULL, 
        NULL, 0));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_HMAC, NULL, 0, NULL, 
        NULL, 0));

    return 0;
err:
    return ~0;
}

static int module_init(config_t *config)
{
    const char *algo;
    const EVP_MD *md;      /* HMAC hash algorithm              */
    
    algo = config_get_subkey_value(config, "hash_function");
    if(algo)
    {
        if(strcasecmp(algo, "md5") == 0)
            md = EVP_md5();
        else if(strcasecmp(algo, "sha1") == 0)
            md = EVP_sha1();
        else if(strcasecmp(algo, "ripemd160") == 0)
            md = EVP_ripemd160();
        else
            warn_err("config error: bad hash_function");
    } else
        md = EVP_md5(); /* default */

    /* initialize OpenSSL HMAC stuff */
    HMAC_CTX_init(g_hmac_ctx);
    /* let it store key and algo */
    // TODO gen key
    HMAC_Init_ex(g_hmac_ctx, g_key, HMAC_KEY_SIZE, md, NULL);

    return 0;
err:
    return ~0;
}

int session_client_create(config_t *config, request_t *rq, response_t *rs, 
        session_t **pss)
{
    session_t *ss = NULL;

    if(!g_module_ready)
        dbg_err_if(module_init(config));

    ss = u_calloc(sizeof(session_t));
    dbg_err_if(ss == NULL);

    ss->load = session_client_load;
    ss->save = session_client_save;
    ss->remove = session_client_remove;
    ss->term = session_client_term;
    ss->mtime = time(0);

    dbg_err_if(module_init(config));

    dbg_err_if(session_prv_init(ss, rq, rs));

    dbg_err_if(u_path_snprintf(ss->filename, PATH_MAX, "%s.ss", ss->id));

    *pss = ss;

    return 0;
err:
    if(ss)
        session_free(ss);
    return ~0;
}
