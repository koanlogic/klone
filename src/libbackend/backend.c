/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: backend.c,v 1.24 2006/10/11 16:31:42 tat Exp $
 */

#include <unistd.h>
#include <klone/backend.h>
#include <klone/server.h>
#include <klone/klog.h>
#include <klone/context.h>
#include <u/libu.h>
#include "klone_conf.h"

extern backend_t be_http;

#ifdef SSL_ON
extern backend_t be_https;
#endif

backend_t *backend_list[] = { 
    &be_http, 
#ifdef SSL_ON
    &be_https, 
#endif
    0 };


static int backend_set_model(backend_t *be, const char *v)
{
    dbg_return_if (v == NULL, ~0);
    dbg_return_if (be == NULL, ~0);

    if(!strcasecmp(v, "iterative"))
        be->model = SERVER_MODEL_ITERATIVE;
#ifdef HAVE_FORK
    else if(!strcasecmp(v, "fork"))
        be->model = SERVER_MODEL_FORK;
    else if(!strcasecmp(v, "prefork"))
        be->model = SERVER_MODEL_PREFORK;
#endif  /* HAVE_FORK */
    else
        warn_err("unknown server model [%s]", v);

    return 0;
err:
    return ~0;
}

int backend_create(const char *proto, u_config_t *config, backend_t **pbe)
{
    backend_t *be = NULL, **pp;
    const char *v;

    dbg_return_if (proto == NULL, ~0);
    dbg_return_if (config == NULL, ~0);
    dbg_return_if (pbe == NULL, ~0);
    
    be = u_zalloc(sizeof(backend_t));
    dbg_err_if(be == NULL);

    /* find the requested backend type */
    for(pp = backend_list; *pp != NULL; ++pp)
        if(!strcasecmp((*pp)->proto, proto))
            break; /* found */

    warn_err_ifm(*pp == NULL, "backend type \"%s\" not found", proto);

    /* copy static backend struct fields */
    memcpy(be, *pp, sizeof(backend_t));

    be->config = config;
#if defined(OS_WIN) || !defined(HAVE_FORK)
    be->model = SERVER_MODEL_ITERATIVE;  /* default */
#else
    be->model = SERVER_MODEL_PREFORK;  /* default */
#endif

    if((v = u_config_get_subkey_value(config, "model")) != NULL)
        dbg_err_if(backend_set_model(be, v));

    if(be->model == SERVER_MODEL_FORK)
    {
        /* max # of child allowed to run at once */
        dbg_err_if(u_config_get_subkey_value_i(config, "fork.max_child", 
            SERVER_MAX_BACKEND_CHILD, (int *)&be->max_child));
    }

    if(be->model == SERVER_MODEL_PREFORK)
    {
        /* max # of child allowed to run at once */
        dbg_err_if(u_config_get_subkey_value_i(config, "prefork.max_child", 
            SERVER_MAX_BACKEND_CHILD, (int *)&be->max_child));
        
        /* # of child to run at startup */
        dbg_err_if(u_config_get_subkey_value_i(config, "prefork.start_child", 
            SERVER_PREFORK_START_CHILD, (int *)&be->start_child));

        /* max # of requests a child process will handle before it dies */
        dbg_err_if(u_config_get_subkey_value_i(config, 
            "prefork.max_requests_per_child", SERVER_PREFORK_MAX_RQ_CHILD, 
            (int *)&be->max_rq_xchild));

        /* set start_child child to be forked when possible */
        be->fork_child = be->start_child;
    }

    /* call backend initialization function */
    if(be->cb_init)
        warn_err_ifm(be->cb_init(be), "backend (%s) init error", proto);

    *pbe = be;

    return 0;
err:
    U_FREE(be);
    return ~0;
}

int backend_serve(backend_t *be, int fd)
{
    dbg_return_if (be == NULL, ~0);
    dbg_return_if (fd < 0, ~0);
    
    dbg_err_if(be->cb_serve == NULL);

    be->cb_serve(be, fd);

    return 0;
err:
    return ~0;
}

int backend_free(backend_t *be)
{
    if(be)
    {
        /* children must not call klog_close (see comment in server.c) */
        if(be->klog && ctx->pipc == 0)
            klog_close(be->klog);
        if(be->cb_term)
            be->cb_term(be);
        U_FREE(be);
    }
    return 0;
}

