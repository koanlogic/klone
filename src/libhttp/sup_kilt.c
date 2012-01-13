/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: sup_kilt.c,v 1.1 2008/10/27 21:28:04 tat Exp $
 */

#include "klone_conf.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex.h>
#include <klone/supplier.h>
#include <klone/io.h>
#include <klone/utils.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/session.h>
#include <klone/kilt.h>
#include <klone/kilt_urls.h>
#include <klone/emb.h>

typedef struct url_to_ku_s
{
    regex_t re;
    kilt_url_t *ku;
} url_to_ku_t;

static url_to_ku_t *url_to_ku = NULL;

void kilt_run_script(dypage_args_t *args) 
{ 
    embpage_t *e;
    const char *script;

    script = dypage_get_param(args, "script");
    crit_err_ifm(script == NULL, "missing 'script' param");

    crit_err_ifm(emb_lookup(script, (embres_t**)&e), 
            "script %s not found", script);

    args->fun = e->fun;

    /* run the page code */
    e->fun(args);

    return; 
err:
    response_set_status(args->rs, HTTP_STATUS_NOT_FOUND); 
    return;
}

void kilt_show_params(dypage_args_t *args) 
{ 
    io_t *out = response_io(args->rs);
    int i;

    io_printf(out, "argc: %u<br>", args->argc);
    for(i = 0; i < args->argc; ++i)
        io_printf(out, "argv[%d]: %s<br>", i, args->argv[i]);

    io_printf(out, "<p>");

    io_printf(out, "nparams: %u<br>", args->nparams);
    for(i = 0; i < args->nparams; ++i)
        io_printf(out, "param['%s']: %s<br>", args->params[i].key, 
                args->params[i].val);

    return; 
}

static int kilt_is_valid_uri(http_t *h, request_t *rq, const char *uri, 
        size_t len, void **handle, time_t *mtime)
{
    url_to_ku_t *utk;
    char url[U_FILENAME_MAX];
    int i;

    u_unused_args(h, rq);

    dbg_return_if (uri == NULL, 0);
    dbg_return_if (mtime == NULL, 0);
    dbg_return_if (len + 1 > U_FILENAME_MAX, 0);
    dbg_return_if (url_to_ku == NULL, 0);

    memcpy(url, uri, len);
    url[len] = '\0';

    for(i = 0; i < kilt_nurls; ++i)
    {
        utk = &url_to_ku[i];

        if(regexec(&utk->re, url, 0, NULL, 0) == 0)
        {
            *handle = (void*)utk;
            *mtime = 0;
            return 1; /* found */
        }
    }

    return 0; 
}

static int kilt_serve(request_t *rq, response_t *rs)
{
    url_to_ku_t *utk;
    dypage_args_t args;
    regmatch_t subs[DYPAGE_MAX_PARAMS + 1];
    const char *file_name;
    char *argv[DYPAGE_MAX_PARAMS + 1];
    int i, argc;
    void *handle;

    /* get cached utk pointer (avoid running all regex's on this url again) */
    request_get_sup_info(rq, NULL, &handle, NULL);
    dbg_err_if(handle == NULL);

    utk = handle;

    file_name = request_get_filename(rq);
    dbg_err_if(file_name == NULL);

    dbg_err_if(regexec(&utk->re, file_name, DYPAGE_MAX_PARAMS, subs, 0));

    for(i = 0, argc = 0; subs[i].rm_so != subs[i].rm_eo; ++i, ++argc)
    {
        argv[i] = u_strndup(file_name + subs[i].rm_so, 
                        subs[i].rm_eo - subs[i].rm_so);
        dbg_err_if(argv[i] == NULL);
    }

    /* set dypage args first */
    args.rq = rq;
    args.rs = rs;
    args.ss = NULL; /* set by dypage_serve */
    args.fun = utk->ku->fun;
    args.opaque = NULL;

    /* kilt args */

    /* regex submatches */
    args.argc = argc;
    args.argv = argv;

    /* user provided named arguments */
    args.params = utk->ku->params;
    for(args.nparams  = 0; args.params[args.nparams].key; ++args.nparams)
        continue;

    /* run the function! */
    dypage_serve((dypage_args_t*)&args);

    for(i = 0; i < argc; ++i)
        u_free(argv[i]);

    return 0;
err:
    return ~0;
}

static int kilt_init(void)
{
    kilt_url_t *u;
    url_to_ku_t *utk;
    int i;

    url_to_ku = u_zalloc( (kilt_nurls + 1) * sizeof(url_to_ku_t));
    dbg_err_if(url_to_ku == NULL);

    for(u = kilt_urls, i = 0; i < kilt_nurls; ++u, ++i)
    {
        utk = &url_to_ku[i];

        /* save the compiled regex in utk.re */
        dbg_err_if(regcomp(&utk->re, u->pattern, REG_EXTENDED));

        /* save the ptr to the kilt_url object */
        utk->ku = u;

        dbg_err_if(utk->re.re_nsub > DYPAGE_MAX_PARAMS);
    }

    return 0;
err:
    return ~0;
}

static void kilt_term(void)
{
    url_to_ku_t *utk;
    int i;

    if(url_to_ku)
    {
        for(i = 0; i < kilt_nurls; ++i)
        {
            utk = &url_to_ku[i];
            regfree(&utk->re);
        }
        u_free(url_to_ku); url_to_ku = NULL;
    }

    return;
}

supplier_t sup_kilt = {
    "kilt supplier",
    kilt_init,
    kilt_term,
    kilt_is_valid_uri,
    kilt_serve
};

