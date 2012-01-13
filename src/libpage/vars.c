/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: vars.c,v 1.32 2008/05/16 15:04:47 tat Exp $
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <stdlib.h>
#include <u/libu.h>
#include <klone/vars.h>
#include <klone/varprv.h>
#include <klone/utils.h>

TAILQ_HEAD(var_list_s, var_s);

struct vars_s
{
    u_list_t *list;              /* list of variables (var_t) */
    int flags;
};

/**
 * \ingroup vars
 * \brief   Get \c u_string_t value of a variable
 *
 * Return an \c u_string_t containing the value of variable with \p name in
 * variable list \p vs.
 *  
 * \param vs    variable list
 * \param name  name of variable
 *      
 * \return the variable value (may be \c NULL)
 */     
u_string_t *vars_get_value_s(vars_t *vs, const char *name)
{
    var_t *v = NULL;

    dbg_err_if (vs == NULL);
    dbg_err_if (name == NULL);

    dbg_err_if((v = vars_get(vs, name)) == NULL);

    return var_get_value_s(v);
err:
    return NULL;
}

int vars_create(vars_t **pvs)
{
    vars_t *vs = NULL;

    dbg_err_if (pvs == NULL);

    vs = u_zalloc(sizeof(vars_t));
    dbg_err_if(vs == NULL);

    dbg_err_if(u_list_create(&vs->list));

    *pvs = vs;

    return 0;
err:
    if(vs)
        vars_free(vs);
    return ~0;
}

int vars_set_flags(vars_t *vs, int flags)
{
    dbg_err_if (vs == NULL);

    vs->flags = flags;

    return 0;
err:
    return ~0;
}

int vars_free(vars_t *vs)
{
    var_t *v;
    int t;

    if(vs)
    {
        /* if the var_t objects are owned by this vars_t then free them all */
        if((vs->flags & VARS_FLAG_FOREIGN) == 0)
        {
            /* free all variables */
            for(t = 0; (v = u_list_get_n(vs->list, t)) != NULL; ++t)
                var_free(v);
        }

        if(vs->list)
            u_list_free(vs->list);

        U_FREE(vs);
    }

    return 0;
}

int vars_add(vars_t *vs, var_t *v)
{
    dbg_err_if(vs == NULL);
    dbg_err_if(v == NULL);

    dbg_err_if(u_list_add(vs->list, v));

    return 0;
err:
    return ~0;
}

int vars_del(vars_t *vs, var_t *v)
{
    dbg_err_if(vs == NULL);
    dbg_err_if(v == NULL);
   
    dbg_err_if(u_list_del(vs->list, v));

    return 0;
err:
    return ~0;
}

/**
 * \ingroup vars
 * \brief   Get ith variable
 *
 * Return the \c var_t at index \p i in list \p vs.
 *
 * \param vs  variable list
 * \param i   index
 *
 * \return the ith variable or \c NULL if there's no ith var in \p vs
 */
var_t *vars_getn(vars_t *vs, size_t i)
{
    dbg_return_if (vs == NULL, NULL);
    nop_return_if (i >= u_list_count(vs->list), NULL);

    return u_list_get_n(vs->list, i);
}

/**
 * \ingroup vars
 * \brief   Count the number of variables
 *
 * Return a the number of variables in a list
 *
 * \param vs  variable list
 *
 * \return the number of elements in \p vs
 */
size_t vars_count(vars_t *vs)
{
    dbg_return_if (vs == NULL, 0);

    return u_list_count(vs->list);
}

/**
 * \ingroup vars
 * \brief   Count the number of variables with given name
 *
 * Return a the number of variables in a list with given name \p name
 *
 * \param vs    variable list
 * \param name  name of the variables to count
 *
 * \return the number of elements in \p vs whose name is \p name
 */
size_t vars_countn(vars_t *vs, const char *name)
{
    var_t *v;
    size_t c = 0;
    int t;

    dbg_return_if (vs == NULL || name == NULL, 0);

    for(t = 0; (v = u_list_get_n(vs->list, t)) != NULL; ++t)
    {
        if(strcasecmp(u_string_c(v->sname), name) == 0)
            c++;
    }

    return c;
}

