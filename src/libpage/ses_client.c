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
#include <klone/ses_prv.h>
#include <klone/codecs.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <u/libu.h>

#define KL1_CLISES_DATA     "KL1_CLISES_DATA"
#define KL1_CLISES_MTIME    "KL1_CLISES_MTIME"
#define KL1_CLISES_HMAC     "KL1_CLISES_HMAC"

enum { HMAC_HEX_SIZE = 2*EVP_MAX_MD_SIZE + 1 };

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

static ssize_t session_client_enccpy(session_opt_t *so, char *dst, 
    const char *src, size_t slen)
{
    ssize_t dlen = 0;  /* dst buffer length */
    int wr;
    
    dbg_err_if(!EVP_EncryptInit_ex(&so->cipher_enc_ctx, so->cipher, NULL, 
        so->cipher_key, so->cipher_iv));

    dbg_err_if(!EVP_EncryptUpdate(&so->cipher_enc_ctx, dst, &wr, src, slen));
    dlen += wr;
    dst += wr;

    dbg_err_if(!EVP_EncryptFinal_ex(&so->cipher_enc_ctx, dst, &wr));

    return dlen + wr;;
err:
    return -1;
}

static ssize_t session_client_deccpy(session_opt_t *so, char *dst, 
    const char *src, size_t slen)
{
    ssize_t dlen = 0;  /* dst buffer length */
    int wr;
    
    dbg_err_if(!EVP_DecryptInit_ex(&so->cipher_dec_ctx, so->cipher, NULL,
        so->cipher_key, so->cipher_iv));

    dbg_err_if(!EVP_DecryptUpdate(&so->cipher_dec_ctx, dst, &wr, src, slen));
    dlen += wr;
    dst += wr;

    dbg_err_if(!EVP_DecryptFinal_ex(&so->cipher_dec_ctx, dst, &wr));

    return dlen + wr;;
err:
    return -1;
}

/* calc the hmac of data+sid+mtime */
static int session_client_hmac(HMAC_CTX *ctx, char *hmac, size_t hmac_sz, 
    const char *data, const char *sid, const char *mtime)
{
    char mac[EVP_MAX_MD_SIZE];
    int mac_len;

    /* hmac must be at least 'EVP_MAX_MD_SIZE*2 + 1' (it will be hex encoded) */
    dbg_err_if(hmac_sz < EVP_MAX_MD_SIZE*2 + 1);

    /* calc HMAC hash of the data buf + mtime (reuse stored key and md) */
    HMAC_Init_ex(ctx, NULL, 0, NULL, NULL); 
    HMAC_Update(ctx, data, strlen(data));
    HMAC_Update(ctx, sid, strlen(sid));
    HMAC_Update(ctx, mtime, strlen(mtime));
    HMAC_Final(ctx, mac, &mac_len);

    /* encode the hash */
    dbg_err_if(u_hexncpy(hmac, mac, mac_len, URLCPY_ENCODE) <= 0);

    return 0;
err:
    return -1;
}

