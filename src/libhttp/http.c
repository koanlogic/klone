/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: http.c,v 1.26 2005/11/24 21:35:18 tho Exp $
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_LIBOPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif  /* HAVE_LIBOPENSSL */
#include <u/libu.h>
#include <klone/utils.h>
#include <klone/klone.h>
#include <klone/server.h>
#include <klone/broker.h>
#include <klone/request.h>
#include <klone/ses_prv.h>
#include <klone/response.h>
#include <klone/backend.h>
#include <klone/io.h>
#include <klone/timer.h>
#include <klone/tls.h>
#include <klone/ses_prv.h>
#include "http_s.h"

struct http_status_map_s
{
    int status;
    const char *desc;
} http_status_map[] = {
    { HTTP_STATUS_OK                    , "OK"                      },
    { HTTP_STATUS_NOT_FOUND             , "Not Found"               },
    { HTTP_STATUS_INTERNAL_SERVER_ERROR , "Internal Server Error"   },
    { HTTP_STATUS_NOT_MODIFIED          , "Not Modified"            },
    { HTTP_STATUS_MOVED_PERMANENTLY     , "Moved Permanently"       },
    { HTTP_STATUS_MOVED_TEMPORARILY     , "Moved Temporarily"       },
    { HTTP_STATUS_CREATED               , "Created"                 },
    { HTTP_STATUS_ACCEPTED              , "Accepted"                },
    { HTTP_STATUS_NO_CONTENT            , "No Content"              },
    { HTTP_STATUS_BAD_REQUEST           , "Bad Request"             },
    { HTTP_STATUS_UNAUTHORIZED          , "Unauthorized"            },
    { HTTP_STATUS_FORBIDDEN             , "Forbidden"               },
    { HTTP_STATUS_NOT_IMPLEMENTED       , "Not Implemented"         },
    { HTTP_STATUS_BAD_GATEWAY           , "Bad Gateway"             },
    { HTTP_STATUS_SERVICE_UNAVAILABLE   , "Service Unavailable"     },
    { 0                                 , NULL                      }

};

/* in cgi.c */
int cgi_set_request(request_t *rq);

session_opt_t *http_get_session_opt(http_t *http)
{
    dbg_return_if (http == NULL, NULL);

    return http->sess_opt;
}

u_config_t *http_get_config(http_t* http)
{
    dbg_return_if (http == NULL, NULL);

    return http->config;
}

const char *http_get_status_desc(int status)
{
    struct http_status_map_s *map = http_status_map;
    const char *msg = "Unknown Status Code";

    for( ; map->status; ++map)
        if(map->status == status)
        {
            msg = map->desc;
            break;
        }

    return msg;
}

int http_alias_resolv(http_t *h, char *dst, const char *filename, size_t sz)
{
    static const char *WP = " \t";
    u_config_t *config;
    int i;
    const char *value;
    char *src, *res, *v = NULL,*pp = NULL;

    dbg_err_if (h == NULL);
    dbg_err_if (dst == NULL);
    dbg_err_if (filename == NULL);

    /* for each dir_alias config item */
    for(i = 0; !u_config_get_subkey_nth(h->config, "dir_alias", i, &config); 
        ++i)
    {
        if((value = u_config_get_value(config)) == NULL)
            continue; /* empty key */

        /* otherwise strtok_r will modify 'value' */
        v = u_strdup(value);
        dbg_err_if(v == NULL);

        src = strtok_r(v, WP, &pp); 
        dbg_err_if(src == NULL);

        if(strncmp(src, filename, strlen(src)) == 0)
        {
            /* dbg("filename %s  src %s", filename, src); */

            /* alias found, get resolved prefix */
            res = strtok_r(NULL, WP, &pp);
            dbg_err_if(res == NULL);

            dbg_err_if(u_path_snprintf(dst, sz, '/', "%s/%s", res, 
                        filename + strlen(src)));

            /* dbg("resolved %s in %s", filename, dst); */

            U_FREE(v); 
            return 0;
        }

        U_FREE(v);
    }

    /* prepend dir_root */
    dbg_err_if(u_path_snprintf(dst, sz, '/', "%s/%s", h->dir_root, filename));

    return 0;
err:
    U_FREE(v);
    return ~0;
}

