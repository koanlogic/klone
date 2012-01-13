/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: http.h,v 1.14 2007/11/09 22:06:26 tat Exp $
 */

#ifndef _KLONE_HTTP_H_
#define _KLONE_HTTP_H_

#include <u/libu.h>

#ifdef __cplusplus
extern "C" {
#endif 

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
    /**< request timeout */
    HTTP_STATUS_REQUEST_TIMEOUT           = 408,
    /**< nothing found at matching request URI */
    HTTP_STATUS_LENGTH_REQUIRED           = 411,
    /**< missing Content-Length header field */
    HTTP_STATUS_REQUEST_TOO_LARGE         = 413,
    /**< decryption key is needed - KLone extension */
    HTTP_STATUS_EXT_KEY_NEEDED            = 430,
    /**< request PDU too big */
    HTTP_STATUS_INTERNAL_SERVER_ERROR     = 500,
    /**< unexpected condition caused an error */
    HTTP_STATUS_NOT_IMPLEMENTED           = 501,
    /**< request method not supported */
    HTTP_STATUS_BAD_GATEWAY               = 502,
    /**< invalid response while acting as gateway or proxy */
    HTTP_STATUS_SERVICE_UNAVAILABLE       = 503 
    /**< server unavailable due to temporary overloading or maintenance */
};

/** HTTP Methods */
enum http_method_e
{ 
    HM_UNKNOWN,   /**< unknown value */
    HM_GET,       /**< retrieve data at URI */
    HM_HEAD,      /**< ~HM_GET with headers only */
    HM_POST,      /**< create new object subordinate to specified object */
    HM_PUT,       /**< data in body is to be stored under URL */
    HM_DELETE     /**< deletion request at given URL */
};

struct http_s;
typedef struct http_s http_t;

struct session_opt_s;
struct request_s;
struct vhost_s;

u_config_t *http_get_config(http_t* http);
struct session_opt_s *http_get_session_opt(http_t* http);

struct vhost_s* http_get_vhost(http_t *h, struct request_s *rq);
struct vhost_list_s* http_get_vhost_list(http_t *h);
int http_alias_resolv(http_t *h, struct request_s *rq, char *dst, 
        const char *uri, size_t sz);
const char* http_get_status_desc(int status);

#ifdef __cplusplus
}
#endif 

#endif
