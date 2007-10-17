/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: http.c,v 1.48 2007/10/17 22:58:35 tat Exp $
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
#include <klone/os.h>
#include <klone/server.h>
#include <klone/context.h>
#include <klone/broker.h>
#include <klone/request.h>
#include <klone/ses_prv.h>
#include <klone/response.h>
#include <klone/backend.h>
#include <klone/io.h>
#include <klone/timer.h>
#include <klone/tls.h>
#include <klone/ses_prv.h>
#include <klone/hook.h>
#include <klone/hookprv.h>
#include "http_s.h"

struct http_status_map_s
{
    int status;
    const char *desc;
} http_status_map[] = {
    { HTTP_STATUS_OK                    , "OK"                      },
    { HTTP_STATUS_NOT_MODIFIED          , "Not Modified"            },
    { HTTP_STATUS_NOT_FOUND             , "Not Found"               },
    { HTTP_STATUS_INTERNAL_SERVER_ERROR , "Internal Server Error"   },
    { HTTP_STATUS_MOVED_PERMANENTLY     , "Moved Permanently"       },
    { HTTP_STATUS_MOVED_TEMPORARILY     , "Moved Temporarily"       },
    { HTTP_STATUS_CREATED               , "Created"                 },
    { HTTP_STATUS_ACCEPTED              , "Accepted"                },
    { HTTP_STATUS_NO_CONTENT            , "No Content"              },
    { HTTP_STATUS_BAD_REQUEST           , "Bad Request"             },
    { HTTP_STATUS_UNAUTHORIZED          , "Unauthorized"            },
    { HTTP_STATUS_FORBIDDEN             , "Forbidden"               },
    { HTTP_STATUS_LENGTH_REQUIRED       , "Content-Length required" },
    { HTTP_STATUS_REQUEST_TOO_LARGE     , "Request data too big"    },
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

static int http_try_resolv(const char *alias, char *dst, const char *uri, 
        size_t sz)
{
    static const char *WP = " \t";
    char *src, *res, *v = NULL,*pp = NULL;

    dbg_err_if(dst == NULL);
    dbg_err_if(uri == NULL);
    dbg_err_if(alias == NULL);

    /* dup to use it in strtok */
    v = u_strdup(alias);
    dbg_err_if(v == NULL);

    /* src is the source directory */
    src = strtok_r(v, WP, &pp); 
    dbg_err_if(src == NULL);

    /* exit if the URI doesn't match this alias */
    nop_err_if(strncmp(src, uri, strlen(src)));

    /* if src doesn't end with a slash check that the next char in uri is a / */
    if(src[strlen(src)-1] != U_PATH_SEPARATOR)
        nop_err_if(uri[strlen(src)] != U_PATH_SEPARATOR);

    /* alias found, get the resolved prefix */
    res = strtok_r(NULL, WP, &pp);
    dbg_err_if(res == NULL);

    /* copy-out the resolved uri to dst */
    dbg_err_if(u_path_snprintf(dst, sz, '/', "%s/%s", res, uri + strlen(src)));

    U_FREE(v);

    return 0;
err:
    U_FREE(v);
    return ~0;
}

int http_alias_resolv(http_t *h, char *dst, const char *uri, size_t sz)
{
    u_config_t *config, *cgi;
    int i;

    dbg_err_if (h == NULL);
    dbg_err_if (dst == NULL);
    dbg_err_if (uri == NULL);

    /* for each dir_alias config item */
    for(i = 0; !u_config_get_subkey_nth(h->config, "dir_alias", i, &config); 
        ++i)
    {
        if(!http_try_resolv(u_config_get_value(config), dst, uri, sz))
            return 0;   /* alias found, uri resolved */
    }

    /* if there's a cgi tree also try to resolv script_alias rules */
    if(!u_config_get_subkey(h->config, "cgi", &cgi))
    {
        for(i = 0; !u_config_get_subkey_nth(cgi, "script_alias", i, &config);
            ++i)
        {
            if(!http_try_resolv(u_config_get_value(config), dst, uri, sz))
                return 0;   /* alias found, uri resolved */
        }
    }

    /* not alias found, prepend dir_root to the uri */
    dbg_err_if(u_path_snprintf(dst, sz, '/', "%s/%s", h->dir_root, uri));

    return 0;
err:
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

    return broker_is_valid_uri(h->broker, h, resolved, strlen(resolved));
err:
    return ~0;
}

static void http_resolv_request(http_t *h, request_t *rq)
{
    const char *cstr;
    char resolved[U_FILENAME_MAX];

    dbg_ifb(h == NULL) return;
    dbg_ifb(rq == NULL) return;
    
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

            if(broker_is_valid_uri(h->broker, h, resolved, strlen(resolved)))
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

static int http_print_error_page(http_t *h, request_t *rq, response_t *rs, 
    int http_status)
{
    enum { BUFSZ = 64 };
    const char *err_page;
    char buf[BUFSZ];
    int rc;

    dbg_err_if (h == NULL);
    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    dbg_err_if (http_status == 0);
    
    /* clean dirty header fields (not for redirects) */
    if(http_status != 302)
        dbg_err_if(header_clear(response_get_header(rs)));

    /* add default header fields */
    dbg_err_if(http_add_default_header(h, rs));

    /* disable page caching */
    dbg_err_if(response_disable_caching(rs));

    /* looking for user provided error page */
    dbg_err_if(u_snprintf(buf, BUFSZ, "error.%d", http_status));
    err_page = u_config_get_subkey_value(h->config, buf);

    if(err_page && !request_set_uri(rq, err_page, NULL, NULL))
    {
        http_resolv_request(h, rq);
        if((rc = broker_serve(h->broker, h, rq, rs)) == 0)
            return 0; 
        else {
            /* configured error page not found */
            http_status = rc;
        }
    }

    /* be sure that the status code is properly set */
    response_set_status(rs, http_status);

    response_print_header(rs);

    if(request_get_method(rq) == HM_HEAD)
        return 0; /* just the header is requested */

    /* print default error page */
    dbg_err_if(io_printf(response_io(rs), 
        "<html><head><title>%d %s</title></head>\n"
        "<body><h1>%s</h1><p>URL: %s</body></html>", 
        http_status, http_get_status_desc(http_status), 
        http_get_status_desc(http_status), 
        (request_get_uri(rq) ? request_get_uri(rq) : "") ) < 0);

    return 0;
err:
    return ~0;
}

static int http_serve(http_t *h, int fd)
{
    request_t *rq = NULL;
    response_t *rs = NULL;
    io_t *in = NULL, *out = NULL;
    int cgi = 0, port;
    const char *gwi = NULL;
    talarm_t *al = NULL;
    addr_t *addr;
    struct sockaddr sa;
    int sasz, rc = HTTP_STATUS_INTERNAL_SERVER_ERROR;

    u_unused_args(al);

    dbg_err_if (h == NULL);
    dbg_err_if (fd < 0);
    
    if(fd == 0 && (gwi = getenv("GATEWAY_INTERFACE")) != NULL)
        cgi++; /* klone is being used as a CGI */

    /* create a request object */
    dbg_err_if(request_create(h, &rq));
    request_set_cgi(rq, cgi);

    /* save local and peer address into the request object */
    dbg_err_if(addr_create(&addr));

    if(cgi)
    {
        if(getenv("REMOTE_ADDR") && getenv("REMOTE_PORT"))
        {
            port = atoi(getenv("REMOTE_PORT"));
            dbg_err_if(addr_set(addr, getenv("REMOTE_ADDR"), port));
            dbg_err_if(request_set_addr(rq, addr));
        }

        if(getenv("SERVER_ADDR"))
        {
            if(getenv("SERVER_PORT"))
                port = atoi(getenv("SERVER_PORT"));
            else
                port = 80;
            dbg_err_if(addr_set(addr, getenv("SERVER_ADDR"), port));
            dbg_err_if(request_set_peer_addr(rq, addr));
        }
    } else {
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
    }

    addr_free(addr);
    addr = NULL;

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

    /* create a response object */
    dbg_err_if(response_create(h, &rs));

    response_set_cgi(rs, cgi);

    if(cgi)
        dbg_err_if(cgi_set_request(rq));

    /* create the output io_t */
    if(cgi)
        dbg_err_if(io_fd_create((cgi ? 1 : fd), IO_FD_CLOSE, &out));
    else
        /* create the response io_t dup'ping the request io_t object */
        dbg_err_if(io_dup(request_io(rq), &out));

    /* default method used if we cannot parse the request (bad request) */
    response_set_method(rs, HM_GET);

    /* bind the response to the connection c */
    dbg_err_if(response_bind(rs, out));
    out = NULL;

    dbg_err_if(response_set_status(rs, HTTP_STATUS_BAD_REQUEST));

    /* parse request. may fail on timeout */
    dbg_err_if(rc = request_parse_header(rq, http_is_valid_uri, h));

    response_set_method(rs, request_get_method(rq));

    /* if we're running in server mode then resolv aliases and dir_root */
    http_resolv_request(h, rq);

    /* if / is requested then return one of index.{klone,kl1,html,htm} */
    if(strcmp(request_get_filename(rq), "/") == 0)
        dbg_err_if(http_set_index_request(h, rq)); /* set the index page */

    /* add default header fields */
    dbg_err_if(http_add_default_header(h, rs));

    /* set default successfull status code */
    dbg_err_if(response_set_status(rs, HTTP_STATUS_OK));

    /* serve the page; on error write out a simple error page */
    dbg_err_if(rc = broker_serve(h->broker, h, rq, rs));

    /* call the hook that fires on each request */
    hook_call(request, rq, rs);

    /* page successfully served */

    request_free(rq);
    response_free(rs); /* must be free'd after the request object because
                          the rsfilter references the response object during
                          the flush of the codec (so the response object must
                          not be free'd) that happens during the io_free call */
    return 0;
err:
    /* hook get fired also on error */
    hook_call(request, rq, rs);

    if(rc && rq && rs && response_io(rs))
        http_print_error_page(h, rq, rs, rc); /* print the error page */
    if(in)
        io_free(in);
    if(out)
        io_free(out);
    if(rq)
        request_free(rq);
    if(rs)
        response_free(rs);
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
    http->server_sig = "klone/" KLONE_VERSION;
    http->dir_root = "";
    http->index = NULL;
    http->send_enc_deflate = 0; 

    /* send_enc_deflate (disable if not configured) */
    dbg_err_if(u_config_get_subkey_value_b(c, "send_enc_deflate", 0, 
        &http->send_enc_deflate));

    /* server signature */
    if((v = u_config_get_subkey_value(c, "server_sig")) != NULL)
        http->server_sig = v;

    /* html dir root */
    if((v = u_config_get_subkey_value(c, "dir_root")) != NULL)
        http->dir_root = v;
    else
        crit_err("dir_root must be set");

    /* index page */
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

static int http_backend_serve(struct backend_s *be, int fd)
{
    http_t *h;
    int rc;

    dbg_err_if (be == NULL);
    dbg_err_if (be->arg == NULL);
    dbg_err_if (fd < 0);
    
    h = (http_t *) be->arg;
    
    /* new connection accepted on http listening socket, handle it */
    dbg_if((rc = http_serve(h, fd)) != 0);

    return rc;
err:
    return ~0;
}

static int http_backend_term(struct backend_s *be)
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

static int http_backend_init(struct backend_s *be)
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
static int https_backend_init(struct backend_s *be)
{
    http_t *https;

    dbg_err_if (be == NULL);

    dbg_err_if(http_backend_init(be));

    https = (http_t *) be->arg;

    /* turn on SSL encryption */
    https->ssl = 1;

    /* load config values and set SSL_CTX accordingly */
    https->ssl_ctx = tls_load_init_ctx(http_get_config(https));
    warn_err_ifm (https->ssl_ctx == NULL, "bad or missing HTTPS credentials");

    dbg_err_if(session_module_init(https->config, &https->sess_opt));

    return 0;
err:
    return ~0;
}

static int https_backend_term(struct backend_s *be)
{
    http_t *https;

    dbg_err_if (be == NULL);

    https = (http_t *) be->arg;
    if (https == NULL)
        return 0;

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