static int http_is_valid_uri(void *arg, const char *buf, size_t len)
{
    enum { URI_MAX = 2048 };
    char resolved[U_FILENAME_MAX], uri[URI_MAX];
    http_t *h = (http_t*)arg;

    dbg_err_if (arg == NULL);
    dbg_err_if (buf == NULL);
    
    strncpy(uri, buf, len);
    uri[len] = 0;

    dbg_err_if(http_alias_resolv(h, resolved, uri, URI_MAX));

    return broker_is_valid_uri(h->broker, resolved, strlen(resolved));
err:
    return ~0;
}

static void http_resolv_request(http_t *h, request_t *rq)
{
    const char *cstr;
    char resolved[U_FILENAME_MAX];

    dbg_return_if (h == NULL, );
    dbg_return_if (rq == NULL, );
    
    /* unalias rq->filename */
    cstr = request_get_filename(rq);
    if(cstr && !http_alias_resolv(h, resolved, cstr, U_FILENAME_MAX))
        request_set_resolved_filename(rq, resolved);

    /* unalias rq->path_info */
    cstr = request_get_path_info(rq);
    if(cstr && !http_alias_resolv(h, resolved, cstr, U_FILENAME_MAX))
        request_set_resolved_path_info(rq, resolved);
}

static int http_set_index_request(http_t *h, request_t *rq)
{
    static const char *indexes[] = { "/index.klone", "/index.kl1",
        "/index.html", "/index.htm", NULL };
    const char **pg;
    char resolved[U_FILENAME_MAX];

    dbg_err_if (h == NULL);
    dbg_err_if (rq == NULL);

    /* user provided index page list (FIXME add list support) */
    if(h->index == NULL)
    {   
        /* try to find an index page between default index uris */
        for(pg = indexes; *pg; ++pg)
        {
            resolved[0] = 0;  /* for valgrind's happyness */
            dbg_err_if(u_path_snprintf(resolved, U_FILENAME_MAX, '/', "%s/%s", 
                    request_get_resolved_filename(rq), *pg));

            if(broker_is_valid_uri(h->broker, resolved, strlen(resolved)))
            {
                /* a valid index uri has been found; rewrite request */
                request_set_filename(rq, *pg);
                break;
            }
        }
        if(*pg == NULL) /* no index found, set index.html (will return 404 ) */
            dbg_if(request_set_filename(rq, "/index.html"));
    } else
        dbg_if(request_set_filename(rq, h->index));

    http_resolv_request(h, rq);

    return 0;
err:
    return ~0;
}

static int http_add_default_header(http_t *h, response_t *rs)
{
    time_t now;

    dbg_err_if (h == NULL);
    dbg_err_if (rs == NULL);
    
    /* set server signature */
    dbg_err_if(response_set_field(rs, "Server", h->server_sig));

    now = time(NULL);
    dbg_err_if(response_set_date(rs, now));

    return 0;
err:
    return ~0;
}

static int http_do_serve(http_t *h, request_t *rq, response_t *rs)
{
    enum { BUFSZ = 64 };
    const char *err_page;
    char buf[BUFSZ];
    int  status;

    dbg_err_if (h == NULL);
    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    
    /* add default header fields */
    dbg_err_if(http_add_default_header(h, rs));

    /* set default successfull status code */
    response_set_status(rs, HTTP_STATUS_OK); 

    /* serve the page; on error write out a simple error page */
    if(!broker_serve(h->broker, rq, rs))
        return 0; /* page successfully served */

    /* something has gone wrong, if the status code is reasonable
       try to honor it otherwise try to output an internal server error */
    status = response_get_status(rs);
    switch(status)
    {
        case HTTP_STATUS_NO_CONTENT:
            return 0; /* no MIME body needed */
        case HTTP_STATUS_EMPTY:
        case HTTP_STATUS_ACCEPTED:
        case HTTP_STATUS_OK:
        case HTTP_STATUS_CREATED:
        case HTTP_STATUS_INTERNAL_SERVER_ERROR:
            response_set_status(rs, HTTP_STATUS_INTERNAL_SERVER_ERROR); 
            break;
    }

    /* clean dirty header fields */
    dbg_err_if(header_clear(response_get_header(rs)));

    /* add default header fields */
    dbg_err_if(http_add_default_header(h, rs));

    /* looking for user provided error page */
    dbg_err_if(u_snprintf(buf, BUFSZ, "error.%d", status));
    err_page = u_config_get_subkey_value(h->config, buf);

    if(err_page && !request_set_uri(rq, err_page, NULL, NULL))
    {
        http_resolv_request(h, rq);
        if(!broker_serve(h->broker, rq, rs))
            return 0; 
        /* else serve default error page */
    }

    /* refresh the status code */
    status = response_get_status(rs);

    /* print HTTP header */
    dbg_err_if(response_print_header(rs));

    if(request_get_method(rq) == HM_HEAD)
        return 0; /* just the header is requested */

    /* print default error page */
    dbg_err_if(io_printf(response_io(rs), 
        "<html><head><title>%d %s</title></head>\n"
        "<body><h1>%s</h1><p>URL: %s</body></html>", 
        status, http_get_status_desc(status), 
        http_get_status_desc(status), request_get_uri(rq)) < 0);

    return 0;
err:
    return ~0;
}

