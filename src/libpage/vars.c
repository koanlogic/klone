#include <stdlib.h>
#include <sys/types.h>
#include <klone/vars.h>
#include <klone/varprv.h>
#include <klone/utils.h>
#include <klone/queue.h>
#include <klone/debug.h>

TAILQ_HEAD(var_list_s, var_s);

struct vars_s
{
    struct var_list_s list;     /* list of variables (var_t) */
    size_t count;               /* # of vars in the list     */
};


/** 
 *  \ingroup Vhttp Chttp
 *  \{
 */

/**
 *          \defgroup vars HTTP variables handling
 *          \{
 *              \par
 */

/**
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
string_t* vars_get_value_s(vars_t *vs, const char *name)
{
    var_t *v = NULL;

    dbg_err_if((v = vars_get(vs, name)) == NULL);

    return var_get_value_s(v);
err:
    return NULL;
}

/**
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

    vs = u_calloc(sizeof(vars_t));
    dbg_err_if(vs == NULL);

    TAILQ_INIT(&vs->list);

    *pvs = vs;

    return 0;
err:
    if(vs)
        vars_free(vs);
    return ~0;
}

/**
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
        u_free(vs);
    }

    return 0;
}

/**
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

/**
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs  parameter \a vs description
 * \param i   parameter \a i description
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs  parameter \a vs description
 *
 * \return
 *  - the number of elements in \a vs
 */
size_t vars_count(vars_t *vs)
{
    return vs->count;
}

/**
 * \brief   One line description
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
int vars_add_urlvar(vars_t *vs, const char *cstr)
{
    enum { NAMESZ = 256, VALSZ = 2048 };
    char *eq, name[NAMESZ], value[VALSZ];
    var_t *var = NULL;
    char *str = NULL;

    /* dup the string so we can modify it */
    str = u_strdup(cstr);
    dbg_err_if(str == NULL);

    eq = strchr(str, '=');
    dbg_err_if(eq == NULL);

    /* zero-term the name part */
    *eq = 0;

    /* url-decode var name */
    dbg_err_if(u_urlncpy(name, str, strlen(str), URLCPY_DECODE) <= 0);

    /* url-decode var value */
    dbg_err_if(u_urlncpy(value, eq+1, strlen(eq+1), URLCPY_DECODE) <= 0);

    /* dbg("name: [%s]  value: [%s]", name, value); */

    dbg_err_if(var_create(name, value, &var));

    /* push into the cookie list */
    dbg_err_if(vars_add(vs, var));

    u_free(str);

    return 0;
err:
    if(cstr)
        dbg("%s", cstr);
    if(str)
        u_free(str);
    if(var)
        var_free(var);
    return ~0;
}


/**
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

    u_free(dups);

    /* push into the cookie list */
    dbg_err_if(vars_add(vs, var));

    return 0;
err:
    if(dups)
        u_free(dups);
    if(var)
        var_free(var);
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs        parameter \a path description
 * \param var_name  parameter \a rs description
 * \param i         parameter \a pss description
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
        if(strcmp(string_c(v->sname), var_name) == 0)
        {
            if(i-- == 0)
                return v;
        }
    }

    return NULL;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs        parameter \a vs description
 * \param var_name  parameter \a var_name description
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs    parameter \a vs description
 * \param name  parameter \a name description
 * \param ith   parameter \a ith description
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs    parameter \a vs description
 * \param name  parameter \a name description
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs    parameter \a vs description
 * \param name  parameter \a name description
 * \param ith   parameter \a ith description
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs    parameter \a vs description
 * \param name  parameter \a name description
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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param vs       parameter \a vs description
 * \param foreach  parameter \a foreach description
 * \param arg      parameter \a arg description
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
