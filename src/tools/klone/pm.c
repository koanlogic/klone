#include <fnmatch.h>
#include <u/libu.h>
#include "pm.h"

TAILQ_HEAD(pm_list_s, pm_s);
typedef struct pm_list_s pm_list_t;

/* this struct actually holds a tree but we use it as a plain linked list */
struct pm_s
{
    TAILQ_ENTRY(pm_s) np; /* next & prev pointers */
    char *pattern;

    pm_list_t list;
};

int pm_free(pm_t *pm)
{
    pm_t *item;

    dbg_err_if(pm == NULL);

    while((item = TAILQ_FIRST(&pm->list)) != NULL)    
    {                                                     
        TAILQ_REMOVE(&pm->list, item, np);                    
        pm_free(item);

    }                                                     
    if(pm->pattern)                                          
        U_FREE(pm->pattern);

    U_FREE(pm);

    return 0;
err:
    return ~0;
}


int pm_create(pm_t **ppm)
{
    pm_t *pm = NULL;

    dbg_err_if(ppm == NULL);

    pm = u_zalloc(sizeof(pm_t));
    dbg_err_if(pm == NULL);

    TAILQ_INIT(&pm->list);

    *ppm = pm;

    return 0;
err:
    return ~0;
}

int pm_add(pm_t *pm, const char *pattern)
{
    pm_t *item = NULL;

    dbg_err_if(pm == NULL);
    dbg_err_if(pattern == NULL);

    dbg_err_if(pm_create(&item));

    item->pattern = u_strdup(pattern);
    dbg_err_if(item->pattern == NULL);

    TAILQ_INSERT_TAIL(&pm->list, item, np);

    return 0;
err:
    if(item)
        pm_free(item);
    return ~0;
}

int pm_remove(pm_t *pm, const char *pattern)
{
    pm_t *item;

    dbg_err_if(pm == NULL);
    dbg_err_if(pattern == NULL);

    TAILQ_FOREACH(item, &pm->list, np)
    {
        if(!strcmp(pattern, item->pattern))
        {
            TAILQ_REMOVE(&pm->list, item, np);                    
            pm_free(item);
        }
    }

    return 0;
err:
    return ~0;
}

int pm_is_empty(pm_t *pm)
{
    dbg_err_if(pm == NULL);

    return TAILQ_FIRST(&pm->list) == NULL;
err:
    return 1; /* empty */
}

int pm_match(pm_t *pm, const char *uri)
{
    pm_t *item;

    dbg_err_if(pm == NULL);
    dbg_err_if(uri == NULL);

    TAILQ_FOREACH(item, &pm->list, np)
    {
        if(!fnmatch(item->pattern, uri, 0))
            return 1; /* matches */
    }

err:
    return 0; /* doesn't match */
}