static int http_cb_close_fd(alarm_t *al, void *arg)
{
    int fd = (int)arg;

    u_unused_args(al);

    warn("connection on fd [%d] timed out, closing", fd);

    /* this will unblock pending I/O calls */
    close(fd);

    return 0;
}

static int http_serve(http_t *h, int fd)
{
    request_t *rq = NULL;
    response_t *rs = NULL;
    io_t *in = NULL, *out = NULL;
    int cgi = 0;
    const char *gwi = NULL;
    alarm_t *al = NULL;
    addr_t *addr;
    struct sockaddr sa;
    int sasz;

    dbg_err_if (h == NULL);
    dbg_err_if (fd < 0);
    
    if(fd == 0 && (gwi = getenv("GATEWAY_INTERFACE")) != NULL)
        cgi++;

    /* create a request object */
    dbg_err_if(request_create(h, &rq));

    /* save local and peer address into request/response objects */
    dbg_err_if(addr_create(&addr));

    /* set local addr */
    sasz = sizeof(struct sockaddr);
    dbg_err_if(getsockname(fd, &sa, &sasz));
    dbg_err_if(addr_set_from_sa(addr, &sa, sasz));
    dbg_err_if(request_set_addr(rq, addr));

    /* set peer addr */
    sasz = sizeof(struct sockaddr);
    dbg_err_if(getpeername(fd, &sa, &sasz));
    dbg_err_if(addr_set_from_sa(addr, &sa, sasz));
    dbg_err_if(request_set_peer_addr(rq, addr));

    /* create a response object */
    dbg_err_if(response_create(h, &rs));

#ifdef HAVE_LIBOPENSSL
    /* create input io buffer (no IO_FD_CLOSE used because 'out' 
       will close it */
    if(h->ssl && !cgi)
        dbg_err_if(io_ssl_create(fd, IO_FD_CLOSE, h->ssl_ctx, &in));
    else
        dbg_err_if(io_fd_create(fd, IO_FD_CLOSE, &in));
#else
    /* create input io buffer */
    dbg_err_if(io_fd_create(fd, IO_FD_CLOSE, &in));
#endif

    /* bind the request object to the 'in' io_t */
    dbg_err_if(request_bind(rq, in));
    in = NULL; 

    /* wait at most N seconds to receive the request */
    dbg_err_if(timerm_add(h->idle_timeout, http_cb_close_fd, (void*)fd, &al));

    if(cgi)
        dbg_err_if(cgi_set_request(rq));
    else
        dbg_err_if(request_parse(rq, http_is_valid_uri, h));

    /* timeout not expired, clear it */
    dbg_if(timerm_del(al));

    /* if we're running in server mode then resolv aliases and dir_root */
    if(!cgi)
        http_resolv_request(h, rq);

    if(strcmp(request_get_filename(rq), "/") == 0)
        dbg_err_if(http_set_index_request(h, rq)); /* set the index page */

    /* request_print(rq); */

    /* create the output io_t */
    if(cgi)
        dbg_err_if(io_fd_create((cgi ? 1 : fd), IO_FD_CLOSE, &out));
    else
        /* create the response io_t dup'ping the request io_t object */
        dbg_err_if(io_dup(request_io(rq), &out));

    response_set_method(rs, request_get_method(rq));

    /* bind the response to the connection c */
    dbg_err_if(response_bind(rs, out));
    out = NULL;

    dbg_err_if(http_do_serve(h, rq, rs));

    request_free(rq);
    response_free(rs); /* must be free'd after the request object because
                          the rsfilter references the response object during
                          the flush of the codec (so the response object must
                          not be free'd) that happens during the io_free call */
    return 0;
err:
    if(in)
        io_free(in);
    if(out)
        io_free(out);
    if(rs)
        response_free(rs);
    if(rq)
        request_free(rq);
    return ~0;
}

static int http_free(http_t *h)
{
    dbg_return_if (h == NULL, 0);   /* it's ok */

    if(h->broker)
        broker_free(h->broker);

    U_FREE(h);

    return 0;
}