static int session_client_save(session_t *ss)
{
    enum { 
        MTIME_SIZE = 32, 
        /* cookie size + MAC size + gzip header & enlargement on worse case */
        BUF_SIZE = COOKIE_MAX_SIZE + EVP_MAX_BLOCK_LENGTH + 96 
    };
    io_t *io = NULL;
    codec_t *zip = NULL;
    session_opt_t *so = ss->so;
    char hmac[HMAC_HEX_SIZE], buf[BUF_SIZE], ebuf[BUF_SIZE], mtime[MTIME_SIZE];
    size_t sz = 0;
    ssize_t blen;

    if(vars_count(ss->vars) == 0)
        return 0; /* nothing to save */

    /* calc the maximum session data size (exact calc requires url encoding) */
    vars_foreach(ss->vars, (vars_cb_t)session_calc_maxsize, &sz);

    /* create a big-enough in-memory io object */
    dbg_err_if(io_mem_create(buf, sz, 0, &io));

    if(so->compress)
    {
        dbg_err_if(codec_gzip_create(GZIP_COMPRESS, &zip));
        dbg_err_if(io_codec_add_tail(io, zip));
        zip = NULL; /* io_t owns it after io_codec_add_tail */
    }

    vars_foreach(ss->vars, session_prv_save_var, io);

    /* this will free and flush codec buffers (so we can use io_tell safely) */
    dbg_err_if(io_codecs_remove(io));

    /* get real buffer size (not the size of underlaying buffer) */
    blen = io_tell(io);

    io_free(io); /* flush and close (session data stored in buf) */
    io = NULL;

    /* encrypt buf if requested */
    if(so->encrypt)
    {
        /* encrypt to buffer */
        memcpy(ebuf, buf, blen);
        dbg_err_if((blen = session_client_enccpy(so, buf, ebuf, blen)) < 0);
    }

    warn_err_ifm(blen > COOKIE_MAX_SIZE, 
                "session data too big for client-side sessions");

    /* hex-encode the buffer */
    dbg_err_if(u_hexncpy(ebuf, buf, blen, URLCPY_ENCODE) <= 0);

    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_DATA, ebuf, 0, NULL, 
        NULL, 0));

    ss->mtime = time(0);
    dbg_err_if(u_snprintf(mtime, MTIME_SIZE, "%lu", ss->mtime));
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_MTIME, mtime, 0, NULL, 
        NULL, 0));

    /* calc the HMAC */
    dbg_err_if(session_client_hmac(&so->hmac_ctx, hmac, HMAC_HEX_SIZE, ebuf, 
        ss->id, mtime));

    /* store the hash in a cookie */
    dbg_err_if(response_set_cookie(ss->rs, KL1_CLISES_HMAC, hmac, 0, NULL, 
        NULL, 0));

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int session_client_load(session_t *ss)
{
    codec_t *zip = NULL;
    io_t *io = NULL;
    session_opt_t *so = ss->so;
    char hmac[HMAC_HEX_SIZE], buf[COOKIE_MAX_SIZE], tmpbuf[COOKIE_MAX_SIZE]; 
    const char *cli_ebuf, *cli_hmac, *cli_mtime;
    ssize_t c;

    /* extract session data, mtime and hmac from cookies */
    cli_ebuf = request_get_cookie(ss->rq, KL1_CLISES_DATA);
    cli_mtime = request_get_cookie(ss->rq, KL1_CLISES_MTIME);
    cli_hmac = request_get_cookie(ss->rq, KL1_CLISES_HMAC);

    dbg_err_if(cli_ebuf == NULL || cli_mtime == NULL || cli_hmac == NULL);

    /* calc the HMAC */
    dbg_err_if(session_client_hmac(&so->hmac_ctx, hmac, HMAC_HEX_SIZE, 
        cli_ebuf, ss->id, cli_mtime));

    /* compare HMACs */
    if(strcmp(hmac, cli_hmac))
    {
        session_remove(ss); /* remove all bad stale data */
        warn_err("HMAC don't match, rejecting session data");
    }

    /* hash ckeched not decode/uncompress/decrypt session data */

    /* set client provided mtime */
    ss->mtime = strtoul(cli_mtime, NULL, 0);

    dbg_err_if(strlen(cli_ebuf) > COOKIE_MAX_SIZE);

    /* hex decode session data */
    dbg_err_if((c = u_hexncpy(buf, cli_ebuf, strlen(cli_ebuf), 
        URLCPY_DECODE)) <= 0);

    c--; /* ignore last '\0' that hexncpy adds */

    /* decrypt the buffer if needed */
    if(so->encrypt)
    {
        /* decrypt to buffer */
        memcpy(tmpbuf, buf, c);
        dbg_err_if((c = session_client_deccpy(so, buf, tmpbuf, c)) < 0);
    }

    /* create an in-memory io object to read from */
    dbg_err_if(io_mem_create(buf, c, 0, &io));

    if(so->compress)
    {
        dbg_err_if(codec_gzip_create(GZIP_UNCOMPRESS, &zip));
        dbg_err_if(io_codec_add_tail(io, zip));
        zip = NULL; /* io_t owns it after io_codec_add_tail */
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
    u_unused_args(ss);
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

int session_client_create(session_opt_t *so, request_t *rq, response_t *rs, 
        session_t **pss)
{
    session_t *ss = NULL;

    ss = u_zalloc(sizeof(session_t));
    dbg_err_if(ss == NULL);

    ss->load = session_client_load;
    ss->save = session_client_save;
    ss->remove = session_client_remove;
    ss->term = session_client_term;
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
int session_client_module_init(u_config_t *config, session_opt_t *so)
{
    u_config_t *c;
    const char *v;

    /* defaults */
    so->compress = 0;
    so->encrypt = 1;
    so->hash = EVP_sha1(); 
    so->cipher = NULL;

    dbg_err_if(u_config_get_subkey(config, "client", &c));

    if((v = u_config_get_subkey_value(c, "hash_function")) != NULL)
    {
        if(!strcasecmp(v, "md5"))
            so->hash = EVP_md5();
        else if(!strcasecmp(v, "sha1"))
            so->hash = EVP_sha1();
        else if(!strcasecmp(v, "ripemd160"))
            so->hash = EVP_ripemd160();
        else
            warn_err("config error: bad hash_function");
    } 

    if((v = u_config_get_subkey_value(c, "compress")) != NULL)
    {
        if(!strcasecmp(v, "yes"))
            so->compress = 1;
        else if(!strcasecmp(v, "no"))
            so->compress = 0;
        else
            warn_err("config error: bad compress value");
    }

    if((v = u_config_get_subkey_value(c, "encrypt")) != NULL)
    {
        if(!strcasecmp(v, "yes"))
            so->encrypt = 1;
        else if(!strcasecmp(v, "no"))
            so->encrypt = 0;
        else
            warn_err("config error: bad encrypt value");
    }

    if(so->encrypt)
    {
        so->cipher = EVP_aes_256_cbc(); /* use AES-256 in CBC mode */

        EVP_add_cipher(so->cipher);

        EVP_CIPHER_CTX_init(&so->cipher_enc_ctx);
        EVP_CIPHER_CTX_init(&so->cipher_dec_ctx);

        dbg_err_if(!RAND_bytes(so->cipher_key, CIPHER_KEY_SIZE));
        dbg_err_if(!RAND_pseudo_bytes(so->cipher_iv, CIPHER_IV_SIZE));
    }

    /* initialize OpenSSL HMAC stuff */
    HMAC_CTX_init(&so->hmac_ctx);

    /* gen HMAC key */
    dbg_err_if(!RAND_bytes(so->hmac_key, HMAC_KEY_SIZE));

    /* init HMAC with our key and chose hash algorithm */
    HMAC_Init_ex(&so->hmac_ctx, so->hmac_key, HMAC_KEY_SIZE, so->hash, NULL);

    return 0;
err:
    return ~0;
}


