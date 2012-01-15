/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: field.c,v 1.14 2007/10/26 08:57:59 tho Exp $
 */

#include "klone_conf.h"
#include <string.h>
#include <klone/field.h>
#include <klone/utils.h>
#include <u/libu.h>

int field_set(field_t*, const char *name, const char *value);
int field_set_from_line(field_t*, const char *line);
const char* field_get_name(field_t *f);
const char* field_get_value(field_t *f);

/**
 * \ingroup header
 * \brief   Set a field
 *
 * Set field \p f to have \p name and \p value.
 *  
 * \param f     field object
 * \param name  field name
 * \param value field value
 *      
 * \return \c 0 if successful, non-zero on error
 */     
int field_set(field_t *f, const char *name, const char *value)
{
    char *n = NULL, *v = NULL;

    dbg_err_if (f == NULL);
    dbg_err_if (name == NULL);
    dbg_err_if (value == NULL);

    n = u_strdup(name);
    dbg_err_if(n == NULL);

    v = u_strdup(value);
    dbg_err_if(v == NULL);

    U_FREE(f->name);
    U_FREE(f->value);

    f->name = n;
    f->value = v;

    return 0;
err:
    U_FREE(n);
    U_FREE(v);

    return ~0;
}

/**
 * \ingroup header
 * \brief   Set a field from a line
 *
 * Set the name and value of field \p f.  Name and value must be separated by 
 * \c ":".
 * 
 * \param f     field object
 * \param ln    line
 *  
 * \return \c 0 if successful, non-zero on error
 */ 
int field_set_from_line(field_t *f, const char *ln)
{
    enum { BUFSZ = 256 };
    char *p, *name = NULL;

    dbg_err_if (f == NULL);
    dbg_err_if (ln == NULL);
    dbg_err_if (!strlen(ln));

    dbg_err_if((p = strchr(ln, ':')) == NULL);

    name = u_strndup(ln, p-ln);
    dbg_err_if(name == NULL);

    /* eat blanks between ':' and value */
    for(++p; u_isblank(*p); ++p)
        ;

    dbg_err_if(field_set(f, name, p));

    U_FREE(name);

    return 0;
err:
    u_dbg("failed setting field from line: %s", ln);
    U_FREE(name);
    return ~0;
}

/** 
 * \ingroup header
 * \brief   Get the name of a field
 *  
 * Return the string value of field \p f.
 *
 * \param f     field object
 *  
 * \return the (null-terminated) string corresponding to the field name
 */
const char* field_get_name(field_t *f)
{
    dbg_return_if (f == NULL, NULL);

    return f->name; /* may be null */
}

/** 
 * \ingroup header
 * \brief   Get the value of a field
 *  
 * Return the string value of field \p f.
 *
 * \param f     field object
 *  
 * \return the (null-terminated) string corresponding to the field value
 */
const char* field_get_value(field_t *f)
{
    dbg_return_if (f == NULL, NULL);

    return f->value; /* may be null */
}

/** 
 * \ingroup header
 * \brief   Create a field
 *  
 * Create a field from \p name and \p value into \p *pf.
 *
 * \param name  field name
 * \param value field value
 * \param pf    address of field pointer
 *  
 * \return \c 0 if successful, non-zero on error
 */
int field_create(const char *name, const char *value, field_t **pf)
{
    field_t *f = NULL;

    /* name and value may be NULL */
    dbg_err_if (pf == NULL);

    f = u_zalloc(sizeof(field_t));
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
 * \ingroup header
 * \brief   Free a field
 *  
 * Free field \p f.
 *
 * \param f     field object
 *  
 * \return \c 0, always
 */
int field_free(field_t *f)
{
    if(f)
    {
        U_FREE(f->name);
        U_FREE(f->value);
        U_FREE(f);
    }

    return 0;
}
