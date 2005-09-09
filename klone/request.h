#ifndef _KLONE_REQUEST_H_
#define _KLONE_REQUEST_H_
#include <sys/types.h>
#include <klone/header.h>
#include <klone/io.h>
#include <klone/http.h>
#include <klone/vars.h>

struct request_s;
typedef struct request_s request_t;

int request_create(http_t *h, request_t **prq);
int request_free(request_t *rq);
int request_bind(request_t *rq, io_t *);
int request_parse(request_t *rq, 
        int (*is_valid_url)(void*, const char *, size_t),
        void* arg);

http_t* request_get_http(request_t *rq);

io_t* request_io(request_t *rq);

const char *request_get_uri(request_t *rq);
const char *request_get_filename(request_t *rq);
const char *request_get_resolved_filename(request_t *rq);
const char *request_get_query_string(request_t *rq);
const char *request_get_path_info(request_t *rq);
const char *request_get_resolved_path_info(request_t *rq);
int request_get_method(request_t *rq);
size_t request_get_content_length(request_t *rq);
time_t request_get_if_modified_since(request_t *rq);

int request_is_encoding_accepted(request_t *rq, const char *encoding);

int request_set_field(request_t *rq, const char *name, const char *value);
int request_set_uri(request_t *rq, const char *uri,
        int (*is_valid_uri)(void*, const char *, size_t),
        void* arg);
int request_set_filename(request_t *rq, const char *filename);
int request_set_method(request_t *rq, const char *method);
int request_set_path_info(request_t *rq, const char *path_info);
int request_set_query_string(request_t *rq, const char *query);
int request_set_resolved_filename(request_t *rq, const char *resolved);
int request_set_resolved_path_info(request_t *rq, const char *resolved);

void request_clear_uri(request_t *rq);

/* args */
vars_t *request_get_args(request_t *rq);
const char *request_get_arg(request_t *rq, const char *name);

/* cookies */
vars_t *request_get_cookies(request_t *rq);
const char *request_get_cookie(request_t *rq, const char *name);

int request_print(request_t *rq);

#endif

