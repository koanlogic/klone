/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: vars.c,v 1.13 2005/11/23 18:07:14 tho Exp $
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
    struct var_list_s list;     /* list of variables (var_t) */
    size_t count;               /* # of vars in the list     */
};


/**
 *  \defgroup vars_t vars_t - variables handling
 *  \{
 *      \par
 */

/**
 * \brief   Get u_string_t value of a variable
 *
 * Return an u_string_t containing the value of variable with \a name in
 * variable list \a vs.
 *  
 * \param vs    variable list
 * \param name  name of variable
 *      
 * \return
 *  - the variable value 
 */     
u_string_t* vars_get_value_s(vars_t *vs, const char *name)
{
    var_t *v = NULL;

    dbg_err_if((v = vars_get(vs, name)) == NULL);

    return var_get_value_s(v);
err:
    return NULL;
}

/*
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *  
 * \param pvs  parameter \a pvs description
 *      
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */     
int vars_create(vars_t ** pvs)
{
    vars_t *vs = NULL;

    vs = u_zalloc(sizeof(vars_t));
    dbg_err_if(vs == NULL);

    TAILQ_INIT(&vs->list);

    *pvs = vs;

    return 0;
err:
    if(vs)
        vars_free(vs);
    return ~0;
}

/*
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs  parameter \a vs description
 *
 * \return
 *  - \c 0  always
 */
int vars_free(vars_t *vs)
{
    var_t *v;

    if(vs)
    {
        /* free all variables */
        while((v = TAILQ_FIRST(&vs->list)) != NULL)
        {
            vars_del(vs, v);
            var_free(v);
        }
        U_FREE(vs);
    }

    return 0;
}

/*
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs  parameter \a vs description
 * \param v   parameter \a v description
 *
 * \return
 *  - \c 0  always
 */
int vars_add(vars_t *vs, var_t *v)
{
    TAILQ_INSERT_TAIL(&vs->list, v, np);
    vs->count++;

    return 0;
}

/*
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs  parameter \a vs description
 * \param v   parameter \a v description
 *
 * \return
 *  - \c 0  always
 */
int vars_del(vars_t *vs, var_t *v)
{
    TAILQ_REMOVE(&vs->list, v, np);
    vs->count--;

    return 0;
}

/**
 * \brief   Get ith variable
 *
 * Return the var_t at index \a i in list \a vs.
 *
 * \param vs  variable list
 * \param i   index
 *
 * \return
 *  - the ith var
 *  - \c NULL if there's no ith var in \a vs
 */
var_t* vars_getn(vars_t *vs, size_t i)
{
    var_t *v;

    if(i >= vs->count)
        return NULL; /* out of bounds */

    TAILQ_FOREACH(v, &vs->list, np)
    {
        if(i-- == 0)
            return v;
    }

    return NULL;
}

/**
 * \brief   Count the number of variables
 *
 * Return a size_t with the number of variables in a list.
 *
 * \param vs  variable list
 *
 * \return
 *  - the number of elements in \a vs
 */
size_t vars_count(vars_t *vs)
{
    return vs->count;
}

/*
 * \brief   Add an URL variable
 *
 * Parse the "name=value" string \a str, url-decode name and value and push 
 * it into \a vs.
 *
 * \param vs   parameter \a vs description
 * \param str  parameter \a str description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int vars_add_urlvar(vars_t *vs, const char *cstr, var_t **v)
{
    enum { NAMESZ = 256, VALSZ = 4096 };
    char sname[NAMESZ], svalue[VALSZ];
    char *val, *str = NULL, *name = sname, *value = svalue;
    var_t *var = NULL;
    size_t vsz;

    /* dup the string so we can modify it */
    str = u_strdup(cstr);
    dbg_err_if(str == NULL);

    val = strchr(str, '=');
    dbg_err_if(val == NULL);

    /* zero-term the name part and set the value pointer */
    *val++ = 0; 

    /* if the buffer on the stack is too small alloc a bigger one */
    if(strlen(str) >= NAMESZ)
        dbg_err_if((name = u_zalloc(1 + strlen(str))) == NULL);

    /* if the buffer on the stack is too small alloc a bigger one */
    if(strlen(val) >= VALSZ)
        dbg_err_if((value = u_zalloc(1 + strlen(val))) == NULL);

    /* url-decode var name */
    dbg_err_if(u_urlncpy(name, str, strlen(str), URLCPY_DECODE) <= 0);

    /* url-decode var value */
    dbg_err_if((vsz = u_urlncpy(value, val, strlen(val), URLCPY_DECODE)) <= 0);

    /* dbg("name: [%s]  value: [%s]", name, value); */

    /* u_urlncpy always add a \0 at the end of the resulting data block */
    --vsz;

    dbg_err_if(var_bin_create(name, value, vsz, &var));

    /* push into the var list */
    dbg_err_if(vars_add(vs, var));

    if(v)
        *v = var;

    /* if the buffer has been alloc'd on the heap then free it */
    if(value && value != svalue)
        U_FREE(value);

    if(name && name != sname)
        U_FREE(name);

    U_FREE(str);

    return 0;
