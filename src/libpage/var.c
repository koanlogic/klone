#include <stdlib.h>
#include <sys/types.h>
#include <klone/var.h>
#include <klone/queue.h>
#include <klone/debug.h>
#include <klone/utils.h>
#include <klone/varprv.h>

/** 
 *  \ingroup Vhttp Chttp
 *  \{
 */

/**
 *          \defgroup var HTTP variable handling
 *          \{
 *              \par
 */

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param name   parameter \a name description
 * \param value  parameter \a value description
 * \param pv     parameter \a pv description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
string_t* var_get_name_s(var_t *v)
{
    dbg_return_if(v == NULL, NULL);

    return v->sname; /* may be NULL */
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param name   parameter \a name description
 * \param value  parameter \a value description
 * \param pv     parameter \a pv description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
string_t* var_get_value_s(var_t *v)
{
    dbg_return_if(v == NULL, NULL);

    return v->svalue; /* may be NULL */
}

int var_create(const char* name, const char *value, var_t**pv)
{
    var_t *v = NULL;

    v = u_calloc(sizeof(var_t));
    dbg_err_if(v == NULL);

    dbg_err_if(string_create(name, strlen(name), &v->sname));

    dbg_err_if(string_create(value, strlen(value), &v->svalue));

    *pv = v;

    return 0;
err:
    if(v)
        var_free(v);
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param v  parameter \a v description
 *
 * \return
 *  - \c 0  always
 */
int var_free(var_t *v)
{
    if(v)
    {
        if(v->sname)
            string_free(v->sname);
        if(v->svalue)
            string_free(v->svalue);
        u_free(v);
    }

    return 0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param v  parameter \a v description
 *
 * \return
 *  - the name string of the given \a v
 */
const char* var_get_name(var_t *v)
{
    return string_c(v->sname);
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param v  parameter \a v description
 *
 * \return
 *  - the value string of the given \a v
 */
const char* var_get_value(var_t *v)
{
    return string_c(v->svalue);
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param var   parameter \a var description
 * \param name  parameter \a name description
 * \param value parameter \a value description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */ 
int var_set(var_t* var, const char *name, const char *value)
{
    dbg_err_if(var_set_name(var, name));

    dbg_err_if(var_set_value(var, value));

    return 0;
err:
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param v     parameter \a v description
 * \param name  parameter \a name description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int var_set_name(var_t *v, const char *name)
{
    dbg_err_if(string_set(v->sname, name, strlen(name)));

    return 0; 
err:
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param v      parameter \a v description
 * \param value  parameter \a value description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int var_set_value(var_t *v, const char *value)
{
    dbg_err_if(string_set(v->svalue, value, strlen(value)));

    return 0; 
err:
    return ~0;
}

#if 0

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param name   parameter \a name description
 * \param value  parameter \a value description
 * \param pv     parameter \a pv description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int var_create(const char* name, const char *value, var_t**pv)
{
    var_t *v = NULL;

    v = u_calloc(sizeof(var_t));
    dbg_err_if(v == NULL);

    if(name)
        dbg_err_if((v->name = u_strdup(name)) == NULL);

    if(value)
        dbg_err_if((v->value = u_strdup(value)) == NULL);

    *pv = v;

    return 0;
err:
    if(v)
        var_free(v);
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param v  parameter \a v description
 *
 * \return
 *  - \c 0  always
 */
int var_free(var_t *v)
{
    if(v)
    {
        if(v->name)
            u_free(v->name);
        if(v->value)
            u_free(v->value);
        u_free(v);
    }

    return 0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param v  parameter \a v description
 *
 * \return
 *  - the name string of the given \a v
 */
const char* var_get_name(var_t *v)
{
    return v->name;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param v  parameter \a v description
 *
 * \return
 *  - the value string of the given \a v
 */
const char* var_get_value(var_t *v)
{
    return v->value;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param var   parameter \a var description
 * \param name  parameter \a name description
 * \param value parameter \a value description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */ 
int var_set(var_t* var, const char *name, const char *value)
{
    char *n = NULL, *v = NULL;

    n = u_strdup(name);
    dbg_err_if(n == NULL);

    v = u_strdup(value);
    dbg_err_if(v == NULL);

    if(var->name)
        u_free(var->name);

    if(var->value)
        u_free(var->value);

    var->name = n;
    var->value = v;

    return 0;
err:
    if(n)
        u_free(n);
    if(v)
        u_free(v);
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param v     parameter \a v description
 * \param name  parameter \a name description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int var_set_name(var_t *v, const char *name)
{
    char *n = NULL;

    n = u_strdup(name);
    dbg_err_if(n == NULL);

    if(v->name)
        u_free(v->name);

    v->name = n;

    return 0; 
err:
    return ~0;
}

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param v      parameter \a v description
 * \param value  parameter \a value description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int var_set_value(var_t *v, const char *value)
{
    char *n = NULL;

    n = u_strdup(value);
    dbg_err_if(n == NULL);

    if(v->value)
        u_free(v->value);

    v->value = n;

    return 0; 
err:
    return ~0;
}

#endif



/**
 *          \}
 */

/**
 *  \}
 */
