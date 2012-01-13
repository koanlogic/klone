/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: response.c,v 1.30 2008/07/25 09:38:56 tho Exp $
 */

#include "klone_conf.h"
#include <time.h>
#include <u/libu.h>
#include <klone/response.h>
#include <klone/utils.h>
#include <klone/io.h>
#include <klone/codec.h>
#include <klone/http.h>
#include <klone/rsfilter.h>

struct response_s
{
    http_t *http;           /* http server handle               */
    header_t *header;       /* output header                    */
    io_t *io;               /* output stream                    */
    int status;             /* http status code                 */
    int method;             /* HTTP request method              */
    int cgi;                /* if we're running in cgi context  */
};

/**
 * \ingroup response
 * \brief   Set response content encoding field 
 * 
 * Set the \e Content-Encoding field in a response object \p rs to \p encoding.
 * 
 * \param rs        response object
 * \param encoding  encoding type 
 *
 * \return 
 *  - \c 0 if successful
 *  - \c ~0 if successful
 */
int response_set_content_encoding(response_t *rs, const char *encoding)
{
    dbg_err_if(encoding == NULL);

    dbg_err_if(header_set_field(rs->header, "Content-Encoding", encoding));

    return 0;
err:
    return ~0;
}

/**
 * \ingroup response
 * \brief   Add all header field that enable page caching (i.e. disable caching)
 * 
 * Adds all relevant Header fields to the current HTTP response to avoid 
 * browser caching. 
 *
 * The function will set/modify the following fields:
 *
 *    Cache-Control: no-cache, must-revalidate 
 *    Expires: Mon, 1 Jan 1990 05:00:00 GMT
 *    Pragma: no-cache
 * 
 * \param rs        response object
 *
 * \return 
 *  - \c 0 if successful
 *  - \c ~0 if successful
 */
int response_disable_caching(response_t *rs)
{
    dbg_err_if(response_set_field(rs, "Cache-Control", 
        "no-cache, must-revalidate"));

    dbg_err_if(response_set_field(rs, "Expires", 
        "Mon, 1 Jan 1990 05:00:00 GMT"));

    dbg_err_if(response_set_field(rs, "Pragma", "no-cache"));

    return 0;
err:
    return ~0;
}

/**
 * \ingroup response
 * \brief   Remove all headers that inhibit page caching
 * 
 * Remove all HTTP Header fields that (should) prevent browsers caching. This
 * should enable caching on specs-compliant browsers.
 *
 * Those fields are:
 *
 *    Cache-Control: 
 *    Expires: 
 *    Pragma: 
 * 
 * \param rs        response object
 *
 * \return 
 *  - \c 0 if successful
 *  - \c ~0 if successful
 */
int response_enable_caching(response_t *rs)
{
    dbg_err_if(response_del_field(rs, "Cache-Control"));

    dbg_err_if(response_del_field(rs, "Expires"));

    dbg_err_if(response_del_field(rs, "Pragma"));

    return 0;
err:
    return ~0;
}

