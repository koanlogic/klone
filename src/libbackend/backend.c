#include <unistd.h>
#include <klone/debug.h>
#include <klone/utils.h>
#include <klone/backend.h>
#include <klone/config.h>
#include "conf.h"

extern backend_t http;

#ifdef HAVE_OPENSSL
extern backend_t https;
#endif

backend_t *backend_list[] = { 
    &http, 
    #ifdef HAVE_OPENSSL
    &https, 
    #endif
    0 };


int backend_create(const char *proto, config_t *config, backend_t **pbe)
{
    backend_t *be = NULL, **pp;

    be = u_calloc(sizeof(backend_t));
    dbg_err_if(be == NULL);

    /* look for the requested backend */
    for(pp = backend_list; *pp != NULL; ++pp)
    {
        if(strcasecmp((*pp)->proto, proto) == 0)
        {   /* found */
            memcpy(be, *pp, sizeof(backend_t));
            be->config = config;
            if(be->cb_init)
                be->cb_init(be);
            *pbe = be;
            return 0;
        }
    }

    warn_err("backend type \"%s\" not found", proto);

    return 0;
err:
    if(be)
        backend_free(be);
    return ~0;
}

int backend_serve(backend_t *be, int fd)
{
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
        if(be->cb_term)
            be->cb_term(be);
        u_free(be);
    }
    return 0;
}

