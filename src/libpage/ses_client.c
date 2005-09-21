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
#include <klone/codgzip.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

#define KL1_CLISES_DATA     "KL1_CLISES_DATA"
#define KL1_CLISES_MTIME    "KL1_CLISES_MTIME"
#define KL1_CLISES_HMAC     "KL1_CLISES_HMAC"

enum { HMAC_KEY_SIZE = 128 };
static char g_key[HMAC_KEY_SIZE];   /* HMAC secret key                  */
static HMAC_CTX gs_hmac_ctx;        /* HMAC context                     */ 
static HMAC_CTX *g_hmac_ctx = &gs_hmac_ctx; /* HMAC context ptr         */ 
static int g_compress;              /* >0 if compression is requested   */

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
    enum { MTIME_SZ = 32 };
    io_t *io = NULL;
    codec_gzip_t *zip = NULL;
    size_t sz = 0;
    ssize_t blen;
    char *buf = NULL, mtime[MTIME_SZ];
    char bhmac[EVP_MAX_MD_SIZE], hmac[1000 + (EVP_MAX_MD_SIZE*3)];
    int bhmac_len;
    char ebuf[COOKIE_MAX_SIZE];

    if(vars_count(ss->vars) == 0)
        return 0; /* nothing to save */

    /* calc the maximum session data size (exact calc requires url encoding) */
    vars_foreach(ss->vars, (vars_cb_t)session_calc_maxsize, &sz);

    /* alloc a block to save the session */
    buf = u_calloc(sz + 128); /* size + some bytes for gzip header size */
    dbg_err_if(buf == NULL);

    /* create a big-enough in-memory io object */
    dbg_err_if(io_mem_create(buf, sz, 0, &io));

    if(g_compress)
    {
        dbg_err_if(codec_gzip_create(GZIP_COMPRESS, &zip));
        dbg_err_if(io_set_codec(io, (codec_t*)zip));
        zip = NULL; /* io_t owns it after io_set_codec */
    }

    vars_foreach(ss->vars, session_prv_save_var, io);

    /* this will free and flush codec buffers (so we can use io_tell safely) */
    dbg_err_if(io_set_codec(io, NULL));

    /* get real buffer size (not the size of underlaying buffer) */
    blen = io_tell(io);

    io_free(io); /* flush and close (session data stored in buf) */
    io = NULL;

    /* TODO encrypt buf if requested  */

    /* url-encode the buffer */
    warn_err_ifm(blen > COOKIE_MAX_SIZE, 
                "session data too big for client-side sessions");

    dbg_err_if(u_urlncpy(ebuf, buf, blen, URLCPY_ENCODE) <= 0);

    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_DATA, ebuf, 0, NULL, 
        NULL, 0));

    ss->mtime = time(0);
    dbg_err_if(u_snprintf(mtime, MTIME_SZ, "%lu", ss->mtime));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_MTIME, mtime, 0, NULL, 
        NULL, 0));

    /* calc HMAC hash of the data buf + mtime */
    HMAC_Init_ex(g_hmac_ctx, NULL, 0, NULL, NULL); /* reuse stored key and md */
    HMAC_Update(g_hmac_ctx, ebuf, strlen(ebuf));
    HMAC_Update(g_hmac_ctx, mtime, strlen(mtime));
    HMAC_Final(g_hmac_ctx, bhmac, &bhmac_len);

    /* encode the hash */
    dbg_err_if(u_urlncpy(hmac, bhmac, bhmac_len, URLCPY_ENCODE) <= 0);

    /* store the hash in a cookie */
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_HMAC, hmac, 0, NULL, 
        NULL, 0));

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
    char *cli_ebuf, *cli_hmac, *cli_mtime;
    char bhmac[EVP_MAX_MD_SIZE], hmac[1 + (EVP_MAX_MD_SIZE*3)],
        buf[COOKIE_MAX_SIZE]; 
    codec_gzip_t *zip = NULL;
    int bhmac_len;
    ssize_t c;

    /* extract session data, mtime and hmac from cookies */
    cli_ebuf = request_get_cookie(ss->rq, KL1_CLISES_DATA);
    cli_mtime = request_get_cookie(ss->rq, KL1_CLISES_MTIME);
    cli_hmac = request_get_cookie(ss->rq, KL1_CLISES_HMAC);

    dbg_err_if(cli_ebuf == NULL || cli_mtime == NULL || cli_hmac == NULL);

    /* calc HMAC hash of the data buf + mtime */
    HMAC_Init_ex(g_hmac_ctx, NULL, 0, NULL, NULL); /* reuse stored key and md */
    HMAC_Update(g_hmac_ctx, cli_ebuf, strlen(cli_ebuf));
    HMAC_Update(g_hmac_ctx, cli_mtime, strlen(cli_mtime));
    HMAC_Final(g_hmac_ctx, bhmac, &bhmac_len);

    /* encode the hash */
    dbg_err_if(u_urlncpy(hmac, bhmac, bhmac_len, URLCPY_ENCODE) <= 0);

    /* compare HMACs */
    if(strcmp(hmac, cli_hmac))
    {
        session_remove(ss); /* remove all bad stale data */
        warn_err("HMAC don't matching, rejecting session data");
    }

    /* hash ckeched not decode/uncompress/decrypt session data */

    /* set client provided mtime */
    ss->mtime = strtoul(cli_mtime, NULL, 0);

    dbg_err_if(strlen(cli_ebuf) > COOKIE_MAX_SIZE);

    /* url decode session data */
    dbg_err_if((c = u_urlncpy(buf, cli_ebuf, strlen(cli_ebuf), 
        URLCPY_DECODE)) <= 0);

    /* create an in-memory io object to read from */
    dbg_err_if(io_mem_create(buf, c, 0, &io));

    if(g_compress)
    {
        dbg_err_if(codec_gzip_create(GZIP_UNCOMPRESS, &zip));
        dbg_err_if(io_set_codec(io, (codec_t*)zip));
        zip = NULL; /* io_t owns it after io_set_codec */
    }

    /* load session vars */
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

/* this function will be called once by the server during startup */
int session_client_module_init(config_t *config)
{
    const char *algo, *compress, *crypt;
    const EVP_MD *md;      /* HMAC hash algorithm              */
    config_t *sk;

    config_get_subkey(config, "server.httpd.session", &sk); // FIXME
    config = sk;

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

    compress = config_get_subkey_value(config, "compress");
    if(compress)
    {
        if(strcasecmp(compress, "yes") == 0)
            g_compress = 1;
        else if(strcasecmp(compress, "no") == 0)
            g_compress = 0;
        else
            warn_err("config error: bad compress value");
    }
    crypt = config_get_subkey_value(config, "encrypt");
    if(crypt)
    {

    }

    /* initialize OpenSSL HMAC stuff */
    HMAC_CTX_init(g_hmac_ctx);

    // TODO gen key
    int i;
    for(i = 0; i < HMAC_KEY_SIZE; ++i)
            g_key[i] = i;

    /* init HMAC with our key and chose hash algorithm */
    HMAC_Init_ex(g_hmac_ctx, g_key, HMAC_KEY_SIZE, md, NULL);

    return 0;
err:
    return ~0;
}


