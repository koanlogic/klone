/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: request.c,v 1.66 2009/05/29 10:26:00 tho Exp $
 */

#include "klone_conf.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <u/libu.h>
#include <klone/request.h>
#include <klone/utils.h>
#include <klone/io.h>
#include <klone/ioprv.h>
#include <klone/http.h>
#include <klone/vars.h>
#include <klone/timer.h>
#include <klone/vhost.h>
#include <klone/supplier.h>

struct request_s
{
    http_t *http;               /* http server handle                       */
    header_t *header;           /* input header                             */
    io_t *io;                   /* input io stream                          */
    int method;                 /* get,post,etc.                            */
    char *cli_rq;               /* verbatim client request line             */
    char *uri;                  /* verbatim uri asked by the client         */
    char *protocol;             /* proto/ver                                */
    char *path_info;            /* extra info at the end of the path        */
    char *query;                /* query string (data after '?')            */
    char *filename;             /* path of the req resource                 */
    char *resolved_path_info;   /* resolved path_info                       */
    char *resolved_filename;    /* unaliased filename                       */
    vars_t *args;               /* mixed get/post args                      */
    vars_t *args_get;           /* get variables                            */
    vars_t *args_post;          /* post variables                           */
    vars_t *cookies;            /* cookies                                  */
    vars_t *uploads;            /* uploaded file list                       */
    char *content_type;         /* type/subtype                             */
    char *content_encoding;     /* 7bit/8bit/base64/qp, etc                 */
	size_t content_length;      /* content-length http header field         */
    time_t if_modified_since;   /* time_t IMS header                        */
    char local_addr[256];       /* local address                            */
    char peer_addr[256];        /* peer address                             */
    int cgi;                    /* if running in cgi mode                   */
    size_t idle_timeout;        /* max # of secs to wait for the request    */
    size_t post_timeout;        /* max # of secs for reading POSTed data    */
    size_t post_maxsize;        /* max # of POSTed bytes to accepts         */
    const char *temp_dir;       /* where temp files go                      */
    vhost_t *vhost;             /* cached vhost pointer                     */
    size_t padding;
    /* cached supplier info data */
    supplier_t *si_sup;
    void *si_handle;
    time_t si_mtime;
    size_t body_off;
};

typedef struct upload_info_s    /* uploaded file info struct         */
{
    char mime_type[MIME_TYPE_BUFSZ];
    char filename[U_FILENAME_MAX];
    size_t size;
} upload_info_t;

enum { 
    REQUEST_DEFAULT_IDLE_TIMEOUT = 10,         /* 10 secs */
    REQUEST_DEFAULT_POST_TIMEOUT = 600,        /* 10 mins */
    REQUEST_DEFAULT_POST_MAXSIZE = 5*1024000,  /* 5 MB    */
};


#define REQUEST_SET_STRING_FIELD(lval, rval)        \
    do {                                            \
        U_FREE(lval);                               \
        if(rval)                                    \
        {                                           \
            lval = u_strdup(rval);                  \
            dbg_err_if(lval == NULL);               \
        }                                           \
    } while(0)


int request_is_encoding_accepted(request_t *rq, const char *encoding)
{
    char *pp, *tok, *src, *buf = NULL;
    const char *accept_encoding;
    int rc = 0;

    dbg_err_if (rq == NULL);
    dbg_err_if (encoding == NULL);
    
    accept_encoding = header_get_field_value(rq->header, "Accept-Encoding");
    if(accept_encoding)
    {
        /* get a copy to work on */
        buf = u_strdup(accept_encoding);
        dbg_err_if(buf == NULL);

        /* foreach encoding pair... */
        for(src = buf; (tok = strtok_r(src, " ,", &pp)) != NULL; src = NULL)
        {
            if(strcasecmp(tok, encoding) == 0)
            {
                rc++; /* found */
                break;
            }
        }

        U_FREE(buf);
    }

    return rc;
err:
    U_FREE(buf);
    return 0;
}

/**
 * \ingroup request
 * \brief   Get the \c io_t object associated with a request object
 *
 * Return the I/O object (\c io_t) used by the request object passed as 
 * parameter.  The \c io_t object is bound to the socket connected to the
 * client (the web browser).
 *
 * \param rq    request object
 *
 * \return child \c io_t object of the given \p rq or NULL if no \c io_t 
 *         object has been set
 *
 * \sa io_t
 */
io_t *request_io(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->io;
}

/**
 * \ingroup request
 * \brief   Get the cookies list
 *
 * Return a \c vars_t object containing the list of all cookies sent by the
 * browser.
 *
 * \param rq  request object
 *
 * \return the cookie list of the given \p rq
 */
vars_t *request_get_cookies(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->cookies;
}

/**
 * \ingroup request
 * \brief   Get the value of a cookie named \p name
 *
 * Return the value of a cookie sent by the browser
 *
 * \param rq    request object
 * \param name  cookie name
 *
 * \return the cookie value or \c NULL on error
 */