/**
 * \ingroup response
 * \brief    Set the value of a cookie 
 *
 * Set the value of a cookie named \p name to \p value in response object \p rs.
 * Other fields that can be set are \p expire, \p path, \p domain, \p and secure.
 *  
 * \param rs        response object
 * \param name      cookie name
 * \param value     cookie value
 * \param expire    cookie expiration date
 * \param path      cookie path
 * \param domain    cookie domain
 * \param secure    cookie secure flag
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_set_cookie(response_t *rs, const char *name, const char *value,
    time_t expire, const char *path, const char *domain, int secure)
{
    enum { BUFSZ = 4096, DATESZ = 64 };
    field_t *field = NULL;
    char buf[BUFSZ], date[DATESZ];

    if(value == NULL)
    {   /* delete this cookie */
        dbg_err_if(u_snprintf(buf, BUFSZ, 
            "%s=; expires=Wed, 01-Jan-1990 10:10:10 GMT", name));
    } else {
        /* name */
        dbg_err_if(u_snprintf(buf, BUFSZ, "%s=", name));

        /* encoded value */
        dbg_err_if(u_urlncpy(buf + strlen(buf), value, strlen(value), 
            URLCPY_ENCODE) <= 0);

        /* expiration date */
        if(expire)
        {
            dbg_err_if(u_tt_to_rfc822(date, expire));

            dbg_err_if(u_snprintf(buf + strlen(buf), BUFSZ - strlen(buf), 
                        "; expires=%s", date));
        }

        /* path */
        if(path)
            dbg_err_if(u_snprintf(buf + strlen(buf), 
                        BUFSZ - strlen(buf), "; path=%s", path));

        /* domain */
        if(domain)
            dbg_err_if(u_snprintf(buf + strlen(buf), 
                        BUFSZ - strlen(buf), "; domain=%s", domain));
        /* secure flag */
        if(secure)
            dbg_err_if(u_snprintf(buf + strlen(buf), 
                        BUFSZ - strlen(buf), "; secure"));

    }

    dbg_err_if(field_create("Set-Cookie", buf, &field));

    dbg_err_if(header_add_field(rs->header, field));

    return 0;
err:
    if(field)
        field_free(field);
    return ~0;
}

/*
 * \ingroup response
 * \brief   Print the status of a response object.
 *
 * A string representing the status of a response object \p rs is printed to \p
 * io. 
 *
 * \param rs      response object
 * \param io      output I/O object
 *
 * \return
 *  - \c 0 if successful
 *  - \c ~0 on error
 */
static int response_print_status(response_t *rs, io_t *io)
{
    dbg_err_if(io_printf(io, "HTTP/1.0 %d %s\r\n", rs->status, 
        http_get_status_desc(rs->status)) < 0);

    return 0;
err:
    return ~0;
}

/*
 * \ingroup response
 * \brief   Print a response field.
 *
 * Print the name and value of a \p field of \p response to \p io.
 *
 * \param rs      response object
 * \param io      output I/O object
 * \param field   field to be printed
 *
 * \return
 *  - \c 0 if successful
 *  - \c ~0 on error
 */
static int response_print_field(response_t *rs, io_t *io, field_t *field)
{
    u_unused_args(rs);

    dbg_err_if(io_printf(io, "%s: %s\r\n", field->name, field->value) < 0);
    
    return 0;
err:
    return ~0;
}

/** 
 * \ingroup response
 * \brief   Set the response method
 *  
 * Set the response method of \p rs to \p method. For possible values of
 * method, refer to http.h.
 *
 * \param rs      response object
 * \param method  response method
 *  
 * \return
 *  - nothing
 */
void response_set_method(response_t *rs, int method)
{
    rs->method = method;
}

/** 
 * \ingroup response
 * \brief   Get the response method
 *  
 * Get the response method of \p rs. For possibile values of method, refer to
 * http.h.
 *
 * \param rs    response object
 *  
 * \return
 *  - the method of the given \p rs
 */
int response_get_method(response_t *rs)
{
    return rs->method;
}


/* set is-cgi flag */
void response_set_cgi(response_t *rs, int cgi)
{
    rs->cgi = cgi;
    return ;
}

/* calculate the approx max value of the current header (useful to alloc a
 * buffer big enough) */
size_t response_get_max_header_size(response_t *rs)
{
    field_t *field;
    int i, n;
    size_t sz = 0;

    /* calc status line length */
    sz += 16; /* http/x.y nnn[n] \r\n */
    sz += strlen(http_get_status_desc(rs->status));

    n = header_field_count(rs->header);
    for(i = 0; i < n; ++i)
    {
        const char *p;

        field =  header_get_fieldn(rs->header, i);
        sz += ((p = field_get_name(field)) != NULL) ? strlen(p) : 0;
        sz += ((p = field_get_value(field)) != NULL) ? strlen(p) : 0;
        sz += 4; /* blanks and new lines */
    }

    sz += 2; /* final \r\n */
    sz += 64; /* guard bytes */

    return sz;
}

