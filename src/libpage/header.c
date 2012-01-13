/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: header.c,v 1.20 2007/10/26 11:21:51 tho Exp $
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/os.h>
#include <klone/header.h>
#include <klone/utils.h>

/**
 * \ingroup header
 * \brief   Set the value of a field in a header
 *
 * Set the value of field \p name to \p value in header \p h.
 *
 * \param h     header object
 * \param name  name of the field
 * \param value value of the field
 *
 * \return \c 0 if successful, non-zero on error
 */
int header_set_field(header_t *h, const char *name, const char *value)
{
    field_t* n = NULL, *ex;

    dbg_err_if (h == NULL);
    dbg_err_if (name == NULL);
    dbg_err_if (value == NULL);

    /* modify existing field if already set */
    if((ex = header_get_field(h, name)) == NULL)
    {
        dbg_err_if(field_create(name, value, &n));
        dbg_err_if(header_add_field(h, n));
    } else
        dbg_err_if(field_set(ex, name, value));

    return 0;
err:
    if(n)
        field_free(n);
    return ~0;
}

/**
 * \ingroup header
 * \brief   Clear a header
 *
 * Clear all items in header \p h.
 *
 * \param h    header object
 *
 * \return \c 0 if successful, non-zero on error
 */
int header_clear(header_t *h)
{
    field_t *f;
    
    dbg_return_if (h == NULL, ~0);
    
    /* free all items */
    while((f = TAILQ_FIRST(&h->fields)) != NULL)
    {
        header_del_field(h, f);
        field_free(f);
    }

    return 0;
}

/**
 * \ingroup header
 * \brief   Count fields in a header
 *
 * Return the number of fields in header \p h.
 *
 * \param h    header object
 *
 * \return the number of fields found in \p h
 */
size_t header_field_count(header_t *h)
{
    dbg_return_if (h == NULL, 0);

    return h->nfields;
}

/** 
 * \ingroup header
 * \brief   Get ith field in a header
 *  
 * Return the \c field_t object at index \p idx in header \p h.
 *
 * \param h    header object
 * \param idx  index
 *  
 * \return the pointer to the field or \c NULL if no field could be found
 */
field_t *header_get_fieldn(header_t *h, size_t idx)
{
    field_t *f;
    size_t i = 0;

    dbg_goto_if (h == NULL, notfound);
    nop_goto_if (idx >= h->nfields, notfound);

    TAILQ_FOREACH(f, &h->fields, np)
    {
        if(i == idx)
            return f;
        ++i;
    }

notfound:
    return NULL;
}

/** 
 * \ingroup header
 * \brief   Get a field given a name
 *  
 * Return the first matching \c field_t object with name \p name in header \p h
 *
 * \param h     header object
 * \param name  name of the field to be searched
 *  
 * \return
 *  - the field string corresponding to \p name
 *  - \c NULL if no field could be found
 */
field_t *header_get_field(header_t *h, const char *name)
{
    field_t *f = NULL;

    dbg_goto_if (h == NULL, notfound);
    dbg_goto_if (name == NULL, notfound);

    TAILQ_FOREACH(f, &h->fields, np)
        if(strcasecmp(f->name, name) == 0)
            return f;

notfound:
    return NULL;
}

/** 
 * \ingroup header
 * \brief   Get field value
 *  
 * Return a string representation of the field with name \p name in header \p h
 *
 * \param h     header object
 * \param name  name of the field
 *  
 * \return
 *  - the field string corresponding to \p name
 *  - \c NULL if no field could be found
 */
const char *header_get_field_value(header_t *h, const char *name)
{
    field_t *f;

    dbg_return_if (h == NULL, NULL);
    dbg_return_if (name == NULL, NULL);
    
    f = header_get_field(h, name);

    return f ? field_get_value(f) : NULL;
}

/** 
 * \ingroup header
 * \brief   Delete a field from a header
 *  
 * Delete the supplied field \p f in header \p h.
 *
 * \param h  header object
 * \param f  field to be deleted
 *  
 * \return \c 0 on success, non-zero otherwise
 */
int header_del_field(header_t *h, field_t *f)
{
    dbg_return_if (h == NULL, ~0);
    dbg_return_if (f == NULL, ~0);

    TAILQ_REMOVE(&h->fields, f, np);
    h->nfields--;

    return 0;
}

/** 
 * \ingroup header
 * \brief   Add a field to a header
 *  
 * Add a field \p f to header \p h.
 *
 * \param h  header object
 * \param f  field to be added
 *  
 * \return \c 0 on success, non-zero otherwise
 */