const char *request_get_cookie(request_t *rq, const char *name)
{
    var_t *v;

    dbg_return_if (rq == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    v = vars_get(rq->cookies, name);

    return v ? var_get_value(v): NULL;
}

/**
 * \ingroup request
 * \brief   Get the list of request variables (using either GET or POST method)
 *
 * Return get/post arguments of request \p rq in a \c vars_t object
 *
 * \param rq  request object
 *
 * \return the arguments' list of the given \p rq
 */
vars_t *request_get_args(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->args;
}

/**
 * \ingroup request
 * \brief   Get request variables passed using the GET method
 *
 * Return GET arguments of request \p rq in a \c vars_t object
 *
 * \param rq  request object
 *
 * \return the arguments' list of the given \p rq
 */
vars_t *request_get_getargs(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->args_get;
}

/**
 * \ingroup request
 * \brief   Get request variables passed using the POST method
 *
 * Return POST arguments of request \p rq in a \c vars_t object
 *
 * \param rq  request object
 *
 * \return the arguments' list of the given \p rq
 */
vars_t *request_get_postargs(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->args_post;
}

/**
 * \ingroup request
 * \brief   Get a request variable (using either GET or POST variable list)
 *
 * Return the string value of argument \p name in request \p rq.
 *
 * \param rq    request object
 * \param name  name of the argument
 *
 * \return
 *  - the string value corresponding to the supplied \p name 
 *  - \c NULL if there's no argument named \p name
 */
const char *request_get_arg(request_t *rq, const char *name)
{
    var_t *v;

    dbg_return_if (rq == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    v = vars_get(rq->args, name);

    return v ? var_get_value(v): NULL;
}

/**
 * \ingroup request
 * \brief   Return a GET variable
 *
 * Return the string value of argument \p name in request \p rq.
 *
 * \param rq    request object
 * \param name  name of the argument
 *
 * \return
 *  - the string value corresponding to the supplied \p name 
 *  - \c NULL if there's no argument named \p name
 */
const char *request_get_getarg(request_t *rq, const char *name)
{
    var_t *v;

    dbg_return_if (rq == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    v = vars_get(rq->args_get, name);

    return v ? var_get_value(v): NULL;
}

/**
 * \ingroup request
 * \brief   Return a POST variable
 *
 * Return the string value of argument \p name in request \p rq.
 *
 * \param rq    request object
 * \param name  name of the argument
 *
 * \return
 *  - the string value corresponding to the supplied \p name 
 *  - \c NULL if there's no argument named \p name
 */
const char *request_get_postarg(request_t *rq, const char *name)
{
    var_t *v;

    dbg_return_if (rq == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    v = vars_get(rq->args_post, name);

    return v ? var_get_value(v): NULL;
}

int request_set_field(request_t *rq, const char *name, const char *value)
{
    dbg_return_if (rq == NULL, ~0);
    dbg_return_if (name == NULL, ~0);
    dbg_return_if (value == NULL, ~0);

    return header_set_field(rq->header, name, value);
}

/** 
 * \ingroup request
 * \brief   Get the URI field of a request
 *  
 * Return the string value of the URI in request \p rq.
 *
 * \param rq    request object
 *  
 * \return the URI child object of the given \p rq
 */
const char *request_get_uri(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->uri;
}

/** 
 * \ingroup request
 * \brief   Get the filename field of a request
 *  
 * Return the string value of the filename field in request \p rq.
 *
 * \param rq    request object
 *  
 * \return the file name bound to \p rq (can be \c NULL) 
 */
const char *request_get_filename(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->filename;
}

/*
 * \ingroup request
 * \brief   Set the filename field of a request
 *
 * Set the filename field of request \p rq to \p filename.
 *
 * \param rq        request object
 * \param filename  filename string
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_set_filename(request_t *rq, const char *filename)
{
    dbg_err_if (rq == NULL);
    dbg_err_if (filename == NULL);
    
    REQUEST_SET_STRING_FIELD(rq->filename, filename);

    return 0;
err:
    return ~0;
}

/** 
 * \ingroup request
 * \brief   Get the query string field of a request
 *  
 * Return the query string field of request \p rq.
 *
 * \param rq    request object
 *  
 * \return the query string bound to \p rq (may be \c NULL)
 */
const char *request_get_query_string(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->query;
}

/** 
 * \ingroup request
 * \brief   Get the path info field of a request
 *  
 * Return the path info field of request \p rq.
 *
 * \param rq    request object
 *  
 * \return the path info of \p rq (may be \c NULL)
 */
const char *request_get_path_info(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->path_info;
}

/* parse and set if-modified-since value */
static int request_parse_ims(request_t *rq)
{
    const char *ims;

    dbg_err_if (rq == NULL);
    
    rq->if_modified_since = 0;

    ims = header_get_field_value(rq->header, "If-Modified-Since");
    if(ims)
        dbg_err_if(u_httpdate_to_tt(ims, &rq->if_modified_since));

err: /* ignore if it's not formatted properly */
    return 0;
}

/**
 * \ingroup request
 * \brief   Get IMS field of a request
 *
 * Return the \c time_t value of the IMS field of request \p rq
 *
 * \param rq      request object
 *
 * \return a valid \c time_t value on success, \c -1 on failure
 */
time_t request_get_if_modified_since(request_t *rq)
{
    dbg_return_if (rq == NULL, (time_t) -1);

    return rq->if_modified_since;
}

/*
 * \ingroup request
 * \brief   Set the resolved filename field of a request
 *
 * Set the resolved filename field of request \p rq to \p resolved_fn
 *
 * \param rq          request object
 * \param resolved_fn resolved filename
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_set_resolved_filename(request_t *rq, const char *resolved_fn)
{
    dbg_err_if (rq == NULL);
    dbg_err_if (resolved_fn == NULL);

    REQUEST_SET_STRING_FIELD(rq->resolved_filename, resolved_fn);

    return 0;
err:
    return ~0;
}

/** 
 * \ingroup request
 * \brief   Get the HTTP server handle of a request
 *  
 * Get the \c http_t object containing the HTTP server handle of request \p rq
 *
 * \param rq    request object
 *  
 * \return the child HTTP object of the given \p rq (may be \c NULL)
 */
http_t* request_get_http(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->http;
}

/*
 * \ingroup request
 * \brief   Bind request I/O to a given I/O. 
 *  
 * Bind the I/O of request \p rq to \p in.
 *
 * \param rq    request object
 * \param in    input I/O object
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_bind(request_t *rq, io_t *in)
{
    dbg_return_if (rq == NULL, ~0);
    dbg_return_if (in == NULL, ~0);

    rq->io = in;

    return 0;
}

/*
 * \ingroup request
 * \brief   Set the query string of a request
 *
 * Parse \p query string and build the \p rq->args list.
 *
 * \param rq     request object
 * \param query  query string 
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_set_query_string(request_t *rq, const char *query)
{
    dbg_err_if (rq == NULL);
    dbg_err_if (query == NULL);
    
    REQUEST_SET_STRING_FIELD(rq->query, query);

    return 0;
err:
    return ~0;
}

void request_clear_uri(request_t *rq)
{
    U_FREE(rq->uri);
    U_FREE(rq->protocol);
    U_FREE(rq->path_info);
    U_FREE(rq->query);
    U_FREE(rq->filename);
    U_FREE(rq->resolved_path_info);
    U_FREE(rq->resolved_filename);
    U_FREE(rq->content_type);
    U_FREE(rq->content_encoding);
}

/*
 * \ingroup request
 * \brief   Set the path info field of a request
 *
 * Set the path info field of request \p rq to \p path_info.
 *
 * \param rq         request object
 * \param path_info  path info
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_set_path_info(request_t *rq, const char *path_info)
{
    dbg_err_if (rq == NULL);
    dbg_err_if (path_info == NULL);

    REQUEST_SET_STRING_FIELD(rq->path_info, path_info);

    return 0;
err:
    return ~0;
}

/*
 * \ingroup request
 * \brief   Set the resolved path info field of a request
 *
 * Set the resolved path info field of request \p rq to \p resolved_pi.
 *
 * \param rq           request object
 * \param resolved_pi  resolved path info
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_set_resolved_path_info(request_t *rq, const char *resolved_pi)
{
    dbg_err_if (rq == NULL);
    dbg_err_if (resolved_pi == NULL);

    REQUEST_SET_STRING_FIELD(rq->resolved_path_info, resolved_pi);

    return 0;
err:
    return ~0;
}

/*
 * \ingroup request
 * \brief   Set the URI field of a request
 *
 * Set the URI field of request \p rq to \p uri given 
 *
 * \param rq           request object
 * \param uri          URI string
 * \param is_valid_uri URI validation function 
 * \param arg          argument to is_valid_uri
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_set_uri(request_t *rq, const char *uri,
        int (*is_valid_uri)(void*, const char *, size_t),
        void* arg)
{
    char *p, *fn, *pi;
    size_t uri_len = strlen(uri);
    char cp[4096];

    dbg_err_if (rq == NULL);
    dbg_err_if (uri == NULL);
    /* is_valid_uri may be NULL */
    /* arg may be NULL */
 
    request_clear_uri(rq);

    /* this is just to avoid recursive infinite redirect loops for pages that 
       appends something to the URI and redirects to the same page */
    warn_err_ifm(uri_len >= sizeof(cp), "Request URI too long");

    REQUEST_SET_STRING_FIELD(rq->uri, uri);

    /* save (undecoded) query string i.e. everything after '?' */
    if((p = strchr(uri, '?')) != NULL)
        dbg_err_if(request_set_query_string(rq, ++p));

    /* copy decoded url */
    dbg_err_if(u_urlncpy(cp, rq->uri, uri_len, URLCPY_DECODE) <= 0);

    if((p = strchr(cp, '?')) != NULL)
        *p++ = 0; /* remove query string from the uri copy */

    /* normalize the URI (remove /../, /./, ecc) */
    dbg_err_if(u_uri_normalize(cp));

    /* set filename is case there's not path_info and/or file does not exists */
    dbg_err_if(request_set_filename(rq, cp));

    /* look for path_info */
    fn = cp;                    /* filename     */
    pi = fn + strlen(fn);       /* path_info    */
    for(;;)
    {
        if(is_valid_uri == NULL || is_valid_uri(arg, fn, pi - fn))
        {
            dbg_err_if(request_set_filename(rq, fn));
            rq->filename[pi-fn] = 0; /* trunc */
            if(strlen(pi))
                dbg_err_if(request_set_path_info(rq, pi));
            break;
        } else {
            if((p = u_strnrchr(fn, '/', pi - fn)) == NULL)
                break; /* file pointed by this uri does not exists */
            pi = p; /* try again */
        }
    }

    return 0;
err:
    return ~0;
}

static int request_set_proto(request_t *rq, const char *proto)
{
    dbg_err_if (rq == NULL);
    dbg_err_if (proto == NULL);

    /* be sure that the requested protocol is http */
    if(strncasecmp(proto, "http", 4))
        return ~0; /* unknown or unsupported protocol */

    REQUEST_SET_STRING_FIELD(rq->protocol, proto);

    return 0;
err:
    return ~0;
}

/*
 * \ingroup request
 * \brief   Save client request
 *
 * Save client request line
 *
 * \param rq     request object
 * \param ln     the request line
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_set_client_request(request_t *rq, const char *ln)
{
    char *p;
    dbg_err_if(rq == NULL);
    dbg_err_if(ln == NULL);

    rq->cli_rq = u_strdup(ln);
    dbg_err_if(rq->cli_rq == NULL);

    /* cut the trailing newline if any */
    for(p = rq->cli_rq; *p && (*p != '\r' && *p != '\n'); ++p)
        continue;
    *p = 0;

    return 0;
err:
    return ~0;
}

/**
 * \ingroup request
 * \brief   Return the client request line
 *
 * Return the client request (METHOD URI HTTP/HTTP_VERSION)
 *
 * \param rq     request object
 *
 * \return \c 0 if successful, non-zero on error
 */
const char *request_get_client_request(request_t *rq)
{
    return rq->cli_rq;
}

/*
 * Set the \p method of request \p rq.  Refer to http.h for possible methods.
 *
 * \param rq     request object
 * \param method the HTTP method 
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_set_method(request_t *rq, const char *method)
{
    dbg_return_if (rq == NULL, ~0);
    dbg_return_if (method == NULL, ~0);

    if(!strcasecmp(method, "get"))
        rq->method = HM_GET;
    else if(!strcasecmp(method, "head"))
        rq->method = HM_HEAD;
    else if(!strcasecmp(method, "post"))
        rq->method = HM_POST;
    else if(!strcasecmp(method, "put"))
        rq->method = HM_PUT;
    else if(!strcasecmp(method, "delete"))
        rq->method = HM_DELETE;
    else {
        /* put, delete, * */
        rq->method = HM_UNKNOWN;
        return ~0; /* unknown or unsupported method */
    }
    
    return 0;
}