static int http_set_config_opt(http_t *http)
{
    u_config_t *c = http->config;
    const char *v;

    dbg_err_if (http == NULL);
    
    /* defaults */
    http->idle_timeout = HTTP_DEFAULT_IDLE_TIMEOUT;
    http->server_sig = "klone/" KLONE_VERSION;
    http->dir_root = "";
    http->index = NULL;
    http->send_enc_deflate = 0; 

    /* idle_timeout */
    if((v = u_config_get_subkey_value(c, "idle_timeout")) != NULL)
        http->idle_timeout = MAX(1, atoi(v));

    /* send_enc_deflate (disable if not configured) */
    dbg_err_if(u_config_get_subkey_value_b(c, "send_enc_deflate", 0, 
        &http->send_enc_deflate));

    /* server signature */
    if((v = u_config_get_subkey_value(c, "server_sig")) != NULL)
        http->server_sig = v;

    /* html dir root */
    if((v = u_config_get_subkey_value(c, "dir_root")) != NULL)
        http->dir_root = v;

    if((v = u_config_get_subkey_value(c, "index")) != NULL)
        http->index = v;

    return 0;
err:
    return ~0;
}


static int http_create(u_config_t *config, http_t **ph)
{
    http_t *h = NULL;

    dbg_err_if (config == NULL);
    dbg_err_if (ph == NULL);

    h = u_zalloc(sizeof(http_t));
    dbg_err_if(h == NULL);

    h->config = config;
    /* init page broker (and page suppliers) */
    dbg_err_if(broker_create(&h->broker));

    /* set http struct config opt reading from http->config */
    dbg_err_if(http_set_config_opt(h));

    *ph = h;

    return 0;
err:
    if(h)
        http_free(h);
    return ~0;
}

int http_backend_serve(struct backend_s *be, int fd)
{
    http_t *h;
    int rc;

    dbg_err_if (be == NULL);
    dbg_err_if (be->arg == NULL)
    dbg_err_if (fd < 0);
    
    h = (http_t *) be->arg;
    
    /* new connection accepted on http listening socket, handle it */
    dbg_if((rc = http_serve(h, fd)) != 0);

    return rc;
err:
    return ~0;
}

int http_backend_term(struct backend_s *be)
{
    http_t *http;

    dbg_return_if (be == NULL, 0);
    dbg_return_if (be->arg == NULL, 0);

    http = (http_t *) be->arg;

    dbg_err_if(session_module_term(http->sess_opt));

    http_free(http);

    return 0;
err:
    return ~0;
}

int http_backend_init(struct backend_s *be)
{
    http_t *http = NULL;
    broker_t *broker = NULL;

    dbg_err_if (be == NULL);
 
    dbg_err_if(http_create(be->config, &http));

    be->arg = http;

    dbg_err_if(session_module_init(http->config, &http->sess_opt));

    return 0;
err:
    if(http)
        http_free(http);
    if(broker)
        broker_free(broker);
    return ~0;
}

#ifdef HAVE_LIBOPENSSL
int https_backend_init(struct backend_s *be)
{
    http_t *https;
    tls_ctx_args_t *cargs;

    dbg_err_if (be == NULL);

    dbg_err_if(http_backend_init(be));

    https = (http_t *) be->arg;

    /* turn on SSL encryption */
    https->ssl = 1;

    /* load config values and set SSL_CTX accordingly */
    dbg_err_if (tls_load_ctx_args(http_get_config(https), &cargs));
    dbg_err_if (!(https->ssl_ctx = tls_init_ctx(cargs)));

    dbg_err_if(session_module_init(https->config, &https->sess_opt));

    return 0;
err:
    return ~0;
}

int https_backend_term(struct backend_s *be)
{
    http_t *https;

    dbg_err_if (be == NULL);

    https = (http_t *) be->arg;
    if (https == NULL)
        return 0;

    dbg_err_if(session_module_term(https->sess_opt));

    SSL_CTX_free(https->ssl_ctx);

    return http_backend_term(be); 
err:
    return ~0;
}

/* same http functions but different '_init' */
backend_t be_https =
    BACKEND_STATIC_INITIALIZER( "https", 
        https_backend_init, 
        http_backend_serve, 
        https_backend_term );
#endif /* HAVE_LIBOPENSSL */

backend_t be_http =
    BACKEND_STATIC_INITIALIZER( "http", 
        http_backend_init, 
        http_backend_serve, 
        http_backend_term );

