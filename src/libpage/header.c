#include <klone/klone.h>
#include <klone/header.h>
#include <klone/utils.h>
#include <klone/str.h>
#include <klone/debug.h>


/** 
 *  \ingroup Chttp
 *  \{
 */

/**
 *          \defgroup header HTTP header handling
 *          \{
 *              \par
 */


/**
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param h     parameter \a h description
 * \param name  parameter \a name description
 * \param value parameter \a value description
 *
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int header_set_field(header_t *h, const char *name, const char *value)
{
    field_t* n = NULL, *ex;

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
 * \brief   One line description
 *
 * Detailed function descrtiption.
 *
 * \param h    parameter \a h description
 *
 * \return
 *  - the number of fields found in \a h
 */
size_t header_field_count(header_t *h)
{
    return h->nfields;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param h    parameter \a h description
 * \param idx  parameter \a idx description
 *  
 * \return
 *  - \c NULL  if no field could be found
 *  - the pointer to the field found 
 */
field_t* header_get_fieldn(header_t *h, size_t idx)
{
    field_t *f;
    size_t i = 0;

    if(idx >= h->nfields)
        return NULL;

    TAILQ_FOREACH(f, &h->fields, np)
    {
        if(i == idx)
            return f;
        ++i;
    }

    return NULL;
}


/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param h     parameter \a h description
 * \param name  parameter \a name description
 *  
 * \return
 *  - \c NULL if no field could be found
 *  - the field string corresponding to \a name
 */
field_t* header_get_field(header_t *h, const char *name)
{
    field_t *f = NULL;

    TAILQ_FOREACH(f, &h->fields, np)
        if(strcasecmp(f->name, name) == 0)
            return f;

    return NULL;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param h     parameter \a h description
 * \param name  parameter \a name description
 *  
 * \return
 *  - \c NULL if no field could be found
 *  - the field string corresponding to \a name
 */
const char* header_get_field_value(header_t *h, const char *name)
{
    field_t *f;

    f = header_get_field(h, name);

    return f ? field_get_value(f) : NULL;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param h  parameter \a h description
 * \param f  parameter \a f description
 *  
 * \return
 *  - \c 0  always
 */
int header_del_field(header_t *h, field_t *f)
{
    TAILQ_REMOVE(&h->fields, f, np);
    h->nfields--;

    return 0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param h  parameter \a h description
 * \param f  parameter \a f description
 *  
 * \return
 *  - \c 0  always
 */
int header_add_field(header_t *h, field_t *f)
{
    TAILQ_INSERT_TAIL(&h->fields, f, np);
    h->nfields++;

    return 0;
}

static int header_process_line(header_t *h, string_t *line)
{
    field_t *f = NULL;
    const char *p;

    if(!string_len(line))
        return 0;

    /* look for name/value delimiter */
    dbg_err_if((p = strchr(string_c(line), ':')) == NULL);

    /* alloc a new field */
    dbg_err_if(field_create(NULL,0,&f));

    /* parse and set name, value and params */
    dbg_err_if(field_set_from_line(f, string_c(line)));

    /* add to this header */
    dbg_err_if(header_add_field(h, f));

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
 * \param h  parameter \a h description
 * \param io parameter \a io description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int header_load(header_t *h , io_t *io)
{
    string_t *line = NULL, *unfolded = NULL;
    const char *ln;
    size_t len;

    dbg_err_if(h == NULL || io == NULL);

    dbg_err_if(string_create(NULL, 0, &line));
    dbg_err_if(string_create(NULL, 0, &unfolded));

    while(u_getline(io, line) == 0)
    {
        ln = string_c(line);
        len = string_len(line);

        /* remove trailing nl(s) */
        while(len && u_isnl(ln[len-1]))
            string_set_length(line, --len);

        if(string_len(line) == 0)
            break; /* empty line */

        if(u_isblank(ln[0])) 
        {   /* this is a chunk of a folded line */
            dbg_err_if(string_append(unfolded, ln, string_len(line)));
        } else {
            if(string_len(unfolded))
            {
                /* go process this (already unfolded) line */
                header_process_line(h, unfolded);
                string_clear(unfolded);
            }
            /* this may be the first line of a folded line so wait next lines */
            string_copy(unfolded, line);
        }
    }

    if(string_len(unfolded))
        header_process_line(h, unfolded);

    string_free(unfolded);
    string_free(line);

    return 0;
err:
    if(line)
        string_free(line);
    if(unfolded)
        string_free(unfolded);
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param ph  parameter \a ph description
 *  
 * \return
 *  - \c 0  if successful
 *  - \c ~0 on error
 */
int header_create(header_t **ph)
{
    header_t *h = NULL;

    h = u_calloc(sizeof(header_t));
    dbg_err_if(h == NULL);

    TAILQ_INIT(&h->fields);

    *ph = h;

    return 0;
err:
    return ~0;
}

/** 
 * \brief   One line description
 *  
 * Detailed function descrtiption.
 *
 * \param h  parameter \a h description
 *  
 * \return
 *  - \c 0  always
 */
int header_free(header_t *h)
{
    field_t *f;

    /* free all items */
    while((f = TAILQ_FIRST(&h->fields)) != NULL)
    {
        header_del_field(h, f);
        field_free(f);
    }

    u_free(h);

    return 0;
}

/**
 *          \}
 */

/**
 *  \}
 */ 