static int request_set_content_length(request_t *rq)
{
    const char *clen;
    size_t len;

    dbg_err_if (rq == NULL);

    clen = header_get_field_value(rq->header, "Content-Length");
    dbg_err_if(clen == NULL || (len = atoi(clen)) < 0);

    rq->content_length = len;

    return 0;
err:
    return ~0;
}

static int request_parse_cookie(request_t *rq, field_t *field)
{
    enum { BUFSZ = 4096 }; /* cookie size limit */
    char *pp, *tok, *src, buf[BUFSZ];

    dbg_err_if (rq == NULL);
    dbg_err_if (field == NULL);
    
    dbg_err_if(field_get_value(field) == NULL);

    /* save a copy to tokenize it */
    u_strlcpy(buf, field_get_value(field), sizeof buf);

    /* foreach name=value pair... */
    for(src = buf; (tok = strtok_r(src, " ;", &pp)) != NULL; src = NULL)
        dbg_if(vars_add_urlvar(rq->cookies, tok, NULL));

    return 0;
err:
    return ~0;
}

static int request_parse_cookies(request_t *rq)
{
    field_t *f;
    size_t i, count;

    dbg_err_if (rq == NULL);
    
    count = header_field_count(rq->header);
    for(i = 0; i < count; ++i)
    {
        f = header_get_fieldn(rq->header, i);
        dbg_err_if(f == NULL); /* shouldn't happen */
        if(strcasecmp(field_get_name(f), "cookie") == 0)
            dbg_err_if(request_parse_cookie(rq, f));
    }

    return 0;
err:
    return ~0;
}

static int request_cb_add_var(vars_t *args, vars_t *also_to, const char *tok)
{
    var_t *v = NULL;

    dbg_err_if(args == NULL);
    dbg_err_if(also_to == NULL);
    dbg_err_if(tok == NULL);

    /* may fail with bad var encoding (url?a=b&v); bad vars are ignored */
    dbg_if(vars_add_urlvar(args, tok, &v));

    if(v)
    {
        /* add the ptr also to the other vars list */
        dbg_err_if(vars_add(also_to, v));
    }

    return 0;
err:
    return ~0;
}


static int request_cb_add_post_var(void *arg, const char *tok)
{
    request_t *rq = (request_t*)arg;

    return request_cb_add_var(rq->args, rq->args_post, tok);
}

