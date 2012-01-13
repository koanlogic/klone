/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: var.h,v 1.8 2006/01/09 12:38:38 tat Exp $
 */

#ifndef _KLONE_VAR_H_
#define _KLONE_VAR_H_

#include <sys/types.h>
#include <u/libu.h>

#ifdef __cplusplus
extern "C" {
#endif

struct var_s;
typedef struct var_s var_t;

int var_create(const char* name, const char *value, var_t**);
int var_bin_create(const char* name, const unsigned char *data, size_t size, 
        var_t**);
int var_free(var_t*);

const char* var_get_name(var_t *v);
const char* var_get_value(var_t *v);
size_t var_get_value_size(var_t *v);

u_string_t* var_get_name_s(var_t *v);
u_string_t* var_get_value_s(var_t *v);

int var_set(var_t*, const char *name, const char *value);
int var_set_name(var_t *v, const char *name);
int var_set_value(var_t *v, const char *value);
int var_set_bin_value(var_t *v, const unsigned char *data, size_t size);

void var_set_opaque(var_t *v, void *blob);
void* var_get_opaque(var_t *v);

#ifdef __cplusplus
}
#endif 

#endif
