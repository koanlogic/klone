/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: vars.h,v 1.10 2008/04/18 17:31:11 tat Exp $
 */

#ifndef _KLONE_VARLIST_H_
#define _KLONE_VARLIST_H_

#include <u/libu.h>
#include <klone/var.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { 
    VARS_FLAG_NONE = 0,
    VARS_FLAG_FOREIGN = 1 << 0  /* don't free the list of var_t on vars_free */
};

struct vars_s;
typedef struct vars_s vars_t;

typedef int (*vars_cb_t)(var_t*, void*);

int vars_create(vars_t ** pvs);
int vars_free(vars_t *vs);

int vars_set_flags(vars_t *vs, int flags);

int vars_add(vars_t *vs, var_t *v);
int vars_del(vars_t *vs, var_t *v);

/* str must be a 'name=value' string */
int vars_add_strvar(vars_t *vs, const char *str);

/* str must be a (possibly url-encoded) 'name=value' string */
int vars_add_urlvar(vars_t *vs, const char *cstr, var_t **v);

var_t* vars_getn(vars_t *vs, size_t n);
size_t vars_count(vars_t *vs);

size_t vars_countn(vars_t *vs, const char *name);

void vars_foreach(vars_t *vs, int (*foreach)(var_t*, void*), void *arg);

/* get first variable called "name" */
var_t* vars_get(vars_t *vs, const char *name);
const char* vars_get_value(vars_t *vs, const char *name);
int vars_get_value_i(vars_t *vs, const char *name);
u_string_t* vars_get_value_s(vars_t *vs, const char *name);

/* get i-th variable called "name" */
var_t* vars_geti(vars_t *vs, const char *name, size_t ith);
const char* vars_geti_value(vars_t *vs, const char *name, size_t ith);
int vars_geti_value_i(vars_t *vs, const char *name, size_t ith);
u_string_t* vars_geti_value_s(vars_t *vs, const char *name, size_t ith);

#ifdef __cplusplus
}
#endif 

#endif
