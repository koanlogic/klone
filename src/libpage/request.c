#include <stdlib.h>
#include <string.h>
#include <klone/request.h>
#include <klone/utils.h>
#include <klone/io.h>
#include <klone/http.h>
#include <klone/vars.h>
#include <u/libu.h>

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
};

#define RQ_FREE(s) if(s) { u_free(s); s = NULL; } 
#define REQUEST_SET_STRING_FIELD(lval, rval)        \
    do {                                            \
        if(lval)                                    \
        {                                           \
            u_free(lval);                           \
            lval = NULL;                            \
        }                                           \
        if(rval)                                    \
        {                                           \
            lval = u_strdup(rval);                  \
            dbg_err_if(lval == NULL);               \
        }                                           \
    } while(0)



/** 
 *  \ingroup Chttp
 *  \{
 */

/**
 *          \defgroup request HTTP request handling
 *          \{
 *              \par
 */

int request_is_encoding_accepted(request_t *rq, const char *encoding)
{
    char *pp, *tok, *src, *buf = NULL;
    const char *accept_encoding;
    int rc = 0;

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

        u_free(buf);
    }

    return rc;
err:
    if(buf)
        u_free(buf);
    return 0;
}

/**
 * \brief   Get the io_t object associated with a request object
 *
 * Return the I/O object (io_t) used by the request object passed as 
 * parameter. The io_t object is bound to the socket connected to the
 * client (the web browser)
 *
 * \param rq    request object
 *
 * \return child io object of the given \a rq or NULL if no io object has 
 *  been set
 *
 * \sa io_t
 */
io_t* request_io(request_t *rq)
{
    return rq->io;
}

/**
 * \brief   Get the cookies list
 *
 * Return a vars_t object containing the list of all cookies sent by the
 * browser.
 *
 * \param rq  request object
 *
 * \return the cookie list of the given \a rq
 */
vars_t *request_get_cookies(request_t *rq)
{
    return rq->cookies;
}

/**
 * \brief   Get the value of a cookie named \a name
 *
 * Return the value of a cookie sent by the browser
 *
 * \param rq    request object
 * \param name  cookie name
 *
 * \return the cookie value or NULL on error
 */
