/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: sup_emb.c,v 1.35 2008/10/27 21:28:04 tat Exp $
 */

#include "klone_conf.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <u/libu.h>
#include <klone/supplier.h>
#include <klone/io.h>
#include <klone/ioprv.h>
#include <klone/page.h>
#include <klone/http.h>
#include <klone/emb.h>
#include <klone/codecs.h>
#include <klone/ses_prv.h>
#include <klone/rsfilter.h>
#include <klone/dypage.h>
#include "http_s.h"

static int supemb_is_valid_uri(http_t *h, request_t *rq, const char *uri, 
        size_t len, void **handle, time_t *mtime)
{
    embres_t *e;
    embfile_t *ef;
    char filename[U_FILENAME_MAX];

    dbg_err_if (uri == NULL);
    dbg_err_if (mtime == NULL);
    dbg_err_if (len + 1 > U_FILENAME_MAX);

    u_unused_args(h, rq);

    memcpy(filename, uri, len);
    filename[len] = '\0';

    if(emb_lookup(filename, &e) == 0)
    {   /* resource found */

        *mtime = 0; /* don't cache */
        if(e->type == ET_FILE)
        {
            ef = (embfile_t*)e;  

            if(ef->encrypted == 0)
                *mtime = ef->mtime;
        } 

        *handle = NULL;

        return 1;
    }

err:
    return 0; /* not found */
}

static int supemb_get_cipher_key(request_t *rq, response_t *rs, char *key, 
    size_t keysz)
{
    session_t *ss = NULL;
    http_t *http = NULL;
    session_opt_t *so;
    vars_t *vars;
    var_t *v;

    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    dbg_err_if (key == NULL);

    /* get session options */
    dbg_err_if((http = request_get_http(rq)) == NULL);
    dbg_err_if((so = http_get_session_opt(http)) == NULL);

    /* create/get the session */
    dbg_err_if(session_create(so, rq, rs, &ss));

    /* get variables list */
    vars = session_get_vars(ss);
    dbg_err_if(vars == NULL);

    v = vars_geti(vars, SESSION_KEY_VAR, 0); 
    dbg_err_if(v == NULL); /* no such variable */

    dbg_err_if(var_get_value_size(v) > keysz);

    /* zero-out key array */
    memset(key, 0, keysz);

    /* set the key */
    memcpy(key, var_get_value(v), var_get_value_size(v));

    session_free(ss);

    return 0;
err:
    if(ss)
        session_free(ss);
    return ~0;
}

static int supemb_static_set_header_fields(request_t *rq, response_t *rs, 
    embfile_t *e, int *sai)
{
    vhost_t *vhost;

    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    dbg_err_if (e == NULL);
    dbg_err_if (sai == NULL);

    dbg_err_if((vhost = request_get_vhost(rq)) == NULL);

    /* set header fields based on embfile_t struct */

    /* set content-type, last-modified and content-length*/
    dbg_err_if(response_set_content_type(rs, e->mime_type));
    dbg_err_if(response_set_last_modified(rs, e->mtime));
    dbg_err_if(response_set_content_length(rs, e->file_size));

    /* if the client can accept deflated content don't uncompress the 
       resource but send as it is (if enabled by config) */
    if(vhost->send_enc_deflate)
    {
        if(e->comp && (*sai = request_is_encoding_accepted(rq, "deflate")) != 0)
        {   /* we can send compressed responses */
            dbg_err_if(response_set_content_encoding(rs, "deflate"));
            dbg_err_if(response_set_content_length(rs, e->size));
            /*  u_dbg("sending deflated content"); */
        } 
    }

    return 0;
err:
    return ~0;
}

