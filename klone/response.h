/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: response.h,v 1.12 2007/10/22 15:49:47 tat Exp $
 */

#ifndef _KLONE_RESPONSE_H_
#define _KLONE_RESPONSE_H_

#include <klone/io.h>
#include <klone/header.h>
#include <klone/http.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \file */

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

int response_enable_caching(response_t *rs);
int response_disable_caching(response_t *rs);

void response_set_cgi(response_t *rs, int cgi);

int response_print_header(response_t *rs);
int response_print_header_to_io(response_t *rs, io_t *io);
size_t response_get_max_header_size(response_t *rs);

io_t* response_io(response_t *rs);

header_t* response_get_header(response_t *rs);

field_t* response_get_field(response_t *rs, const char *name);
const char* response_get_field_value(response_t *rs, const char *name);

int response_set_field(response_t *rs, const char *name, const char *value);
int response_set_content_type(response_t *rs, const char *mime_type);
int response_set_content_length(response_t *rs, size_t sz);
int response_set_content_encoding(response_t *rs, const char *encoding);
int response_set_last_modified(response_t *rs, time_t mtime);
int response_set_date(response_t *rs, time_t now);
int response_del_field(response_t *rs, const char *name);

int response_set_cookie(response_t *rs, const char *name, const char *value,
    time_t expire, const char *path, const char *domain, int secure);

#ifdef __cplusplus
}
#endif 

#endif
