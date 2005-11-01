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
 */

/**
 *          \defgroup response HTTP response handling
 *          \{
 *              \par
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
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rs        parameter \a rs description
 * \param name      parameter \a name description
 * \param value     parameter \a value description
 * \param expire    parameter \a expire description
 * \param path      parameter \a path description
 * \param domain    parameter \a domain description
 * \param secure    parameter \a secure description
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


static int response_print_status(response_t *rs, io_t *io)
{
    dbg_err_if(io_printf(io, "HTTP/1.0 %d %s\r\n", rs->status, 
        http_get_status_desc(rs->status)) < 0);

    return 0;
err:
    return ~0;
}

static int response_print_field(response_t *rs, io_t *io, field_t *field)
{
    dbg_err_if(io_printf(io, "%s: %s\r\n", field->name, field->value) < 0);
    
    return 0;
err:
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rs      parameter \a rs description
 * \param method  parameter \a method description
 *  
 * \return
 *  - nothing
 */
void response_set_method(response_t *rs, int method)
{
    rs->method = method;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rs    parameter \a rs description
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
 * \brief   One line description
 *  
 * Detailed function descrtiption.
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rs     parameter \a rs description
 * \param name   parameter \a name description
 * \param value  parameter \a value description
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rs         parameter \a rs description
 * \param mime_type  parameter \a mime_type description
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rs    parameter \a rs description
 * \param date  parameter \a date description
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rs     parameter \a rs description
 * \param mtime  parameter \a mtime description
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rs  parameter \a rs description
 * \param sz  parameter \a sz description
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
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rs  parameter \a rs description
 *  
 * \return
 *  - the status of the given \a rs
 */
int response_get_status(response_t *rs)
{
    return rs->status;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param rs    parameter \a rs description
 *
 * \return
 *  - the child header object of the given \a rs
 */
header_t* response_get_header(response_t *rs)
{
    return rs->header;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *  
 * \param rs  parameter \a rs description
 *      
 * \return
 *  - the io child object of the given \a rs
 */
io_t* response_io(response_t *rs)
{
    return rs->io;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
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
 * \brief   One line description
 *  
 * Detailed function descrtiption.
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

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rs   parameter \a rs description
 * \param out  parameter \a out description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int response_bind(response_t *rs, io_t *out)
{
    rs->io = out;

    return 0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
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

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param rs parameter \a rs description
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