/* 
 * \ingroup response
 * \brief   Output a response header 
 *
 * Print the header of \p rs to \p io.
 *
 * \param rs        response object
 * \param io        output I/O object
 * 
 * \return
 *  - \c 0 if successful
 *  - \c ~0 on error
 */
int response_print_header_to_io(response_t *rs, io_t *io)
{
    int i, n;

    dbg_err_if(io == NULL);

    /* print status line */
    if(!rs->cgi)
        dbg_err_if(response_print_status(rs, io));

    /* print field list */
    n = header_field_count(rs->header);
    for(i = 0; i < n; ++i)
        dbg_err_if(response_print_field(rs, io,
            header_get_fieldn(rs->header, i)));

    dbg_err_if(io_printf(io, "\r\n") < 0);

    return 0;
err:
    return ~0;
}

/** 
 * \ingroup response
 * \brief   Print a response header
 *  
 * Print the header of \p rs
 *
 * \param rs        parameter \p rs description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_print_header(response_t *rs)
{
    return response_print_header_to_io(rs, rs->io);
}


/**
 * \ingroup response
 * \brief   Set an header field of a response object
 *
 * Set field \p name to \p value in reponse object \p rs.
 *
 * \param rs     response object
 * \param name   field name
 * \param value  field value
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_set_field(response_t *rs, const char *name, const char *value)
{
    return header_set_field(rs->header, name, value);
}


/**
 * \ingroup response
 * \brief   Remove an header field of a response object
 *
 * Remove the header field whose name is \p name
 *
 * \param rs     response object
 * \param name   field name
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_del_field(response_t *rs, const char *name)
{
    field_t *f = NULL;
    
    f = header_get_field(rs->header, name);
    dbg_err_if(f == NULL);

    /* field found, delete it */
    dbg_err_if(header_del_field(rs->header, f));

    field_free(f);

    return 0;
err:
    return ~0;
}

/**
 * \ingroup response
 * \brief   Set the content type of a response to a mime type
 *
 * Set the \e Content-Type field of response \p rs to \p mime_type.
 *
 * \param rs         response object
 * \param mime_type  mime type
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_set_content_type(response_t *rs, const char *mime_type)
{
    dbg_err_if(mime_type == NULL);

    dbg_err_if(header_set_field(rs->header, "Content-Type", mime_type));

    return 0;
err:
    return ~0;
}

/**
 * \ingroup response
 * \brief   Set the date field in a response header
 *
 * Set the \e Date field of \p rs to \p date. 
 *
 * \param rs    response object
 * \param date  date value
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_set_date(response_t *rs, time_t date)
{
    enum { BUFSZ = 64 };
    char buf[BUFSZ];

    dbg_err_if(u_tt_to_rfc822(buf, date));

    dbg_err_if(header_set_field(rs->header, "Date", buf));

    return 0;
err:
    return ~0;
}

/**
 * \ingroup response
 * \brief   Set the last modified field in a response header
 *
 * Set the \e Last-Modified field of \p rs to \p mtime.
 *
 * \param rs     response object
 * \param mtime  last modified date value
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_set_last_modified(response_t *rs, time_t mtime)
{
    enum { BUFSZ = 64 };
    char buf[BUFSZ];

    dbg_err_if(u_tt_to_rfc822(buf, mtime));

    dbg_err_if(header_set_field(rs->header, "Last-Modified", buf));

    return 0;
err:
    return ~0;
}

/**
 * \ingroup response
 * \brief   Set the content length field of a response header
 *
 * Set the \e Content-Length field of \p rs to \p sz.
 *
 * \param rs  response object
 * \param sz  number of bytes in content
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_set_content_length(response_t *rs, size_t sz)
{
    enum { BUFSZ = 64 };
    char buf[BUFSZ];

    dbg_err_if(u_snprintf(buf, BUFSZ, "%u", sz));

    dbg_err_if(header_set_field(rs->header, "Content-Length", buf));

    return 0;
err:
    return ~0;
}

/** 
 * \ingroup response
 * \brief   Get the status of a response
 *  
 * Get the status of a response \p rs. For possible values of status refer to
 * response.h.
 *
 * \param rs  response object
 *  
 * \return
 *  - the status of the given \p rs
 */