static int request_cb_add_get_var(void *arg, const char *tok)
{
    request_t *rq = (request_t*)arg;

    return request_cb_add_var(rq->args, rq->args_get, tok);
}

static int foreach_query_var(const char *urlquery, int offset, 
        int(*cb)(void*,const char*), void *arg)
{
    char *pp, *tok, *src, *query = NULL;

    dbg_err_if(offset < 0);
    dbg_err_if(cb == NULL);

    if(!urlquery)
        return 0; /* no args */

    /* dup to tokenize it */
    query = u_strdup(urlquery + offset);
    dbg_err_if(query == NULL);

    /* foreach name=value pair... */
    for(src = query; (tok = strtok_r(src, "&", &pp)) != NULL; src = NULL)
    {
        /* call the callback that will save this var */
        dbg_err_if(cb(arg, tok));
    }

    U_FREE(query);

    return 0;
err:
    U_FREE(query);
    return ~0;
}

static int request_parse_query_args(request_t *rq)
{
    dbg_err_if(rq == NULL);

    return foreach_query_var(rq->query, 0, request_cb_add_get_var, (void*)rq); 
err:
    return ~0;
}

/* set is-cgi flag */
void request_set_cgi(request_t *rq, int cgi)
{
    rq->cgi = cgi;
    return;
}

/** 
 * \ingroup request
 * \brief   Get the content length of a request
 *  
 * Retrieve a size_t corresponding to the \e Content-Length field of request 
 * \p rq
 *
 * \param rq    request object
 *  
 * \return the content length of the given \p rq
 */
ssize_t request_get_content_length(request_t *rq)
{
    dbg_return_if (rq == NULL, -1);

    return (ssize_t) rq->content_length;
}

static int match_content_type(header_t *h, const char *mime_type)
{
    const char *ct;

    dbg_return_if (h == NULL, 0);
    dbg_return_if (mime_type == NULL, 0);

    ct = header_get_field_value(h, "Content-Type");
    if(ct == NULL || strncasecmp(ct, mime_type, strlen(mime_type)))
        return 0;

    return 1;
}

static int request_is_content_type(request_t *rq, const char *ct)
{
    return match_content_type(rq->header, ct);
}

static int request_is_multipart_formdata(request_t *rq)
{
    return request_is_content_type(rq, "multipart/form-data");
}

static int request_is_urlencoded(request_t *rq)
{
    if(header_get_field_value(rq->header, "Content-Type") == NULL)
        return 1; /* yes; no content-type field provided */

    if(request_is_content_type(rq, "application/x-www-form-urlencoded"))
        return 1; /* yes */

    return 0; /* no */
}

static int request_parse_urlencoded_data(request_t *rq)
{
    ssize_t qsz, len;

    dbg_err_if (rq == NULL);

    len = rq->content_length; /* shortcut */

    qsz = (rq->query ? strlen(rq->query) : 0);

    /* alloc or enlarge the query string buffer */
    rq->query = u_realloc(rq->query, len + qsz + 2);
    dbg_err_if(rq->query == NULL);

    /* u_dbg("rq->query %x  size %u", rq->query, len+qsz+2); */

    rq->query[qsz] = 0; /* must be zero-term for strcat to work */
    if(qsz)
    {   /* append a '&' */
        strcat(rq->query, "&");
        ++qsz;
    }

    /* append to current query string */
    dbg_err_if(io_read(rq->io, rq->query + qsz, len) != len);

    /* zero terminate it */
    rq->query[qsz + len] = 0;

    /* parse and add post vars to the rq->args and rq->args_post array */
    dbg_err_if(foreach_query_var(rq->query, qsz, 
                request_cb_add_post_var, (void*)rq));

    return 0;
err:
    return ~0;
}

/* return the value of the param named 'param_name' of the field 'field_name'
   and save it to 'buffer' */
static int request_get_fieldparam(request_t *rq, const char *field_name, 
    const char *param_name, char *buf, size_t size)
{
    const char *param_value, *field_value, *p;
    size_t pv_len;

    dbg_err_if (rq == NULL);
    dbg_err_if (field_name == NULL);
    dbg_err_if (param_name == NULL);
    dbg_err_if (buf == NULL);
    dbg_err_if (size == 0);

    field_value = header_get_field_value(rq->header, field_name);
    dbg_err_if(field_value == NULL);

    /* look for param name=value pair */
    param_value = u_stristr(field_value, param_name);
    dbg_err_if(param_value == NULL);

    /* skip param name */
    param_value += strlen(param_name);

    /* first char must be an equal sign */
    dbg_err_if(*param_value++ != '=');

    /* a param value ends on the first ';', space or at the end of string */
    for(p = param_value; ; ++p)
        if(*p == '\0' || *p == ';' || isspace(*p))
            break; /* end of param value */

    /* param value len */
    pv_len = p - param_value;

    /* boundary check */
    dbg_err_if(pv_len + 1 > size); 

    /* copy out the param value */
    (void) u_strlcpy(buf, param_value, pv_len + 1);

    return 0;
err:
    return ~0;
}

static int is_multipart_mixed(header_t *h)
{
    return match_content_type(h, "multipart/mixed");
}

static int is_encoded(header_t *h)
{
    const char *cte;

    dbg_return_if (h == NULL, 0);

    if((cte = header_get_field_value(h, "Content-Transfer-Encoding")) == NULL)
        return 0; /* not encoded */

    if(strcasecmp(cte, "binary") == 0)
        return 0; /* not encoded */

    return 1; /* encoded */
}

static inline int is_nl(char c)
{
    return (c == '\n' || c == '\r' ? c : 0);
}

static inline int is_quote(char c)
{
    return (c == '"' || c == '\'' ? c : 0);
}