/**
 * \ingroup vars
 * \brief   Add an URL variable
 *
 * Parse the "name=value" string \p cstr, url-decode name and value and push 
 * it into \p vs.  The variable is returned at \p *v.
 *
 * \param vs    variables' list where the variable is pushed
 * \param cstr  URL string to parse
 * \param v     the generated variable as a value-result argument
 *
 * \return \c 0 if successful, non-zero on error
 */
int vars_add_urlvar(vars_t *vs, const char *cstr, var_t **pv)
{
    enum { NAMESZ = 256, VALSZ = 4096 };
    char sname[NAMESZ], svalue[VALSZ];
    char *val, *str = NULL, *name = sname, *value = svalue;
    var_t *var = NULL;
    ssize_t vsz;
    size_t val_len, nam_len;

    dbg_return_if (vs == NULL, ~0);
    dbg_return_if (cstr == NULL, ~0);
    /* pv may be NULL */
        
    /* dup the string so we can modify it */
    dbg_err_sif ((str = u_strdup(cstr)) == NULL);

    /* zero-term the name part and set the value pointer */
    dbg_err_if ((val = strchr(str, '=')) == NULL);
    *val++ = '\0'; 

    val_len = strlen(val);
    nam_len = strlen(str);

    /* if the buffer on the stack is too small alloc a bigger one */
    if (nam_len >= NAMESZ)
        dbg_err_if ((name = u_zalloc(1 + nam_len)) == NULL);

    /* if the buffer on the stack is too small alloc a bigger one */
    if (val_len >= VALSZ)
        dbg_err_if ((value = u_zalloc(1 + val_len)) == NULL);

    /* url-decode var name */
    dbg_err_if (u_urlncpy(name, str, nam_len, URLCPY_DECODE) <= 0);

    /* url-decode var value */
    if (val_len)
        dbg_err_if ((vsz = u_urlncpy(value, val, val_len, URLCPY_DECODE)) <= 0);
    else 
    {
        /* an empty value */
        value[0] = '\0';
        vsz = 1;
    }

    /* u_dbg("name: [%s]  value: [%s]", name, value); */

    dbg_err_if (var_bin_create(name, (unsigned char *) value, vsz, &var));

    /* push into the var list */
    dbg_err_if (vars_add(vs, var));

    if (pv)
        *pv = var;

    /* if the buffer has been alloc'd on the heap then free it */
    if (value && value != svalue)
        u_free(value);

    if (name && name != sname)
        u_free(name);

    u_free(str);

    return 0;
err:
    if (value && value != svalue)
        u_free(value);
    if (name && name != sname)
        u_free(name);
    u_free(str);
    if (var)
        var_free(var);
    return ~0;
}

int vars_add_strvar(vars_t *vs, const char *str)
{
    char *eq, *dups = NULL;
    var_t *var = NULL;

    dbg_err_if (vs == NULL);
    dbg_err_if (str == NULL);

    /* dup the string (str is const) */
    dups = u_strdup(str);
    dbg_err_if(dups == NULL);

    /* search the '=' and replace it with a '\0' */
    eq = strchr(dups, '=');
    dbg_err_if(eq == NULL);
    *eq = 0;

    /* create a new var obj */
    dbg_err_if(var_create(dups, eq+1, &var));

    U_FREE(dups);

    /* push into the cookie list */
    dbg_err_if(vars_add(vs, var));

    return 0;
err:
    U_FREE(dups);
    if(var)
        var_free(var);
    return ~0;
}

/**
 * \ingroup vars
 * \brief   Get i-th variable with given name
 *
 * Return the \c var_t object at index \p i with name \p var_name in list \p vs.
 *
 * \param vs        variable list
 * \param var_name  variable name
 * \param i         index
 *
 * \return
 *  - the \c var_t object found
 *  - \c NULL if there's no i-th variable called \p var_name in \p vs
 */
var_t *vars_geti(vars_t *vs, const char *var_name, size_t i)
{
    var_t *v;
    int t;

    dbg_goto_if (vs == NULL, notfound);
    dbg_goto_if (var_name == NULL, notfound);

    for(t = 0; (v = u_list_get_n(vs->list, t)) != NULL; ++t)
    {
        if(strcasecmp(u_string_c(v->sname), var_name) == 0)
        {
            if(i-- == 0)
                return v;
        }
    }

notfound:
    return NULL;
}

/**
 * \ingroup vars
 * \brief   Get a variable with given name
 *
 * Return a \c var_t object with name \p name in list \p vs.
 *
 * \param vs        variable list
 * \param var_name  variable name
 *
 * \return
 *  - the \c var_t object found
 *  - \c NULL if there's no variable called \p var_name in \p vs
 */
