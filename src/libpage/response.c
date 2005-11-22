#include <time.h>
#include <klone/response.h>
#include <klone/utils.h>
#include <klone/io.h>
#include <klone/codec.h>
#include <klone/http.h>
#include <klone/rsfilter.h>
#include <u/libu.h>

struct response_s
{
    http_t *http;           /* http server handle       */
    header_t *header;       /* output header            */
    io_t *io;               /* output stream            */
    int status;             /* http status code         */
    int method;             /* HTTP request method      */
};


/** 
 *  \ingroup Vhttp
 *  \{
 *          \defgroup response HTTP response handling
 *          \{
 *              \par 
 *              Basic knowledge of the HTTP protocol is assumed. Hence only the
 *              essential information is given. Some useful references are:
 *                - RFC 2616 for a complete description of HTTP 1.1 header fields
 *                - RFC 2109 for cookie format
 *                - RFC 822 for standard data type formats
 *                - http://www.iana.org/assignments/media-types/ for an updated
 *                list of possible mime-types
 */

/**
 * \brief   Set response content encoding field 
 * 
 * Set the \e Content-Encoding field in a response object \a rs to \a encoding.
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
 * \brief    Set the value of a cookie 
 *
 * Set the value of a cookie named \a name to \a value in response object \a rs.
 * Other fields that can be set are \a expire, \a path, \a domain, \a and secure.
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
        // FIXME buf may be too small....
        dbg_err_if(u_urlncpy(buf + strlen(buf), value, strlen(value), 
            URLCPY_ENCODE) <= 0);

        /* expiration date */
        if(expire)
        {
            dbg_err_if(u_tt_to_rfc822(date, expire, DATESZ));

            dbg_err_if(u_snprintf(buf + strlen(buf), BUFSZ - strlen(buf), 
                        "; expire=%s", date));
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
 * \brief   Print the status of a response object.
 *
 * A string representing the status of a response object \a rs is printed to \a
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
 * \brief   Print a response field.
 *
 * Print the name and value of a \a field of \a response to \a io.
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
    dbg_err_if(io_printf(io, "%s: %s\r\n", field->name, field->value) < 0);
    
    return 0;
err:
    return ~0;
}

/** 
 * \brief   Set the response method
 *  
 * Set the response method of \a rs to \a method. For possibile values of
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
 * \brief   Get the response method
 *  
 * Get the response method of \a rs.
 *
 * \param rs    response object
 *  
 * \return
 *  - the method of the given \a rs
 */
int response_get_method(response_t *rs)
{
    return rs->method;
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
        field =  header_get_fieldn(rs->header, i);
        sz += strlen(field_get_name(field));
        sz += strlen(field_get_value(field));
        sz += 4; /* blanks and new lines */
    }

    sz += 2; /* final \r\n */
    sz += 64; /* guard bytes */

    return sz;
}

/* 
 * \brief   Output a response header 
 *
 * Print the header of \a rs to \a io.
 *
 * \param rs    response object
 * \param io    output I/O object
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
 * \brief   Print a response header
 *  
 * Print the header of \a rs
 *
 * \param rs    parameter \a rs description
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
 * \brief   Set a field of a response object
 *
 * Set field \a name to \a value in reponse object \a rs.
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
 * \brief   Set the content type of a response to a mime type
 *
 * Set the \e Content-Type field of response \a rs to \a mime_type.
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
 * \brief   Set the date field in a response header
 *
 * Set the \e Date field of \a rs to \a date. 
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

    dbg_err_if(u_tt_to_rfc822(buf, date, BUFSZ));

    dbg_err_if(header_set_field(rs->header, "Date", buf));

    return 0;
err:
    return ~0;
}

/**
 * \brief   Set the last modified field in a response header
 *
 * Set the \e Last-Modified field of \a rs to \a mtime.
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

    dbg_err_if(u_tt_to_rfc822(buf, mtime, BUFSZ));

    dbg_err_if(header_set_field(rs->header, "Last-Modified", buf));

    return 0;
err:
    return ~0;
}

/**
 * \brief   Set the content length field of a response header
 *
 * Set the \e Content-Length field of \a rs to \a sz.
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
 * \brief   Get the status of a response
 *  
 * Get the status of a response \a rs.
 *
 * \param rs  response object
 *  
 * \return
 *  - the status of the given \a rs
 */
int response_get_status(response_t *rs)
{
    return rs->status;
}

/**
 * \brief   Get the header of a response
 *
 * Get the header of a response \a rs.
 *
 * \param rs    response object
 *
 * \return
 *  - the child header object of the given \a rs
 */
header_t* response_get_header(response_t *rs)
{
    return rs->header;
}

/**
 * \brief   Get the I/O object of a response
 *
 * Get the I/O object of reponse \a rs.
 *  
 * \param rs  response object
 *      
 * \return
 *  - the I/O child object of the given \a rs
 */
io_t* response_io(response_t *rs)
{
    return rs->io;
}

/** 
 * \brief   Redirect to a given url
 *  
 * Redirect to \e url by setting the \e Location field in response \a rs.
 *
 * \param rs    parameter \a rs description
 * \param url   parameter \a url description
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
 * \brief   Set the status of a response
 *  
 * Set the \a status of response \a rs. 
 *
 * \param rs      parameter \a rs description
 * \param status  parameter \a status description
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
 * \brief   Bind the response to a given I/O object
 *  
 * Bind response \a rs to I/O object \a out.
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
 * \brief   Create a response object
 *  
 * \param http  parameter \a http description
 * \param prs   parameter \a prs description
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

    u_free(rs);

    return 0;
}

/**
 *          \}
 */

/**
 *  \}
 */ 
