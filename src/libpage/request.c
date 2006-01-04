/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: request.c,v 1.20 2006/01/04 15:38:55 tat Exp $
 */

#include "klone_conf.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <u/libu.h>
#include <klone/request.h>
#include <klone/utils.h>
#include <klone/io.h>
#include <klone/http.h>
#include <klone/addr.h>
#include <klone/vars.h>

struct request_s
{
    http_t *http;               /* http server handle                */
    header_t *header;           /* input header                      */
    io_t *io;                   /* input io stream                   */

    int method;                 /* get,post,etc.                     */
    char *uri;                  /* verbatim uri asked by the client  */
    char *protocol;             /* proto/ver                         */
    char *path_info;            /* extra info at the end of the path */
    char *query;                /* query string (data after '?')     */
    char *filename;             /* path of the req resource          */
    char *host;                 /* Host: field                       */

    char *resolved_path_info;   /* resolved path_info                */
    char *resolved_filename;    /* unaliased filename                */

    vars_t *args;               /* get/post args                     */
    vars_t *cookies;            /* cookies                           */

    char *content_type;         /* type/subtype                      */
    char *content_encoding;     /* 7bit/8bit/base64/qp, etc          */
	size_t content_length;      /* content-length http header field  */
    time_t if_modified_since;   /* time_t IMS header                 */

    addr_t local_addr, peer_addr; /* local and perr address          */
    int cgi;                    /* if running in cgi mode            */
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

/**
 *  \defgroup request_t request_t - request handling
 *  \{
 *      \par
 *      Basic knowledge of the HTTP protocol is assumed. Hence only the
 *      essential information is given. Some useful references are:
 *        - RFC 2616 for a complete description of HTTP 1.1 header fields
 *        - RFC 2109 for cookie format
 *        - RFC 822 for standard data type formats
 *        - http://www.iana.org/assignments/media-types/ for an updated
 *          list of possible mime-types
 */

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
 * \brief   Get request arguments
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
 * \brief   Get a request argument
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
 * \brief   Set a request field
 *  
 * Set field \p name to \p value in request \p rq
 *
 * \param rq     request object
 * \param name   name of the argument
 * \param value  value of the argument
 *  
 * \return \c 0 if successful, non-zero on error
 */
int request_set_field(request_t *rq, const char *name, const char *value)
{
    dbg_return_if (rq == NULL, ~0);
    dbg_return_if (name == NULL, ~0);
    dbg_return_if (value == NULL, ~0);

    return header_set_field(rq->header, name, value);
}

/** 
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

/**
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

/**
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

/**
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

/**
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

/**
 * \brief   Clear the URI field of a request
 *
 * Clear the URI field of request \p rq.
 *
 * \param rq  request object
 *
 * \return nothing
 */
void request_clear_uri(request_t *rq)
{
    U_FREE(rq->uri);
    U_FREE(rq->protocol);
    U_FREE(rq->path_info);
    U_FREE(rq->query);
    U_FREE(rq->filename);
    U_FREE(rq->host);
    U_FREE(rq->resolved_path_info);
    U_FREE(rq->resolved_filename);
    U_FREE(rq->content_type);
    U_FREE(rq->content_encoding);
}

/**
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

/**
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
    char *p, *fn, *pi, *cp = NULL;
    size_t uri_len = strlen(uri);

    dbg_err_if (rq == NULL);
    dbg_err_if (uri == NULL);
 
    request_clear_uri(rq);

    REQUEST_SET_STRING_FIELD(rq->uri, uri);

    /* save (undecoded) query string i.e. everything after '?' */
    if((p = strchr(uri, '?')) != NULL)
        dbg_err_if(request_set_query_string(rq, ++p));

    cp = (char*)u_malloc(uri_len + 1);
    dbg_err_if(cp == NULL);

    /* copy decoded url */
    dbg_err_if(u_urlncpy(cp, rq->uri, uri_len, URLCPY_DECODE) <= 0);

    if((p = strchr(cp, '?')) != NULL)
        *p++ = 0; /* remove query string from the uri copy */

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

    U_FREE(cp);

    return 0;
err:
    U_FREE(cp);
    return ~0;
}

