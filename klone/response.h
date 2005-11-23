#ifndef _KLONE_RESPONSE_H_
#define _KLONE_RESPONSE_H_

#include <klone/io.h>
#include <klone/header.h>
#include <klone/http.h>

/** \file */

/** HTTP response codes */
enum {
    HTTP_STATUS_EMPTY                     =   0, 
    /**< undefined status */
    HTTP_STATUS_OK                        = 200,
    /**< request succeeded */
    HTTP_STATUS_CREATED                   = 201,
    /**< fulfilled request resulting in creation of new resource */
    HTTP_STATUS_ACCEPTED                  = 202,
    /**< request accepted but processing not completed */
    HTTP_STATUS_NO_CONTENT                = 204,
    /**< no body returned */
    HTTP_STATUS_MOVED_PERMANENTLY         = 301,
    /**< resource relocated permanently */
    HTTP_STATUS_MOVED_TEMPORARILY         = 302,
    /**< resource relocated temporarily */
    HTTP_STATUS_NOT_MODIFIED              = 304,
    /**< GET request for unmodified document */
    HTTP_STATUS_BAD_REQUEST               = 400,
    /**< syntax error */
    HTTP_STATUS_UNAUTHORIZED              = 401,
    /**< user authentication required */
    HTTP_STATUS_FORBIDDEN                 = 403,
    /**< access to resource forbidden */
    HTTP_STATUS_NOT_FOUND                 = 404,
    /**< nothing found at matching request URI */
    HTTP_STATUS_INTERNAL_SERVER_ERROR     = 500,
    /**< unexpected condition caused an error */
    HTTP_STATUS_NOT_IMPLEMENTED           = 501,
    /**< request method not supported */
    HTTP_STATUS_BAD_GATEWAY               = 502,
    /**< invalid response while acting as gateway or proxy */
    HTTP_STATUS_SERVICE_UNAVAILABLE       = 503 
    /**< server unavailable due to temporary overloading or maintenance */
};

enum { COOKIE_MAX_SIZE = 4096 };

struct response_s;
typedef struct response_s response_t;

int response_create(http_t *http, response_t **prs);
int response_free(response_t *rs);
int response_bind(response_t *rs, io_t *);

int response_redirect(response_t *rs, const char *url);

int response_set_status(response_t *rs, int code);
int response_get_status(response_t *rs);
void response_set_method(response_t *rs, int method);
int response_get_method(response_t *rs);

int response_print_header(response_t *rs);
int response_print_header_to_io(response_t *rs, io_t *io);
size_t response_get_max_header_size(response_t *rs);

io_t* response_io(response_t *rs);

header_t* response_get_header(response_t *rs);

int response_set_field(response_t *rs, const char *name, const char *value);
int response_set_content_type(response_t *rs, const char *mime_type);
int response_set_content_length(response_t *rs, size_t sz);
int response_set_content_encoding(response_t *rs, const char *encoding);
int response_set_last_modified(response_t *rs, time_t mtime);
int response_set_date(response_t *rs, time_t now);

int response_set_cookie(response_t *rs, const char *name, const char *value,
    time_t expire, const char *path, const char *domain, int secure);


#endif