static int parse_content_disposition(header_t *h, char *name, char *filename,
    size_t prmsz)
{
    enum { BUFSZ = 512 };
    char *pp, *tok, *src, buf[BUFSZ];
    size_t n_len, fn_len;
    const char *cd;
    int q;

    dbg_err_if (h == NULL);
    dbg_err_if (name == NULL);
    dbg_err_if (filename == NULL);
    dbg_err_if (prmsz == 0);
    
    cd = header_get_field_value(h, "Content-Disposition");
    dbg_err_if(cd == NULL);

    dbg_err_if(strlen(cd) >= BUFSZ);

    /* must start with form-data */
    dbg_err_if(strncmp(cd, "form-data", strlen("form-data")));

    name[0] = filename[0] = '\0';

    /* save a copy to tokenize it */
    u_strlcpy(buf, cd, sizeof buf);

    /* shortcut */
    n_len = strlen("name=");
    fn_len = strlen("filename=");

    /* foreach name=value pair... */
    for(src = buf; (tok = strtok_r(src, ";", &pp)) != NULL; src = NULL)
    {
        /* skip trailing blanks */
        while(isspace(*tok))
            ++tok;

        if(strncmp(tok, "form-data", strlen("form-data")) == 0)
        {
            continue;   /* skip */
        }
        else if(strncmp(tok, "name=", n_len) == 0) 
        {
            /* skip the name part */
            tok += n_len;

            /* remove single or double quotes */
            if((q = is_quote(tok[0])) != 0)
                ++tok;
            if(strlen(tok) && tok[strlen(tok) - 1] == q)
                tok[strlen(tok) - 1] = '\0';

            dbg_if (u_strlcpy(name, tok, prmsz));
        } 
        else if(strncmp(tok, "filename=", fn_len) == 0) 
        {
            /* skip the filename part */
            tok += fn_len;

            /* remove single or double quotes */
            if((q = is_quote(tok[0])) != 0)
                ++tok;
            if(strlen(tok) && tok[strlen(tok) - 1] == q)
                tok[strlen(tok) - 1] = '\0';

            dbg_if (u_strlcpy(filename, tok, prmsz));
        } 
        /* else ignore unknown fields */
    }
            
    return 0;
err:
    return ~0;
}

/* 
 * Read from io until obuf is full or until stop_at string is found.
 *
 * Returns the number of bytes written to obuf 
 */
static ssize_t read_until(io_t *io, const char *stop_at, char *obuf, 
    size_t size, int *found)
{
    char *dst = obuf;
    const char *ptr = stop_at, *end = stop_at + strlen(stop_at);
    int rc;
    size_t slen = strlen(stop_at);

    dbg_err_if(size < slen); /* stop_at can't be bigger then the output buf */

    for(*found = 0; *found == 0; )
    {
        /* at all times the output buffer must be big enough to contain the 
         * boundary string so everytime we meet the first char of the boundary 
         * we check the avail size of the output buffer and return if the 
         * whole boundary doesn't fit into it (read_until will be called 
         * again with an empty output buffer) */
        if(ptr == stop_at && size < slen + 1)
            break;

        rc = io_read(io, dst, 1);
        dbg_err_if(rc < 0);

        if(rc == 0)
            break; /* eof */

        if(*ptr != *dst)
            ptr = stop_at;

        if(*ptr == *dst && ++ptr == end)
            *found = 1;

        dst++;
        size--;
    }

    return dst - obuf;
err:
    return -1;
}


/**
 * \ingroup request
 * \brief   Get uploaded files
 *
 * Return the list of uploaded files. 
 *
 * Any var_t in the list will contain, 
 * within its name/value pair, the name of the HTML form input tag "name" 
 * argument and the filename (with full path) of the temporary file where
 * uploaded content has been stored.
 *
 * This function is only useful to enumerate uploads, 
 * \sa request_get_uploaded_file is what you'll probably use.
 *
 * \param rq  request object
 *
 * \return the arguments' list of the given \p rq
 */
vars_t *request_get_uploads(request_t *rq)
{
    return rq->uploads;
}

/*
 * name:         form "name" <input> tag attribute value
 * filename:     name of the uploaded file provided by the client
 * tmp_filename: name on the temp file when the uploaded data has been saved
 *               on the local disk
 * mime_type:    uploaded MIME type as stated by the browser (may be NULL)
 */
static int request_add_uploaded_file(request_t *rq, const char *name, 
    const char *filename, const char *tmp_filename, const char *mime_type)
{
    struct stat st;
    var_t *v = NULL;
    upload_info_t *info = NULL;

    dbg_err_if (rq == NULL);
    dbg_err_if (name == NULL);
    /* filename may be NULL */
    dbg_err_if (tmp_filename == NULL);
    /* mime_type may be NULL */

    dbg_err_sif (stat(tmp_filename, &st) < 0);

    /* create a new var obj */
    dbg_err_if(var_create(name, tmp_filename, &v));

    /* alloc an info struct to attach to the var_t obj */
    dbg_err_if((info = u_zalloc(sizeof(upload_info_t))) == NULL);

    /* set info data */
    info->size = st.st_size;
    if(mime_type)
        snprintf(info->mime_type, MIME_TYPE_BUFSZ, "%s", mime_type);
    else
        info->mime_type[0] = 0;

    if(filename)
        snprintf(info->filename, U_FILENAME_MAX, "%s", filename);

    /* attach info to v */
    var_set_opaque(v, info);
    info = NULL;

    /* push into the cookie list */
    dbg_err_if(vars_add(rq->uploads, v));

    return 0;
err:
    if(info)
        U_FREE(info);
    if(v)
        var_free(v);
    return ~0;
}

static int request_get_uploaded_filev(request_t *rq, var_t *v,
    char local_filename[U_FILENAME_MAX], char client_filename[U_FILENAME_MAX],
    char mime_type[MIME_TYPE_BUFSZ], size_t *file_size)
{           
    upload_info_t *info;
    const char *tmp_fqn;

    dbg_err_if (rq == NULL);
    dbg_err_if (v == NULL);
    dbg_err_if (local_filename == NULL);
    dbg_err_if (client_filename == NULL);
    dbg_err_if (mime_type == NULL);
    dbg_err_if (file_size == NULL);

    info = var_get_opaque(v);
    dbg_err_if(info == NULL);

    tmp_fqn = var_get_value(v);
    dbg_err_if(tmp_fqn == NULL);

    /* copy out return values */
    u_strlcpy(local_filename, tmp_fqn, U_FILENAME_MAX);
    u_strlcpy(mime_type, info->mime_type, MIME_TYPE_BUFSZ);
    u_strlcpy(client_filename, info->filename, U_FILENAME_MAX);
    *file_size = info->size;

    return 0;
err:
    return ~0;
}

/** 
 * \ingroup request
 * \brief   Get info and handles of an uploaded file
 *  
 * Return information and handles about an uploaded file 
 *
 * \param rq    request object
 * \param name  form input tag variable name (&lt;input type=file name="xxx"&gt;)
 * \param idx   if more then one file with the same name param exists idx will 
 *              be used as an index (use 0 otherwise)
 * \param local_filename    on successfull exit will get the filename (with 
 *                          full path) of the temporary file where uploaded 
 *                          file content has been saved. 
 *                          Must be at least U_FILENAME_MAX bytes long
 * \param client_filename   filename as provided by the client
 *                          Must be at least U_FILENAME_MAX bytes long
 * \param mime_type         MIME type as stated y the client (may be "")
 *                          Must be at least MIME_TYPE_BUFSZ bytes long
 * \param file_size         file size of the uploaded file
 *  
 * \return \c 0 on success, \c ~0 otherwise
 */