static int request_set_proto(request_t *rq, const char *proto)
{
    dbg_err_if (rq == NULL);
    dbg_err_if (proto == NULL);

    REQUEST_SET_STRING_FIELD(rq->protocol, proto);

    return 0;
err:
    return ~0;
}

/**
 * \brief   Set the method of a request
 *
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
    else
        rq->method = HM_UNKNOWN;

    return 0;
}

static int request_set_content_length(request_t *rq)
{
    const char *clen;
    size_t len;

    clen = header_get_field_value(rq->header, "Content-Length");
    dbg_err_if(clen == NULL || (len = atoi(clen)) <= 0);

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
    strncpy(buf, field_get_value(field), BUFSZ);

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

static int request_parse_args(request_t *rq)
{
    char *pp, *tok, *src, *query = NULL;

    dbg_err_if (rq == NULL);

    if(!rq->query)
        return 0; /* no args */

    /* dup to tokenize it */
    query = u_strdup(rq->query);
    dbg_err_if(query == NULL);

    /* foreach name=value pair... */
    for(src = query; (tok = strtok_r(src, "&", &pp)) != NULL; src = NULL)
    {
        /* create a new var_t obj and push it into the args vars-list */
        dbg_if(vars_add_urlvar(rq->args, tok, NULL));
    }

    U_FREE(query);

    return 0;
err:
    U_FREE(query);
    return ~0;
}

/* set is-cgi flag */
void request_set_cgi(request_t *rq, int cgi)
{
    rq->cgi = cgi;
    return;
}

/** 
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

    ct = header_get_field_value(h, "Content-Type");
    if(ct == NULL || strncasecmp(ct, mime_type, strlen(mime_type)))
        return 0;

    return 1;
}

static int request_is_multipart_formdata(request_t *rq)
{
    return match_content_type(rq->header, "multipart/form-data");
}

static int request_parse_urlencoded_data(request_t *rq)
{
    ssize_t qsz, len;

    len = rq->content_length; /* shortcut */

    qsz = (rq->query ? strlen(rq->query) : 0);

    /* alloc or enlarge the query string buffer */
    rq->query = u_realloc(rq->query, len + qsz + 2);
    dbg_err_if(rq->query == NULL);

    /* dbg("rq->query %x  size %u", rq->query, len+qsz+2); */

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

    /* parse rq->query and build the args var_t* array */
    dbg_err_if(request_parse_args(rq));

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

    field_value = header_get_field_value(rq->header, field_name);
    dbg_err_if(field_value == NULL);

    /* look for param name=value pair */
    param_value = strcasestr(field_value, param_name);
    dbg_err_if(param_value == NULL);

    /* skip param name */
    param_value += strlen(param_name);

    /* first char must be an equal sign */
    dbg_err_if(*param_value++ != '=');

    /* a param value ends on the first ';', space or at the end of string */
    for(p = param_value; ;++p)
        if(*p == '\0' || *p == ';' || isspace(*p))
            break; /* end of param value */

    /* param value len */
    pv_len = p - param_value;

    /* boundary check */
    dbg_err_if(pv_len > size - 1); 

    /* copy out the param value */
    strncpy(buf, param_value, pv_len);
    buf[MIN(pv_len, size - 1)] = 0;

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

    cd = header_get_field_value(h, "Content-Disposition");
    dbg_err_if(cd == NULL);

    dbg_err_if(strlen(cd) >= BUFSZ);

    /* must start with form-data */
    dbg_err_if(strncmp(cd, "form-data", strlen("form_data")));

    name[0] = filename[0] = 0;

    /* save a copy to tokenize it */
    strncpy(buf, cd, BUFSZ);

    /* shortcut */
    n_len = strlen("name=");
    fn_len = strlen("filename=");

    /* foreach name=value pair... */
    for(src = buf; (tok = strtok_r(src, " ;", &pp)) != NULL; src = NULL)
    {
        if(strncmp(tok, "form-data", strlen("form-data")) == 0)
            continue;   /* skip */
        else if(strncmp(tok, "name=", n_len) == 0) {
            /* skip the name part */
            tok += n_len;

            /* remove single or double quotes */
            if((q = is_quote(tok[0])) != 0)
                ++tok;
            if(strlen(tok) && tok[strlen(tok) - 1] == q)
                tok[strlen(tok) - 1] = 0;

            strncpy(name, tok, prmsz);
        } else if(strncmp(tok, "filename=", fn_len) == 0) {
            /* skip the filename part */
            tok += n_len;

            /* remove single or double quotes */
            if((q = is_quote(tok[0])) != 0)
                ++tok;
            if(strlen(tok) && tok[strlen(tok) - 1] == q)
                tok[strlen(tok) - 1] = 0;

            strncpy(filename, tok, prmsz);
        } 
        /* else ignore unknown fields */
    }
            
    return 0;
