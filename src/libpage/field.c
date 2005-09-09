#include <string.h>
#include <klone/field.h>
#include <klone/utils.h>
#include <klone/debug.h>

/** 
 *  \ingroup Chttp
 *  \{
 */

/**
 *          \defgroup field HTTP header fields handling
 *          \{
 *              \par
 */

/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *  
 * \param f     parameter \a f description
 * \param name  parameter \a name description
 * \param value parameter \a value description
 *      
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */     
int field_set(field_t* f, const char *name, const char *value)
{
    char *n = NULL, *v = NULL;

    n = u_strdup(name);
    dbg_err_if(n == NULL);

    v = u_strdup(value);
    dbg_err_if(v == NULL);

    if(f->name)
        u_free(f->name);

    if(f->value)
        u_free(f->value);

    f->name = n;
    f->value = v;

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
 * \param f     parameter \a f description
 * \param ln    parameter \a ln description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */ 
int field_set_from_line(field_t* f, const char *ln)
{
    enum { BUFSZ = 256 };
    char *p, *name = NULL;

    dbg_err_if(!ln || !strlen(ln));

    dbg_err_if((p = strchr(ln, ':')) == NULL);

    name = u_strndup(ln, p-ln);
    dbg_err_if(name == NULL);

    /* eat blanks between ':' and value */
    for(++p; u_isblank(*p); ++p)
        ;

    dbg_err_if(field_set(f, name, p));

    if(name)
        u_free(name);

    return 0;
err:
    if(name)
        u_free(name);
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param f parameter \a f description
 *  
 * \return
 *  - the string corresponding to the field name ...
 */
const char* field_get_name(field_t *f)
{
    return f->name; /* may be null */
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param f parameter \a f description
 *  
 * \return
 *  - the string corresponding to the field value ...
 */
const char* field_get_value(field_t *f)
{
    return f->value; /* may be null */
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param name  parameter \a name description
 * \param value parameter \a value description
 * \param pf    parameter \a pf description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int field_create(const char *name, const char *value, field_t **pf)
{
    field_t *f = NULL;

    f = u_calloc(sizeof(field_t));
    dbg_err_if(f == NULL);

    if(name)
        dbg_err_if((f->name = u_strdup(name)) == NULL);

    if(value)
        dbg_err_if((f->value = u_strdup(value)) == NULL);

    *pf = f;

    return 0;
err:
    if(f)
        field_free(f);
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param f parameter \a f description
 *  
 * \return
 *  - \c 0  always
 */
int field_free(field_t *f)
{
    if(f)
    {
        if(f->name)
            u_free(f->name);
        if(f->value)
            u_free(f->value);
        u_free(f);
    }

    return 0;
}

/**
 *          \}
 */

/**
 *  \}
 */