static int supemb_serve_static(request_t *rq, response_t *rs, embfile_t *e)
{
    codec_t *gzip = NULL, *decrypt = NULL;
    int sai = 0; /* send as is */
    int decrypting = 0, ec = ~0;
    char key[CODEC_CIPHER_KEY_BUFSZ];
    codec_t *rsf = NULL;

    dbg_return_if (rq == NULL, ~0);
    dbg_return_if (rs == NULL, ~0);
    dbg_return_if (e == NULL, 0);

    /* create a response filter and attach it to the response io */
    dbg_err_if(response_filter_create(rq, rs, NULL, &rsf));
    dbg_err_if(io_codec_add_tail(response_io(rs), rsf));
    rsf = NULL;

    /* set HTTP header based on 'e' (we have the cipher key here) */
    dbg_err_if(supemb_static_set_header_fields(rq, rs, e, &sai));

    /* if this is a HEAD request print the header and exit */
    if(request_get_method(rq) == HM_HEAD)
        return 0; /* just the header is requested */

#ifdef HAVE_LIBZ
    /* if needed apply a gzip codec to uncompress content data */
    if(e->comp && !sai)
        dbg_err_if(codec_gzip_create(GZIP_UNCOMPRESS, &gzip));
#endif

#ifdef SSL_ON
    /* if the resource is encrypted unencrypt it using the key stored in 
       SESSION_KEY_VAR session variable */
    if(e->encrypted)
    {
        /* if the content is encrypted and there's no key then exit */
        if(supemb_get_cipher_key(rq, rs, key, sizeof(key)))
        {   
            dbg_err_if(response_set_status(rs, HTTP_STATUS_EXT_KEY_NEEDED));

            /* clean up and exit with no error to propagate the status code */
            ec = 0;

            dbg_err("cipher key not found, aborting");
        }

        /* do not cache encrypted content */
        response_disable_caching(rs);

        dbg_err_if(codec_cipher_create(CIPHER_DECRYPT, EVP_aes_256_cbc(),
                    key, NULL, &decrypt));
        /* delete the key from the stack */
        memset(key, 0, CODEC_CIPHER_KEY_BUFSZ);
    } 
#endif

    if(gzip)
    {   /* set gzip filter */
        dbg_err_if(io_codec_add_head(response_io(rs), gzip));
        gzip = NULL; /* io_t owns it after io_codec_add_tail */
    }

    if(decrypt)
    {   /* set decrypt filter */
        dbg_err_if(io_codec_add_head(response_io(rs), decrypt));
        decrypt = NULL; /* io_t owns it after io_codec_add_tail */
        decrypting = 1;
    }

    /* print out page content (the header will be autoprinted by the 
       response io filter) */
    dbg_err_if(io_write(response_io(rs), (const char*)e->data, e->size) 
        < e->size);

    /* remove and free the gzip/decrypt codecs (if they have been set) */
    dbg_err_if(io_codecs_remove(response_io(rs))); 

    return 0;
err:
    if(decrypting) /* usually wrong key given */
    {
        dbg_if(response_set_status(rs, HTTP_STATUS_EXT_KEY_NEEDED)); 
        ec = 0;
    }
    /* remove codecs and rs filter */
    dbg_if(io_codecs_remove(response_io(rs))); 
    if(decrypt)
        codec_free(decrypt);
    if(gzip)
        codec_free(gzip);
    return ec;
}

static int supemb_serve_dynamic(request_t *rq, response_t *rs, embpage_t *e)
{
    dypage_args_t args;

    args.rq = rq;
    args.rs = rs;
    args.ss = NULL; /* dypage_serve will set it before calling args.fun() */
    args.fun = e->fun;
    args.opaque = NULL;

    dbg_err_if(dypage_serve(&args));

    return 0;
err:
    return ~0;
}

static int supemb_serve(request_t *rq, response_t *rs)
{
    const char *file_name;
    embres_t *e;

    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    
    file_name = request_get_resolved_filename(rq);
    dbg_ifb(file_name == NULL || emb_lookup(file_name, &e))
    {
        response_set_status(rs, HTTP_STATUS_NOT_FOUND); 
        return 0;
    }

    /* u_dbg("serving %s", e->filename); */

    switch(e->type)
    {
    case ET_FILE:
        dbg_err_if(supemb_serve_static(rq, rs, (embfile_t*)e));
        break;
    case ET_PAGE:
        dbg_err_if(supemb_serve_dynamic(rq, rs, (embpage_t*)e));
        break;
    default:
        dbg_err_if("unknown res type");
    }

    return 0;
err:
    return ~0;
}

static int supemb_init(void)
{
    return 0;
}

static void supemb_term(void)
{
    return;
}

supplier_t sup_emb = {
    "embedded content supplier",
    supemb_init,
    supemb_term,
    supemb_is_valid_uri,
    supemb_serve
};