err:
    return ~0;
}

static int request_parse_multipart_chunk(request_t *rq, io_t *io, 
    const char *boundary, int *eof)
{
    enum { PRMSZ = 512, BUFSZ = 4096 };
    header_t *h = NULL;
    char name[PRMSZ], filename[PRMSZ], buf[BUFSZ];
    size_t bound_len;

    /* create an header object to parse MIME part headers */
    dbg_err_if(header_create(&h));

    /* read header lines until the first blank line */
    dbg_err_if(header_load(h, io));

    warn_err_ifm(is_multipart_mixed(h), 
        "multipart/mixed content is not supported yet");

    /* HTTP should never use cte */
    warn_err_ifm(is_encoded(h), 
        "encoded file upload is not supported");

    dbg_err_if(parse_content_disposition(h, name, filename, BUFSZ));

    if(filename[0] != '\0')
    {
        /* FIXME save to tmp file */
        warn_err("not impl");
    } else {
        /* read param value from the io and add a new item in rq->args */
        dbg_err_if(u_snprintf(buf, BUFSZ, "%s=", name));

        /* append the param value to the buffer */
        dbg_err_if(io_gets(io, buf + strlen(buf), BUFSZ - strlen(buf)) <=0);

        /* remove trailing new lines */
        while(is_nl(buf[strlen(buf) - 1]))
            buf[strlen(buf) - 1] = 0;

        /* add a new var to request arguments list */
        dbg_if(vars_add_urlvar(rq->args, buf, NULL));
    }

    /* read next boundary */
    dbg_err_if(io_gets(io, buf, BUFSZ) <= 0);

    /* shortcut */
    bound_len = strlen(boundary);

    /* err if next line is not a boundary */
    dbg_err_if(strncmp(buf, boundary, bound_len));

    if(strncmp(buf + bound_len, "--", 2) == 0)
        *eof = 1; /* end of MIME stuff */

    header_free(h);

    return 0;
err:
    if(h)
        header_free(h);
    return ~0;
}

static int request_parse_multipart_data(request_t *rq)
{
    enum { BOUNDARY_BUFSZ = 128, BUFSZ = 1024 }; 
    char boundary[BOUNDARY_BUFSZ], buf[BUFSZ];
    int eof;

    /* boundaries always start with -- */
    strcpy(boundary, "--");

    dbg_err_if(request_get_fieldparam(rq, "Content-Type", "boundary",
        boundary + 2, BOUNDARY_BUFSZ - 2));

    dbg_err_if(strlen(boundary) == 0);

    /* skip the MIME preamble (usually not used in HTTP) */
    for(;;)
    {
        dbg_err_if(io_gets(rq->io, buf, BUFSZ) <= 0);
        if(!strncmp(buf, boundary, strlen(boundary)))
            break; /* boundary found */
    }

    /* cycle on each MIME part */
    for(eof = 0; eof == 0; )
        dbg_err_if(request_parse_multipart_chunk(rq, rq->io, boundary, &eof));

    return 0;
err:
    return ~0;
}


/*
 * \brief   Parse a request object
 *  
 * Parse request object \p rq.
 *
 * \param rq            request object
 * \param is_valid_uri  URI validation function
 * \param arg           argument to is_valid_uri
 *
 * \return \c 0 if successful, non-zero on error
 */