int header_add_field(header_t *h, field_t *f)
{
    dbg_return_if (h == NULL, ~0);
    dbg_return_if (f == NULL, ~0);

    TAILQ_INSERT_TAIL(&h->fields, f, np);
    h->nfields++;

    return 0;
}

static int header_process_line(header_t *h, u_string_t *line, int mode)
{
    field_t *ex, *f = NULL;

    dbg_err_if (h == NULL);
    dbg_err_if (line == NULL);
    
    if(!u_string_len(line))
        return 0;

    /* alloc a new field */
    dbg_err_if(field_create(NULL, NULL, &f));

    /* parse and set name, value and params */
    dbg_err_if(field_set_from_line(f, u_string_c(line)));

    /* add to this header */
    switch(mode)
    {
    case HLM_ADD:
        dbg_err_if(header_add_field(h, f));
        break;
    case HLM_OVERRIDE:
        if((ex = header_get_field(h, field_get_name(f))) != NULL)
        {
            header_del_field(h, ex);
            field_free(ex); ex = NULL;
        }
        dbg_err_if(header_add_field(h, f));
        break;
    case HLM_KEEP:
        if((ex = header_get_field(h, field_get_name(f))) == NULL)
            dbg_err_if(header_add_field(h, f));
        else {
            field_free(f); f = NULL;
        }
        break;
    default:
        crit_err("unknown header load mode");
    }

    return 0;
err:
    if(f)
        field_free(f);
    return ~0;
}

/* load from environment. change each HTTP_name=value to name=value (replacing
   '_' with '-' */
int header_load_from_cgienv(header_t *h)
{
    extern char **environ;
    enum { BUFSZ = 256 };
    int i;
    size_t blen, t;
    char *e, *eq, buf[BUFSZ];

    /* add HTTP_* to header field list */
    for(i = 0; environ[i]; ++i)
    {
        e = environ[i];
        if(strlen(e) > 5 && strncmp("HTTP_", e, 5) == 0)
        {
            memset(buf, 0, sizeof(buf));

            /* make a copy of e so we can modify it */
            u_strlcpy(buf, e + 5, sizeof buf);

            eq = strchr(buf, '=');
            if(eq == NULL)
                continue; /* malformed */

            *eq = 0; /* put a \0 between name and value */

            /* subst '_' with '-' */
            for(t = 0, blen = strlen(buf); t < blen; ++t)
                if(buf[t] == '_')
                    buf[t] = '-';

            dbg_if(header_set_field(h, buf, 1 + eq));
        }
    }

    return 0;
}

int header_load_ex(header_t *h , io_t *io, int mode)
{
    enum { HEADER_MAX_FIELD_COUNT = 256 }; /* max num of header fields */
    u_string_t *line = NULL, *unfolded = NULL;
    const char *ln;
    size_t len, c;

    dbg_err_if (h == NULL);
    dbg_err_if (io == NULL);

    dbg_err_if(u_string_create(NULL, 0, &line));
    dbg_err_if(u_string_create(NULL, 0, &unfolded));

    for(c = HEADER_MAX_FIELD_COUNT; u_getline(io, line) == 0; --c)
    {
        warn_err_ifm(c == 0, "too much header fields");

        ln = u_string_c(line);
        len = u_string_len(line);

        /* remove trailing nl(s) */
        while(len && u_isnl(ln[len-1]))
            u_string_set_length(line, --len);

        if(u_string_len(line) == 0)
            break; /* empty line */

        if(u_isblank(ln[0])) 
        {   /* this is a chunk of a folded line */
            dbg_err_if(u_string_append(unfolded, ln, u_string_len(line)));
        } else {
            if(u_string_len(unfolded))
            {
                /* go process this (already unfolded) line */
                header_process_line(h, unfolded, mode);
                u_string_clear(unfolded);
            }
            /* this may be the first line of a folded line so wait next lines */
            u_string_copy(unfolded, line);
        }
    }

    if(u_string_len(unfolded))
        header_process_line(h, unfolded, mode);

    u_string_free(unfolded);
    u_string_free(line);

    return 0;
err:
    if(line)
        u_string_free(line);
    if(unfolded)
        u_string_free(unfolded);
    return ~0;
}

int header_load(header_t *h , io_t *io)
{
    return header_load_ex(h, io, HLM_ADD);
}

int header_create(header_t **ph)
{
    header_t *h = NULL;

    dbg_err_if (ph == NULL);

    h = u_zalloc(sizeof(header_t));
    dbg_err_if(h == NULL);

    TAILQ_INIT(&h->fields);

    *ph = h;

    return 0;
err:
    return ~0;
}

int header_free(header_t *h)
{
    if (h)
    {
        (void) header_clear(h);
        U_FREE(h);
    }

    return 0;
}
