/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: request.h,v 1.22 2009/10/23 14:08:28 tho Exp $
 */

#ifndef _KLONE_REQUEST_H_
#define _KLONE_REQUEST_H_

#include <sys/types.h>
#include <time.h>
#include <u/libu.h>
#include <klone/header.h>
#include <klone/io.h>
#include <klone/http.h>
#include <klone/vars.h>
#include <klone/vhost.h>

#ifdef __cplusplus
extern "C" {
#endif

struct request_s;
typedef struct request_s request_t;

int request_create(http_t *h, request_t **prq);
int request_free(request_t *rq);
int request_bind(request_t *rq, io_t *);
int request_parse_header(request_t *rq, 
        int (*is_valid_url)(void*, const char *, size_t),
        void* arg);
int request_parse_data(request_t *rq);

io_t* request_io(request_t *rq);
http_t* request_get_http(request_t *rq);
const char *request_get_addr(request_t *rq);
const char *request_get_peer_addr(request_t *rq);
header_t* request_get_header(request_t *rq);
field_t* request_get_field(request_t *rq, const char *name);
const char* request_get_field_value(request_t *rq, const char *name);
const char *request_get_client_request(request_t *rq);
const char *request_get_uri(request_t *rq);
const char* request_get_protocol(request_t *rq);
const char *request_get_filename(request_t *rq);
const char *request_get_resolved_filename(request_t *rq);
const char *request_get_query_string(request_t *rq);
const char *request_get_path_info(request_t *rq);
const char *request_get_resolved_path_info(request_t *rq);
int request_get_method(request_t *rq);
ssize_t request_get_content_length(request_t *rq);
time_t request_get_if_modified_since(request_t *rq);
int request_is_encoding_accepted(request_t *rq, const char *encoding);

int request_set_field(request_t *rq, const char *name, const char *value);
int request_set_client_request(request_t *rq, const char *ln);
int request_set_uri(request_t *rq, const char *uri,
        int (*is_valid_uri)(void*, const char *, size_t),
        void* arg);
int request_set_filename(request_t *rq, const char *filename);
int request_set_method(request_t *rq, const char *method);
int request_set_path_info(request_t *rq, const char *path_info);
int request_set_query_string(request_t *rq, const char *query);
int request_set_resolved_filename(request_t *rq, const char *resolved);
int request_set_resolved_path_info(request_t *rq, const char *resolved);
int request_set_addr(request_t *rq, const char *addr);
int request_set_peer_addr(request_t *rq, const char *addr);
void request_set_cgi(request_t *rq, int cgi);
void request_clear_uri(request_t *rq);

int request_print(request_t *rq);

enum { MIME_TYPE_BUFSZ = 256 };

vars_t *request_get_uploads(request_t *rq);
int request_get_uploaded_file(request_t *rq, const char *name, size_t idx, 
    char local_filename[U_FILENAME_MAX], char client_filename[U_FILENAME_MAX], 
    char mime_type[MIME_TYPE_BUFSZ], size_t *file_size);

vars_t *request_get_args(request_t *rq);
const char *request_get_arg(request_t *rq, const char *name);

vars_t *request_get_getargs(request_t *rq);
const char *request_get_getarg(request_t *rq, const char *name);

vars_t *request_get_postargs(request_t *rq);
const char *request_get_postarg(request_t *rq, const char *name);

vars_t *request_get_cookies(request_t *rq);
const char *request_get_cookie(request_t *rq, const char *name);

vhost_t *request_get_vhost(request_t *rq);
int request_set_vhost(request_t *rq, vhost_t *vhost);

/* internal */
struct supplier_s;
void request_set_sup_info(request_t *, struct supplier_s*, void *, time_t);
void request_get_sup_info(request_t *, struct supplier_s **, void **, time_t *);

#ifdef __cplusplus
}
#endif 

#endif