int request_parse(request_t *rq, 
        int (*is_valid_uri)(void*, const char *, size_t),
        void* arg)
{
    enum { BUFSZ = 1024 };
    const char WP[] = " \t\r\n";
    char ln[BUFSZ], *pp, *method, *uri, *proto;
    
    dbg_err_if (rq == NULL);
    dbg_err_if (rq->io == NULL); /* must call rq_bind before rq_parse */

    if(!rq->cgi)
    {
        /* cp the first line */
        dbg_err_if(io_gets(rq->io, ln, BUFSZ) == 0);

        method = strtok_r(ln, WP, &pp); 
        dbg_err_if(!method || request_set_method(rq, method));

        uri = strtok_r(NULL, WP, &pp);
        dbg_err_if(!uri || request_set_uri(rq, uri, is_valid_uri, arg));

        /* HTTP/0.9 not supported yet */ 
        proto = strtok_r(NULL, WP, &pp);
        dbg_err_if(!proto || request_set_proto(rq, proto)); 

        dbg_err_if(header_load(rq->header, rq->io));
    } else
        dbg_err_if(header_load_from_cgienv(rq->header));

    /* set if-modified-since time_t value */
    dbg_err_if(request_parse_ims(rq));

    /* parse "Cookie:" fields and set the cookies vars_t */
    dbg_err_if(request_parse_cookies(rq));

    if(rq->method == HM_POST)
    {
        dbg_err_if(request_set_content_length(rq));

        dbg_err_if(rq->content_length > 1024000); /* FIXME: no more the 1MB */

        if(request_is_multipart_formdata(rq))
        { 
            /* some vars may be urlencoded */
            dbg_err_if(request_parse_args(rq));

            /* <form enctype="multipart/form-data" ...> */
            dbg_err_if(request_parse_multipart_data(rq));
        } else {
            /* <form [enctype="application/x-www-form-urlencoded"] ...> */
            dbg_err_if(request_parse_urlencoded_data(rq));
        }
    } else {
        /* parse urlencoded variables and set var_t* array */
        dbg_err_if(request_parse_args(rq));
    }

    return 0;
err:
    return ~0;
}

/** 
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

    dbg("method: %u", rq->method);
    dbg("uri: %s", rq->uri);
    dbg("proto: %s", rq->protocol);
    dbg("filename: %s", rq->filename);
    dbg("resolved filename: %s", rq->resolved_filename);
    dbg("path_info: %s", rq->path_info);
    dbg("resolved path_info: %s", rq->resolved_path_info);
    dbg("query: %s", rq->query);

    return 0;
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

    rq->http = http;

    *prq = rq;

    return 0;
err:
    if(rq)
        request_free(rq);
    return ~0;
}

int request_free(request_t *rq)
{
    if (rq)
    {
        request_clear_uri(rq);

        if(rq->header)
            header_free(rq->header);

        if(rq->io)
            io_free(rq->io);

        if(rq->cookies)
            vars_free(rq->cookies);

        if(rq->args)
            vars_free(rq->args);

        U_FREE(rq);
    }

    return 0;
}

/* save the local address struct (ip and port) in the request obj */
int request_set_addr(request_t *rq, addr_t *addr)
{
    dbg_return_if (rq == NULL, ~0);
    dbg_return_if (addr == NULL, ~0);

    memcpy(&rq->local_addr, addr, sizeof(addr_t));

    return 0;
}

/* save the peer address struct (ip and port) in the request obj */
int request_set_peer_addr(request_t *rq, addr_t *addr)
{
    dbg_return_if (rq == NULL, ~0);
    dbg_return_if (addr == NULL, ~0);

    memcpy(&rq->peer_addr, addr, sizeof(addr_t));

    return 0;
}

/* return the local socket address */
addr_t* request_get_addr(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return &rq->local_addr;
}

/* return the peer address */
addr_t* request_get_peer_addr(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return &rq->peer_addr;
}

/* return the header obj */
header_t* request_get_header(request_t *rq)
{
    dbg_return_if (rq == NULL, NULL);

    return rq->header;
}

/* return a field obj of the field named 'name' or NULL if the field does not 
   exist */
field_t* request_get_field(request_t *rq, const char *name)
{
    dbg_return_if (rq == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    return header_get_field(rq->header, name);
}

/* return the string value of the field named 'name' or NULL if the field does
   not exist */
const char* request_get_field_value(request_t *rq, const char *name)
{
    dbg_return_if (rq == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    return header_get_field_value(rq->header, name);
}

/**
 *  \}
 */ 