int request_get_uploaded_file(request_t *rq, const char *name, size_t idx,
    char local_filename[U_FILENAME_MAX], char client_filename[U_FILENAME_MAX],
    char mime_type[MIME_TYPE_BUFSZ], size_t *file_size)
{
    var_t *v;

    dbg_err_if (rq == NULL);
    dbg_err_if (name == NULL);
    dbg_err_if (idx >= vars_count(rq->uploads));
    dbg_err_if (local_filename == NULL);
    dbg_err_if (client_filename == NULL);
    dbg_err_if (mime_type == NULL);
    dbg_err_if (file_size == NULL);

    v = vars_geti(rq->uploads, name, idx);
    dbg_err_if(v == NULL);

    return request_get_uploaded_filev(rq, v, local_filename, client_filename,
        mime_type, file_size);
err:
    return ~0;
}

static ssize_t request_read_until_boundary(request_t *rq, io_t *io, 
        const char *boundary, char *buf, size_t size, u_buf_t **pubuf)
{
    u_buf_t *ubuf = NULL;
    int found;
    size_t bound_len, trb;
    ssize_t rc;

    /* shortcut */
    bound_len = strlen(boundary);

    trb = 0;

    for(found = 0; !found; /* nothing */)
    {
        rc = read_until(io, boundary, buf, size, &found);
        dbg_err_if(rc <= 0); /* on error or eof exit */

        /* write all but the last bound_len + 2 (\r\n) bytes */
        if(found)
        {
            rc -= bound_len;
            dbg_err_if(rc < 0);

            /* zero-term the buffer (removing the boundary) */
            buf[rc] = 0;

        } else {

            /* buffer too small, alloc (and use) a dynamic buffer */
            if(ubuf == NULL)
            {
                dbg_err_if(u_buf_create(&ubuf));
                dbg_err_if(u_buf_reserve(ubuf, 2 * size));
            }
        }

        /* total read bytes */
        trb += rc;

        warn_err_ifm(trb > rq->post_maxsize, "POST data exceed post_maxsize");

        /* if we're using the buf on the heap then append read data; rc will
         * be zero if the last read returned just the boundary (that it's  not
         * going to be written to the output buffer) */
        if(ubuf && rc)
            dbg_err_if(u_buf_append(ubuf, buf, rc));
    }

    *pubuf = ubuf;

    return (ubuf ? u_buf_len(ubuf) : rc);
err:
    if(ubuf)
        u_buf_free(ubuf);
    return -1;

}

static int request_parse_multipart_chunk(request_t *rq, io_t *io, 
    const char *boundary, int *eof)
{
    enum { PRMSZ = 512, BUFSZ = 4096 };
    header_t *h = NULL;
    io_t *tmpio = NULL;
    var_t *v = NULL;
    u_buf_t *ubuf = NULL;
    char name[PRMSZ], filename[PRMSZ], buf[BUFSZ];
    size_t bound_len, rall;
    int found;
    ssize_t rc;

    /* create an header object to parse MIME part headers */
    dbg_err_if(header_create(&h));

    /* read header lines until the first blank line */
    dbg_err_if(header_load(h, io));

    warn_err_ifm(is_multipart_mixed(h), 
        "multipart/mixed content is not supported yet");

    /* HTTP should never use cte */
    warn_err_ifm(is_encoded(h), 
        "encoded file upload is not supported");

    dbg_err_if(parse_content_disposition(h, name, filename, PRMSZ));

    /* shortcut */
    bound_len = strlen(boundary);

    if(filename[0] != '\0')
    {
        dbg_err_if(BUFSZ <= bound_len);

        /* open a temporary file to dump uploaded data */
        dbg_err_if(u_tmpfile_open(rq->temp_dir, &tmpio));

        for(found = 0; !found; /* nothing */)
        {
            rc = read_until(io, boundary, buf, BUFSZ, &found);
            dbg_err_if(rc <= 0); /* on error or eof exit */

            /* write all but the last bound_len bytes (i.e. the boundary) */
            if(found)
            {
                rc -= bound_len;
                dbg_err_if(rc < 0);
            }

            /* write to the temp file */
            dbg_err_if(io_write(tmpio, buf, rc) < 0);
        }

        /* save the path/name of the tmp file to buf */
        dbg_err_if(io_name_get(tmpio, buf, BUFSZ));

        /* flush and free */
        io_free(tmpio); tmpio = NULL;

        /* add this file to the uploaded file list */
        dbg_err_if(request_add_uploaded_file(rq, name, filename, buf, 
            header_get_field_value(h, "Content-Type")));

    } else {
        /* read data before the boundary into the buffer. if the buffer is too 
           small the function will return all data in a new ubuf that must be 
           freed by the caller */
        rc = request_read_until_boundary(rq, io, boundary, buf, BUFSZ, &ubuf);
        dbg_err_if(rc < 0);

        /* add a new binary var to request arguments list */
        dbg_err_if(var_bin_create(name, 
                    (ubuf ? u_buf_ptr(ubuf) : buf), 
                    (ubuf ? u_buf_len(ubuf) : rc), 
                    &v));

        dbg_if(vars_add(rq->args, v));

        /* also add it to the post array */
        dbg_if(vars_add(rq->args_post, v));
    }

    /* will read "\r\n" for not-ending boundaries, "--" otherwise */
    dbg_err_if(io_read(io, buf, 2) <= 0);

    if(strncmp(buf, "--", 2) == 0)
    {
        *eof = 1; /* end of MIME stuff */

        rall = rq->content_length + rq->body_off;

        /* read and ignore the epilogue (if any) */
        while(io->rtot < rall)
        {
            if(io_read(io, buf, U_MIN(rall - io->rtot, sizeof(buf))) <= 0)
                break;
        }
    }

    if(ubuf)
        u_buf_free(ubuf);

    header_free(h);

    return 0;
err:
    if(ubuf)
        u_buf_free(ubuf);
    if(tmpio) {
        /* free space occupied by partial uploads */
        if(io_name_get(tmpio, buf, BUFSZ) == 0)
            u_remove(buf);
        io_free(tmpio);
    }
    if(h)
        header_free(h);
    return ~0;
}

static int request_parse_multipart_data(request_t *rq)
{
    enum { BOUNDARY_BUFSZ = 128, BUFSZ = 1024 }; 
    char boundary[BOUNDARY_BUFSZ], nl_boundary[BOUNDARY_BUFSZ], buf[BUFSZ], *nl;
    int eof, rc;

    /* boundary lines must start with -- */
    strcpy(boundary, "--");

    dbg_err_if(request_get_fieldparam(rq, "Content-Type", "boundary",
        boundary + 2, BOUNDARY_BUFSZ - 2));

    dbg_err_if(strlen(boundary) == 0);

    /* skip the MIME preamble (usually not used in HTTP) */
    for(;;)
    {
        dbg_err_if((rc = io_gets(rq->io, buf, BUFSZ)) <= 0);
        if(!strncmp(buf, boundary, strlen(boundary)))
            break; /* boundary found */
    }

    /* nl_boundary will contain: "\r\n--" + boundary */
    strcpy(nl_boundary, "\r\n");
    dbg_err_if(u_strlcat(nl_boundary, boundary, sizeof(nl_boundary)));

    /* cycle on each MIME part */
    for(eof = 0; eof == 0; )
        dbg_err_if(request_parse_multipart_chunk(rq, rq->io, nl_boundary,&eof));

    return 0;
err:
    return ~0;
}