var_t *vars_get(vars_t *vs, const char *var_name)
{
    dbg_return_if (vs == NULL, NULL);
    dbg_return_if (var_name == NULL, NULL);

    return vars_geti(vs, var_name, 0);
}

/**
 * \ingroup vars
 * \brief   Get the integer value of a variable with a given name and index
 *
 * Get the integer value of the variable with name \p name and index \p ith 
 * in list \p vs.
 *
 * \param vs    variable list
 * \param name  variable name
 * \param ith   index 
 *
 * \return
 *  - the integer value of \p name
 *  - \c 0 if no value could be found
 */
int vars_geti_value_i(vars_t *vs, const char *name, size_t ith)
{
    const char *v;

    dbg_return_if (vs == NULL, 0);
    dbg_return_if (name == NULL, 0);

    v = vars_geti_value(vs, name, ith);
    if(v == NULL)
        return 0;
    else
        return atoi(v);
}

/**
 * \ingroup vars
 * \brief   Get \c u_string_t value of i-th variable
 *
 * Return an \c u_string_t containing the value of i-th variable with \p name in
 * variable list \p vs.
 *  
 * \param vs    variable list
 * \param name  name of variable
 * \param ith   index
 *      
 * \return the variable value (may be \c NULL)
 */     
u_string_t *vars_geti_value_s(vars_t *vs, const char *name, size_t ith)
{
    var_t *v = NULL;

    dbg_err_if (vs == NULL);
    dbg_err_if (name == NULL);

    dbg_err_if((v = vars_geti(vs, name, ith)) == NULL);

    return var_get_value_s(v);
err:
    return NULL;
}

/**
 * \ingroup vars
 * \brief   Get the integer value of a variable with a given name.
 *
 * Return the integer value of the variable with name \p name in list \p vs.
 *
 * \param vs    variable list
 * \param name  variable name
 *
 * \return
 *  - the integer value of \p name
 *  - \c 0 if no value could be found
 */
int vars_get_value_i(vars_t *vs, const char *name)
{
    dbg_return_if (vs == NULL, 0);
    dbg_return_if (name == NULL, 0);

    return vars_geti_value_i(vs, name, 0);
}

/**
 * \ingroup vars
 * \brief   Get the value of the variable at a given index. 
 *
 * Return the string value of the variable with name \p name and index \p ith 
 * in list \p vs.
 *
 * \param vs    variable list that is scanned
 * \param name  variable name to search
 * \param ith   index of the searched variable
 *
 * \return
 *  - the value string corresponding to \p name at i-th position
 *  - \c NULL if no value could be found 
 */
const char *vars_geti_value(vars_t *vs, const char *name, size_t ith)
{
    var_t *v;

    dbg_return_if (vs == NULL, NULL);
    dbg_return_if (name == NULL, NULL);
    
    v = vars_geti(vs, name, ith);

    return  v ? var_get_value(v) : NULL;
}

/**
 * \ingroup vars
 * \brief   Get the value of the variable with given name.
 *
 * Return the string value of the variable with name \p name in list \p vs.
 *
 * \param vs    variable list that is scanned
 * \param name  variable name to search
 *
 * \return
 *  - the value string corresponding to \p name
 *  - \c NULL if no value could be found
 */
const char *vars_get_value(vars_t *vs, const char *name)
{
    dbg_return_if (vs == NULL, NULL);
    dbg_return_if (name == NULL, NULL);

    return vars_geti_value(vs, name, 0);
}

/**
 * \ingroup vars
 * \brief   Execute a function on a list of variables
 *
 * Execute function \p cb with optional arguments \p arg on all variables 
 * in list \p vs
 *
 * \param vs    variable list
 * \param cb    function to be called on each variable (see prototype)
 * \param arg   argument to \p cb
 *
 * \return nothing
 */
void vars_foreach(vars_t *vs, int (*cb)(var_t *, void *), void *arg)
{
    var_t *v;
    int t;

    dbg_ifb (vs == NULL) return;
    dbg_ifb (cb == NULL) return;

    for(t = 0; (v = u_list_get_n(vs->list, t)) != NULL; ++t)
    {
        if(cb(v, arg))
            break;
    }

    return;
}

