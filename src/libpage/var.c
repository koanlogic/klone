#include <stdlib.h>
#include <sys/types.h>
#include <klone/var.h>
#include <klone/utils.h>
#include <klone/varprv.h>
#include <u/libu.h>

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
u_string_t* var_get_name_s(var_t *v)
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
u_string_t* var_get_value_s(var_t *v)
{
    dbg_return_if(v == NULL, NULL);

    return v->svalue; /* may be NULL */
}

int var_create(const char* name, const char *value, var_t**pv)
{
    var_t *v = NULL;

    v = u_zalloc(sizeof(var_t));
    dbg_err_if(v == NULL);

    dbg_err_if(u_string_create(name, strlen(name), &v->sname));

    dbg_err_if(u_string_create(value, strlen(value), &v->svalue));

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
            u_string_free(v->sname);
        if(v->svalue)
            u_string_free(v->svalue);
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
    return u_string_c(v->sname);
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
    return u_string_c(v->svalue);
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
    dbg_err_if(u_string_set(v->sname, name, strlen(name)));

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
    dbg_err_if(u_string_set(v->svalue, value, strlen(value)));

    return 0; 
err:
    return ~0;
}


/**
 *          \}
 */

/**
 *  \}
 */