static int request_cb_close_socket(talarm_t *al, void *arg)
{
    io_t *io = (io_t*)arg;

    u_unused_args(al);

    warn("[%x] connection timed out, closing", io);
    
    /* close the stream (but not free it) */
    io_close(io);

    return 0;
}

int request_parse_data(request_t *rq)
{
    talarm_t *al = NULL;
    io_t *io = request_io(rq);
    int rc = HTTP_STATUS_BAD_REQUEST;

    if(rq->method == HM_POST)
    {
        /* some vars may be urlencoded */
        dbg_err_if(request_parse_query_args(rq));

        /* Content-Length is required when using POST */
        dbg_err_if(request_set_content_length(rq) && 
            (rc = HTTP_STATUS_LENGTH_REQUIRED));

        if(rq->content_length == 0)
            return 0; /* no data posted */

        /* bytes read so far; will be used to know how many bytes we can read
         * from the client based on "Content-Length" value */
        rq->body_off = io->rtot;

        /* set a timeout to abort POST if it takes too long ... */
        dbg_err_if(timerm_add(rq->post_timeout, request_cb_close_socket, 
            (void*)rq->io, &al));

        /* abort if the client is pushing too much data */
        dbg_err_if(rq->content_length > rq->post_maxsize &&
            (rc = HTTP_STATUS_REQUEST_TOO_LARGE));

        if(request_is_multipart_formdata(rq))
        { 
            /* <form enctype="multipart/form-data" ...> */
            dbg_err_if(request_parse_multipart_data(rq));
        } else if(request_is_urlencoded(rq)) {
            /* if there's no content-type field or
               <form [enctype="application/x-www-form-urlencoded"] ...> */
            dbg_err_if(request_parse_urlencoded_data(rq));
        } else {
            /* nothing to do, the user will handle incoming data in his .kl1 */
        }

        /* post timeout not expired, clear it */
        dbg_if(timerm_del(al)); al = NULL;
    } else {
        /* parse urlencoded variables and set var_t* array */
        dbg_err_if(request_parse_query_args(rq));
    }

    return 0;
err:
    if(al)
        dbg_if(timerm_del(al));
    return rc;
}

/* cache data found in broker_is_valid_uri so we don't have to look for it 
   again in broker_serve */
void request_set_sup_info(request_t *rq, supplier_t *sup, void *handle, 
        time_t mtime)
{
    rq->si_sup = sup;
    rq->si_handle = handle;
    rq->si_mtime = mtime;

    return;
}

void request_get_sup_info(request_t *rq, supplier_t **psup, void **phandle, 
        time_t *pmtime)
{
    if(psup)
        *psup = rq->si_sup;

    if(phandle)
        *phandle = rq->si_handle;

    if(pmtime)
        *pmtime = rq->si_mtime;

    return;
}

/*
 * Parse request object \p rq.
 *
 * \param rq            request object
 * \param is_valid_uri  URI validation function
 * \param arg           argument to is_valid_uri
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_parse_header(request_t *rq, 
        int (*is_valid_uri)(void*, const char *, size_t),
        void* arg)
{
    enum { BUFSZ = 4096 };
    const char WP[] = " \t\r\n";
    char ln[BUFSZ], *pp, *method, *uri, *proto;
    talarm_t *al = NULL;
    
    dbg_err_if (rq == NULL);
    dbg_err_if (rq->io == NULL); /* must call rq_bind before rq_parse */

    /* wait at most N seconds to receive the request */
    dbg_err_if(timerm_add(rq->idle_timeout, request_cb_close_socket, 
        (void*)rq->io, &al));

    if(!rq->cgi)
    {
        /* cp the first line */
        dbg_err_if(io_gets(rq->io, ln, BUFSZ) <= 0);

        /* save the verbatim request line */
        dbg_err_if(request_set_client_request(rq, ln));

        method = strtok_r(ln, WP, &pp); 
        dbg_err_if(!method || request_set_method(rq, method));

        uri = strtok_r(NULL, WP, &pp);
        dbg_err_if(!uri || request_set_uri(rq, uri, is_valid_uri, arg));

        /* HTTP/0.9 not supported yet */ 
        proto = strtok_r(NULL, WP, &pp);
        dbg_err_if(!proto || request_set_proto(rq, proto)); 

        dbg_err_if(header_load(rq->header, rq->io));
    } else {
        dbg_err_if(header_load_from_cgienv(rq->header));
    }

    /* set if-modified-since time_t value */
    dbg_err_if(request_parse_ims(rq));

    /* parse "Cookie:" fields and set the cookies vars_t */
    dbg_err_if(request_parse_cookies(rq));

    /* Content-Length is required when using POST */
    if(request_get_method(rq) == HM_POST)
        dbg_err_if(request_set_content_length(rq));

    /* idle timeout not expired, clear it */
    dbg_if(timerm_del(al)); al = NULL;

    return 0;
err:
    if(al)
        timerm_del(al);
    return ~0;
}

/** 
 * \ingroup request
 * \brief   Get the method of a request
 *  
 * Return the method of request \p rq. Refer to http.h for possible methods.
 *
 * \param rq    request object
 *  
 * \return
 *  - the method of the given \p rq  
 */
int request_get_method(request_t *rq)
{
    dbg_return_if (rq == NULL, HM_UNKNOWN);

    return rq->method;
}

/** 
 * \ingroup request
 * \brief   Get the protocol used by the client request
 *  
 * Return the protocol of request \p rq ("HTTP/1.0", "HTTP/1.1", etc.)
 *
 * \param rq    request object
 *  
 * \return
 *  - the method of the given \p rq  
 */
const char* request_get_protocol(request_t *rq)
{
    dbg_return_if (rq == NULL, "unknown");

    return rq->protocol;
}

/** 
 * \ingroup request
 * \brief   Get resolved filename of a request
 *  
 * Return a string representing the resolved filename of request \p rq.
 *
 * \param rq    request object
 *  
 * \return the resolved file name bound to \p rq (may be \c NULL)
 */
const char *request_get_resolved_filename(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->resolved_filename;
}