int response_get_status(response_t *rs)
{
    return rs->status;
}

/**
 * \ingroup response
 * \brief   Get the header of a response
 *
 * Get the header of a response \p rs.
 *
 * \param rs    response object
 *
 * \return
 *  - the child header object of the given \p rs
 */
header_t* response_get_header(response_t *rs)
{
    return rs->header;
}

/**
 * \ingroup response
 * \brief   Get the I/O object of a response
 *
 * Get the I/O object of reponse \p rs.
 *  
 * \param rs  response object
 *      
 * \return
 *  - the I/O child object of the given \p rs
 */
io_t* response_io(response_t *rs)
{
    return rs->io;
}

/** 
 * \ingroup response
 * \brief   Redirect to a given url
 *  
 * Redirect to \e url by setting the \e Location field in response \p rs.
 *
 * \param rs    parameter \p rs description
 * \param url   parameter \p url description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_redirect(response_t *rs, const char *url)
{
    field_t *field;

    /* send Location: with redirect status code */
    response_set_status(rs, HTTP_STATUS_MOVED_TEMPORARILY);

    dbg_err_if(field_create("Location", url, &field));

    header_add_field(rs->header, field); 

    return 0;
err:
    return ~0;
}

/** 
 * \ingroup response
 * \brief   Set the status of a response
 *  
 * Set the \p status of response \p rs. For possible values of status
 * refer to response.h.
 *
 * \param rs      parameter \p rs description
 * \param status  parameter \p status description
 *  
 * \return
 *  - \c 0  always
 */
int response_set_status(response_t *rs, int status)
{
    rs->status = status;

    return 0;
}

/*
 * \ingroup response
 * \brief   Bind the response to a given I/O object
 *  
 * Bind response \p rs to I/O object \p out.
 *
 * \param rs     
 * \param out  output I/O object
 *  
 * \return
 *  - \c 0  always
 */
int response_bind(response_t *rs, io_t *out)
{
    rs->io = out;

    return 0;
}

/*
 * \ingroup response
 * \brief   Create a response object
 *  
 * \param http  parameter \p http description
 * \param prs   parameter \p prs description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_create(http_t *http, response_t **prs)
{
    response_t *rs = NULL;

    rs = u_zalloc(sizeof(response_t));
    dbg_err_if(rs == NULL);

    dbg_err_if(header_create(&rs->header));

    rs->http = http;

    *prs = rs;

    return 0;
err:
    if(rs->header)
        header_free(rs->header);
    if(rs)
        response_free(rs);
    return ~0;
}

/*
 * \ingroup response
 * \brief   Free a response object
 *  
 * \param rs response object
 *  
 * \return
 *  - \c 0  always
 */
int response_free(response_t *rs)
{
    if(rs->io)
        io_free(rs->io);

    if(rs->header)
        header_free(rs->header);

    U_FREE(rs);

    return 0;
}


/* return a field obj of the field named 'name' or NULL if the field does not 
   exist */
field_t* response_get_field(response_t *rs, const char *name)
{
    dbg_return_if (rs == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    return header_get_field(rs->header, name);
}

/* return the string value of the field named 'name' or NULL if the field does
   not exist */
const char* response_get_field_value(response_t *rs, const char *name)
{
    dbg_return_if (rs == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    return header_get_field_value(rs->header, name);
}
