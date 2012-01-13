/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: var.c,v 1.20 2008/05/16 15:04:47 tat Exp $
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <stdlib.h>
#include <u/libu.h>
#include <klone/var.h>
#include <klone/utils.h>
#include <klone/varprv.h>

/**
 * \ingroup vars
 * \brief   Get name u_string_t of a variable
 *
 * Return an u_string_t containing the name string of variable \p v.
 *
 * \param v   variable object
 *
 * \return the name string of \p v (may be \c NULL)
 */
u_string_t *var_get_name_s(var_t *v)
{
    dbg_return_if (v == NULL, NULL);

    return v->sname; /* may be NULL */
}

/**
 * \ingroup vars
 * \brief   Get u_string_t value of a variable
 *
 * Return an u_string_t containing the name string of variable \p v.
 *
 * \param v   variable object
 *
 * \return the value string of \p v (may be \c NULL)
 */
u_string_t *var_get_value_s(var_t *v)
{
    dbg_return_if (v == NULL, NULL);

    if(v->svalue == NULL)
        dbg_err_if(u_string_create(v->data, v->size, &v->svalue));
    
    return v->svalue;
err:
    return NULL;
}

void var_set_opaque(var_t *v, void *opaque)
{
    v->opaque = opaque;
}

void* var_get_opaque(var_t *v)
{
    return v->opaque;
}

int var_bin_create(const char *name, const unsigned char *data, size_t size, 
        var_t **pv)
{
    var_t *v = NULL;

    dbg_return_if (name == NULL, ~0);
    dbg_return_if (data == NULL, ~0);
    dbg_return_if (pv == NULL, ~0);

    v = u_zalloc(sizeof(var_t));
    dbg_err_if(v == NULL);

    dbg_err_if(u_string_create(name, strlen(name), &v->sname));

    dbg_err_if(var_set_bin_value(v, data, size));

    *pv = v;

    return 0;
err:
    if(v)
        var_free(v);
    return ~0;
}

int var_create(const char* name, const char *value, var_t**pv)
{
    dbg_return_if (name == NULL, ~0);
    dbg_return_if (value == NULL, ~0);

    return var_bin_create(name, (const unsigned char *) value, 
            1 + strlen(value), pv);
}

/*
 * \ingroup vars
 * \brief   Free a variable
 *
 * \return \c 0, always
 */
int var_free(var_t *v)
{
    if(v)
    {
        if(v->sname)
            u_string_free(v->sname);

        if(v->svalue)
            u_string_free(v->svalue);

        if(v->opaque)
            U_FREE(v->opaque);

        U_FREE(v->data);
        U_FREE(v);
    }

    return 0;
}

/**
 * \ingroup vars
 * \brief   Get the name of a variable
 *
 * Return a \c char* containing the name of variable \p v.
 *
 * \param v  variable object
 *
 * \return the name string of the given \p v (may be \c NULL)
 */
const char *var_get_name(var_t *v)
{
    dbg_return_if (v == NULL, NULL);

    return u_string_c(v->sname);
}

/**
 * \ingroup vars
 * \brief   Get the value of a variable
 *
 * Return a \c char* containing the value of variable \p v.
 *
 * \param v  variable object
 *
 * \return the value string of the given \p v (may be \c NULL)
 */
const char *var_get_value(var_t *v)
{
    dbg_return_if (v == NULL, NULL);

    return v->data;
}

/**
 * \ingroup vars
 * \brief   Get the size of a variable value
 * 
 * Return a size_t with the value size of variable \p v.
 * 
 * \param v   variable object
 *
 * \return the size of the variable value 
 */
size_t var_get_value_size(var_t *v)
{
    dbg_return_if (v == NULL, 0);   /* XXX should be (ssize_t) '-1' */

    return v->size;
}

/** 
 * \ingroup vars
 * \brief   Set the name and value of a variable
 *  
 * Set variable \p var to \p name and \p value.
 *
 * \param var   variable object
 * \param name  string name (null-terminated)
 * \param value string value (null-terminated)
 *  
 * \return \c 0 if successful, non-zero on error
 */ 
int var_set(var_t *var, const char *name, const char *value)
{
    dbg_err_if (var == NULL);
    dbg_err_if (name == NULL);
    dbg_err_if (value == NULL);
    
    dbg_err_if(var_set_name(var, name));
    dbg_err_if(var_set_value(var, value));

    return 0;
err:
    return ~0;
}

/**
 * \ingroup vars
 * \brief   Set the name of a variable
 *
 * Set the name of variable \p v
 *
 * \param v     variable object
 * \param name  variable name (null-terminated)
 *
 * \return \c 0 if successful, non-zero on error
 */
int var_set_name(var_t *v, const char *name)
{
    dbg_err_if (v == NULL);
    dbg_err_if (name == NULL);

    dbg_err_if(u_string_set(v->sname, name, strlen(name)));

    return 0; 
err:
    return ~0;
}

/**
 * \ingroup vars
 * \brief   Set the value of a variable
 *
 * Set the value of variable \p v to \p value
 *
 * \param v     variable object
 * \param value variable value (null-terminated)
 *
 * \return \c 0 if successful, non-zero on error
 */
int var_set_value(var_t *v, const char *value)
{
    dbg_return_if (v == NULL, ~0);
    dbg_return_if (value == NULL, ~0);

    /* copy the string and the trailing '\0' */
    return var_set_bin_value(v, (const unsigned char *) value, 
            1 + strlen(value));
}

/**
 * \ingroup vars
 * \brief   Set binary value of a variable
 *
 * Set binary value of variable \p v.
 *
 * \param v      variable object
 * \param data   value data
 * \param size   value size
 *
 * \return \c 0 if successful, non-zero on error
 */
int var_set_bin_value(var_t *v, const unsigned char *data, size_t size)
{
    dbg_err_if (v == NULL);
    dbg_err_if (data == NULL);
    
    U_FREE(v->data);

    if(data && size)
    {
        v->size = size;
        v->data = u_malloc(size+1);
        dbg_err_if(v->data == NULL);

        memcpy(v->data, data, size);
        v->data[size] = 0; /* zero-term v->data so it can be used as a string */
    } else {
        v->size = 0;
        v->data = NULL;
    }

    if(v->svalue)
        dbg_err_if(u_string_set(v->svalue, v->data, v->size));

    return 0; 
err:
    return ~0;
}