/** 
 * \ingroup request
 * \brief   Get the resolved path info of a request.
 *  
 * Return a string representing the resolved path info of request \p rq.
 *
 * \param rq    request object
 *  
 * \return the resolved path info of the given \p rq (may be \c NULL)
 */
const char *request_get_resolved_path_info(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->resolved_path_info;
}

int request_print(request_t *rq)
{
    dbg_return_if (rq == NULL, ~0);

    u_dbg("method: %u", rq->method);
    u_dbg("uri: %s", rq->uri);
    u_dbg("proto: %s", rq->protocol);
    u_dbg("filename: %s", rq->filename);
    u_dbg("resolved filename: %s", rq->resolved_filename);
    u_dbg("path_info: %s", rq->path_info);
    u_dbg("resolved path_info: %s", rq->resolved_path_info);
    u_dbg("query: %s", rq->query);

    return 0;
}

static int request_load_config(request_t *rq)
{
    u_config_t *c;
    vhost_t *vhost;
    const char *v;

    dbg_err_if (rq == NULL);
    dbg_err_if (rq->http == NULL);

    dbg_err_if((vhost = http_get_vhost(rq->http, rq)) == NULL);

    /* vhost can override those fields */
    dbg_err_if((c = vhost->config) == NULL);
    
    /* defaults */
    rq->idle_timeout = REQUEST_DEFAULT_IDLE_TIMEOUT;
    rq->post_timeout = REQUEST_DEFAULT_POST_TIMEOUT;
    rq->post_maxsize = REQUEST_DEFAULT_POST_MAXSIZE;
    rq->temp_dir = NULL;    /* use system default */

    /* idle timeout */
    if((v = u_config_get_subkey_value(c, "idle_timeout")) != NULL)
        rq->idle_timeout = U_MAX(1, atoi(v));

    /* post timeout */
    if((v = u_config_get_subkey_value(c, "post_timeout")) != NULL)
        rq->post_timeout = U_MAX(5, atoi(v));

    /* post maxsize */
    if((v = u_config_get_subkey_value(c, "post_maxsize")) != NULL)
        rq->post_maxsize = U_MAX(1024, atoi(v));

    /* temp dir to use on file upload.  when 'NULL' use system default */
    rq->temp_dir = u_config_get_subkey_value(c, "temp_dir");

    return 0;
err:
    return ~0;
}

int request_create(http_t *http, request_t **prq)
{
    request_t *rq = NULL;

    dbg_return_if (prq == NULL, ~0);
    dbg_return_if (http == NULL, ~0);

    rq = u_zalloc(sizeof(request_t));
    dbg_err_if(rq == NULL);

    dbg_err_if(header_create(&rq->header));

    dbg_err_if(vars_create(&rq->args));
    dbg_err_if(vars_create(&rq->cookies));
    dbg_err_if(vars_create(&rq->uploads));

    dbg_err_if(vars_create(&rq->args_get));
    dbg_err_if(vars_create(&rq->args_post));

    /* args_get and args_post link to var_t owned by the rq->args list */
    dbg_err_if(vars_set_flags(rq->args_get, VARS_FLAG_FOREIGN));
    dbg_err_if(vars_set_flags(rq->args_post, VARS_FLAG_FOREIGN));

    rq->http = http;

    dbg_err_if(request_load_config(rq));

    *prq = rq;

    return 0;
err:
    if(rq)
        request_free(rq);
    return ~0;
}

static int request_unlink_uploads(var_t *v, void * arg)
{
    dbg_err_if (v == NULL);

    u_unused_args(arg);

    if(var_get_opaque(v) && var_get_value(v))
    {   /* it's a file var, unlink v->value */
        u_remove(var_get_value(v));
    }

err:
    return 0;
}

int request_free(request_t *rq)
{
    if (rq)
    {
        /* free internal stuff */
        request_clear_uri(rq);

        /* verbatim client request line */
        U_FREE(rq->cli_rq);

        if(rq->header)
            header_free(rq->header);

        if(rq->io)
            io_free(rq->io);

        if(rq->uploads)
        {
            /* unlink uploaded files (if any) */
            vars_foreach(rq->uploads, request_unlink_uploads, NULL);
            vars_free(rq->uploads);
        }

        if(rq->cookies)
            vars_free(rq->cookies);

        if(rq->args_get)
            vars_free(rq->args_get);

        if(rq->args_post)
            vars_free(rq->args_post);

        if(rq->args)
            vars_free(rq->args);

        U_FREE(rq);
    }

    return 0;
}

/* save the local connected address in the request obj */
int request_set_addr(request_t *rq, const char *addr)
{
    dbg_return_if (rq == NULL, ~0);
    dbg_return_if (addr == NULL, ~0);

    return u_strlcpy(rq->local_addr, addr, sizeof rq->local_addr);
}

/* save the address of the connected peer in the request obj */
int request_set_peer_addr(request_t *rq, const char *addr)
{
    dbg_return_if (rq == NULL, ~0);
    dbg_return_if (addr == NULL, ~0);

    return u_strlcpy(rq->peer_addr, addr, sizeof rq->peer_addr);
}

/** 
 * \ingroup request
 * \brief   Return the local address
 *  
 * Return a string containing the address of the accepted the socket
 *
 * \param rq    request object
 *  
 * \return      the address string 
 */
const char *request_get_addr(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->local_addr;
}

/** 
 * \ingroup request
 * \brief   Return the peer address
 *  
 * Return a string containing the address of the connected peer
 *
 * \param rq    request object
 *  
 * \return      the address string
 */
const char *request_get_peer_addr(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->peer_addr;
}

/** 
 * \ingroup request
 * \brief   Return the header object
 *  
 * Return the header object
 *
 * \param rq    request object
 *  
 * \return      a pointer to an header_t type
 * \sa      header_t
 */
header_t* request_get_header(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->header;
}

/** 
 * \ingroup request
 * \brief   Get an header field
 *  
 * Return the header field named \p name.
 *
 * \param rq    request object
 * \param name  the name of the field
 *  
 * \return      the header field or NULL if the header is not found
 */
field_t* request_get_field(request_t *rq, const char *name)
{
    dbg_return_if (rq == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    return header_get_field(rq->header, name);
}

/** 
 * \ingroup request
 * \brief   Get the value of an header field
 *  
 * Return the value of an header field named \p name.
 *
 * \param rq    request object
 * \param name  the name of the field
 *  
 * \return      the value of the field or NULL if the field is not found
 */
const char* request_get_field_value(request_t *rq, const char *name)
{
    dbg_return_if (rq == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    return header_get_field_value(rq->header, name);
}

vhost_t* request_get_vhost(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->vhost; /* may be NULL */
}

int request_set_vhost(request_t *rq, vhost_t *vhost)
{
    dbg_return_if (rq == NULL, ~0);

    rq->vhost = vhost;

    return 0;
}

