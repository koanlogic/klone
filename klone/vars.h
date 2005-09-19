#ifndef _KLONE_VARLIST_H_
#define _KLONE_VARLIST_H_
#include <klone/var.h>
#include <klone/str.h>

struct vars_s;
typedef struct vars_s vars_t;

typedef int (*vars_cb_t)(var_t*, void*);

int vars_create(vars_t ** pvs);
int vars_free(vars_t *vs);

int vars_add(vars_t *vs, var_t *v);
int vars_del(vars_t *vs, var_t *v);

/* str must be a 'name=value' string */
int vars_add_strvar(vars_t *vs, const char *str);

/* str must be a (possibly url-encoded) 'name=value' string */
int vars_add_urlvar(vars_t *vs, const char *str);

var_t* vars_getn(vars_t *vs, size_t i);
size_t vars_count(vars_t *vs);

var_t* vars_get(vars_t *vs, const char *name);
const char* vars_get_value(vars_t *vs, const char *name);

int vars_get_value_i(vars_t *vs, const char *name);
string_t* vars_get_value_s(vars_t *vs, const char *name);

/* get i-th variable called "name" */
var_t* vars_get_ith(vars_t *vs, const char *name, size_t ith);
const char* vars_get_ith_value(vars_t *vs, const char *name, size_t ith);
int vars_get_ith_value_i(vars_t *vs, const char *name, size_t ith);

void vars_foreach(vars_t *vs, int (*foreach)(var_t*, void*), void *arg);

#endif