const char *request_get_cookie(request_t *rq, const char *name)
{
    var_t *v;

    v = vars_get(rq->cookies, name);
    return v ? var_get_value(v): NULL;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq  parameter \a rq description
 *
 * \return
 *  - the arguments' list of the given \a rq
 */
vars_t *request_get_args(request_t *rq)
{
    return rq->args;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 * \param name  parameter \a name description
 *
 * \return
 *  - \c NULL if there's no argument \a name
 *  - the string value corresponding to the supplied \a name 
 */
const char *request_get_arg(request_t *rq, const char *name)
{
    var_t *v;

    v = vars_get(rq->args, name);
    return v ? var_get_value(v): NULL;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq     parameter \a rq description
 * \param name   parameter \a rs description
 * \param value  parameter \a pss description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int request_set_field(request_t *rq, const char *name, const char *value)
{
    return header_set_field(rq->header, name, value);
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 *  
 * \return
 *  - the uri child object of the given \a rq
 */
const char *request_get_uri(request_t *rq)
{
    return rq->uri;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 *  
 * \return
 *  - the file name binded to the \a rq 
 */
const char *request_get_filename(request_t *rq)
{
    return rq->filename;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq        parameter \a rq description
 * \param filename  parameter \a filename description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int request_set_filename(request_t *rq, const char *filename)
{
    REQUEST_SET_STRING_FIELD(rq->filename, filename);

    return 0;
err:
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 *  
 * \return
 *  - the query string bind to the request \a rq
 */
const char *request_get_query_string(request_t *rq)
{
    return rq->query;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 *  
 * \return
 *  - the path info of the given \a rq
 */
const char *request_get_path_info(request_t *rq)
{
    return rq->path_info;
}

/* parse and set if-modified-since value */
static int request_parse_ims(request_t *rq)
{
    const char *ims;

    rq->if_modified_since = 0;

    ims = header_get_field_value(rq->header, "If-Modified-Since");
    if(ims)
        dbg_err_if(u_httpdate_to_tt(ims, &rq->if_modified_since));

err: /* ignore if it's not formatted properly */
    return 0;
}


/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq          parameter \a rq description
 * \param resolved_fn parameter \a resolved_fn description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
time_t request_get_if_modified_since(request_t *rq)
{
    return rq->if_modified_since;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq          parameter \a rq description
 * \param resolved_fn parameter \a resolved_fn description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int request_set_resolved_filename(request_t *rq, const char *resolved_fn)
{
    REQUEST_SET_STRING_FIELD(rq->resolved_filename, resolved_fn);

    return 0;
err:
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 *  
 * \return
 *  - the child http object of the given \a rq
 */
http_t* request_get_http(request_t *rq)
{
    return rq->http;
}

/**
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 * \param in    parameter \a in description
 *
 * \return
 *  - \c 0  always
 */
int request_bind(request_t *rq, io_t *in)
{
    rq->io = in;

    return 0;
}

/**
 * \brief   One line description
 *
 * Parse \a query string and build the \a rq->args list.
 *
 * \param rq     parameter \a rq description
 * \param query  parameter \a query description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int request_set_query_string(request_t *rq, const char *query)
{
    REQUEST_SET_STRING_FIELD(rq->query, query);

    return 0;
err:
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq  parameter \a rq description
 *
 * \return
 *  - nothing
 */
void request_clear_uri(request_t *rq)
{
    RQ_FREE(rq->uri);
    RQ_FREE(rq->protocol);
    RQ_FREE(rq->path_info);
    RQ_FREE(rq->query);
    RQ_FREE(rq->filename);
    RQ_FREE(rq->host);
    RQ_FREE(rq->resolved_path_info);
    RQ_FREE(rq->resolved_filename);
    RQ_FREE(rq->content_type);
    RQ_FREE(rq->content_encoding);
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq         parameter \a rq description
 * \param path_info  parameter \a path_info description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int request_set_path_info(request_t *rq, const char *path_info)
{
    REQUEST_SET_STRING_FIELD(rq->path_info, path_info);

    return 0;
err:
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq           parameter \a rq description
 * \param resolved_pi  parameter \a resolved_pi description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int request_set_resolved_path_info(request_t *rq, const char *resolved_pi)
{
    REQUEST_SET_STRING_FIELD(rq->resolved_path_info, resolved_pi);

    return 0;
err:
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq           parameter \a rq description
 * \param uri          parameter \a uri description
 * \param is_valid_uri parameter \a is_valid_uri description
 * \param arg          parameter \a arg description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int request_set_uri(request_t *rq, const char *uri,
        int (*is_valid_uri)(void*, const char *, size_t),
        void* arg)
{
    char *p, *fn, *pi, *cp = NULL;
    size_t uri_len = strlen(uri);

    request_clear_uri(rq);

    REQUEST_SET_STRING_FIELD(rq->uri, uri);

    /* save (undencoded) query string i.e. everything after '?' */
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

    u_free(cp);

    return 0;
err:
    if(cp)
        u_free(cp);
    return ~0;
}

static int request_set_proto(request_t *rq, const char *proto)
{
    REQUEST_SET_STRING_FIELD(rq->protocol, proto);

    return 0;
err:
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq     parameter \a rq description
 * \param method parameter \a method description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int request_set_method(request_t *rq, const char *method)
{
    if(method == NULL)
        rq->method = HM_UNKNOWN;
    else if(!strcasecmp(method, "get"))
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
        return ~0;
    return 0;
}


static int request_parse_cookie(request_t *rq, field_t *field)
{
    enum { BUFSZ = 4096 }; /* cookie size limit */
    char *pp, *tok, *src, buf[BUFSZ];

    dbg_err_if(field_get_value(field) == NULL);

    /* save a copy to tokenize it */
    strncpy(buf, field_get_value(field), BUFSZ);

    /* foreach name=value pair... */
    for(src = buf; (tok = strtok_r(src, " ;", &pp)) != NULL; src = NULL)
        dbg_err_if(vars_add_urlvar(rq->cookies, tok, NULL));

    return 0;
err:
    return ~0;
}

static int request_parse_cookies(request_t *rq)
{
    field_t *f;
    size_t i, count;

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

    if(!rq->query)
        return 0; /* no args */

    /* dup to tokenize it */
    query = u_strdup(rq->query);
    dbg_err_if(query == NULL);

    /* foreach name=value pair... */
    for(src = query; (tok = strtok_r(src, "&", &pp)) != NULL; src = NULL)
    {
        /* create a new var_t obj and push it into the args vars-list */
        dbg_err_if(vars_add_urlvar(rq->args, tok, NULL));
    }

    u_free(query);

    return 0;
err:
    if(query)
        u_free(query);
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 *  
 * \return
 *  - the content length of the given \a rq
 */
size_t request_get_content_length(request_t *rq)
{
    return rq->content_length;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq            parameter \a rq description
 * \param is_valid_uri  parameter \a is_valid_uri description
 * \param arg           parameter \a arg description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int request_parse(request_t *rq, 
        int (*is_valid_uri)(void*, const char *, size_t),
        void* arg)
{
    enum { BUFSZ = 1024 };
    const char WP[] = " \t\r\n";
    char ln[BUFSZ], *pp, *method, *uri, *proto;
    const char *clen;
    size_t qsz, len;
    
    dbg_err_if(rq->io == NULL); /* must call rq_bind before rq_parse */

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

    /* set if-modified-since time_t value */
    dbg_err_if(request_parse_ims(rq));

    /* parse "Cookie:" fields and set the cookies vars_t */
    dbg_err_if(request_parse_cookies(rq));

    if(rq->method == HM_POST)
    {
        clen = header_get_field_value(rq->header, "Content-Length");
        if(clen != NULL && (len = atoi(clen)) > 0)
        {
            dbg_err_if(len > 2097152); /* FIXME: no more the 2 MB */

            rq->content_length = len;

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
        }
    }

    /* parse rq->query and build the args var_t* array */
    dbg_err_if(request_parse_args(rq));

    return 0;
err:
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 *  
 * \return
 *  - the method of the given \a rq  
 */
int request_get_method(request_t *rq)
{
    return rq->method;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 *  
 * \return
 *  - the resolved file name binded to the \a rq
 */
const char *request_get_resolved_filename(request_t *rq)
{
    return rq->resolved_filename;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 *  
 * \return
 *  - the resolved path info of the given \a rq
 */
const char *request_get_resolved_path_info(request_t *rq)
{
    return rq->resolved_path_info;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rq  parameter \a rq description
 *
 * \return
 *  - \c 0  always
 */
int request_print(request_t *rq)
{
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

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param http  parameter \a http description
 * \param prq   parameter \a prq description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int request_create(http_t *http, request_t **prq)
{
    request_t *rq = NULL;

    rq = u_zalloc(sizeof(request_t));
    dbg_err_if(rq == NULL);

    dbg_err_if(header_create(&rq->header));

    dbg_err_if(vars_create(&rq->args));
    dbg_err_if(vars_create(&rq->cookies));

    rq->http = http;

    *prq = rq;

    return 0;
err:
    if(rq->header)
        header_free(rq->header);
    if(rq)
        request_free(rq);
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rq    parameter \a rq description
 *  
 * \return
 *  - \c 0  always
 */
int request_free(request_t *rq)
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

    u_free(rq);

    return 0;
}

/**
 *          \}
 */

/**
 *  \}
 */ 