err:
    if(value && value != svalue)
        U_FREE(value);
    if(name && name != sname)
        U_FREE(name);
    if(cstr)
        dbg("%s", cstr);
    if(str)
        U_FREE(str);
    if(var)
        var_free(var);
    return ~0;
}


/*
 * \brief   One line description
 *
 * Parse the "name=value" string \a str, create a var_t and push it to \a vs.
 *
 * \param vs   parameter \a vs description
 * \param str  parameter \a str description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int vars_add_strvar(vars_t *vs, const char *str)
{
    char *eq, *dups = NULL;
    var_t *var = NULL;

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
    if(dups)
        U_FREE(dups);
    if(var)
        var_free(var);
    return ~0;
}

/**
 * \brief   Get ith variable with given name
 *
 * Return the var_t at index \a i with name \a var_name in list \a vs.
 *
 * \param vs        variable list
 * \param var_name  variable name
 * \param i         index
 *
 * \return
 *  - the var_t object found
 *  - \c NULL if there's no i-th variable called \a var_name in \a vs
 */
var_t* vars_get_ith(vars_t *vs, const char *var_name, size_t i)
{
    var_t *v;

    TAILQ_FOREACH(v, &vs->list, np)
    {
        if(strcmp(u_string_c(v->sname), var_name) == 0)
        {
            if(i-- == 0)
                return v;
        }
    }

    return NULL;
}

/**
 * \brief   Get a variable with given name
 *
 * Return a var_t of the variable with \a name in list \a vs.
 *
 * \param vs        variable list
 * \param var_name  variable name
 *
 * \return
 *  - the var_t object found
 *  - \c NULL if there's no variable called \a var_name in \a vs
 */
var_t* vars_get(vars_t *vs, const char *var_name)
{
    return vars_get_ith(vs, var_name, 0);
}

/**
 * \brief   Get int value of variable with given name and index
 *
 * Get the int value of variable with given \a name and index \a ith in list \a
 * vs.
 *
 * \param vs    variable list
 * \param name  variable name
 * \param ith   index 
 *
 * \return
 *  - the integer value of \a name
 *  - \c 0 if no value could be found
 */
int vars_get_ith_value_i(vars_t *vs, const char *name, size_t ith)
{
    const char *v;

    v = vars_get_ith_value(vs, name, ith);
    if(v == NULL)
        return 0;
    else
        return atoi(v);
}

/**
 * \brief   Get int value of variable with given name.
 *
 * Return the int value of the variable with \a name in list \a vs.
 *
 * \param vs    variable list
 * \param name  variable name
 *
 * \return
 *  - the integer value of \a name
 *  - \c 0 if no value could be found
 */
int vars_get_value_i(vars_t *vs, const char *name)
{
    return vars_get_ith_value_i(vs, name, 0);
}

/**
 * \brief   Get the value of the variable at a given index. 
 *
 * Return the string value of the variable with \a name and index \a ith in list
 * \a vs.
 *
 * \param vs    variable list
 * \param name  variable name
 * \param ith   index
 *
 * \return
 *  - the value string corresponding to \a name at i-th position
 *  - \c NULL if no value could be found 
 */
const char* vars_get_ith_value(vars_t *vs, const char *name, size_t ith)
{
    var_t *v;

    v = vars_get_ith(vs, name, ith);

    return  v ? var_get_value(v) : NULL;
}

/**
 * \brief   Get the value of the variable with given name.
 *
 * Return the string value of the variable with \a name in list \a vs.
 *
 * \param vs    variable list
 * \param name  variable name
 *
 * \return
 *  - the value string corresponding to \a name
 *  - NULL if no value could be found
 */
const char* vars_get_value(vars_t *vs, const char *name)
{
    return vars_get_ith_value(vs, name, 0);
}

/**
 * \brief   Execute a function on all variables.
 *
 * Execute function \a foreach on 
 *
 * \param vs       variable list
 * \param foreach  function pointer
 * \param arg      argument to function
 *
 * \return
 *  - nothing
 */
void vars_foreach(vars_t *vs, int (*foreach)(var_t*, void*), void *arg)
{
    var_t *v;

    TAILQ_FOREACH(v, &vs->list, np)
    {
        if(foreach(v, arg))
            break;
    }

    return;
}

/**
 *          \}
 */

/**
 *  \}
 */
