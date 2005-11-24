#include "klone_conf.h"
#include <u/libu.h>
#include "child.h"

TAILQ_HEAD(child_list_s, child_s);
typedef struct child_list_s child_list_t;

struct children_s
{
    child_list_t clist;
    size_t nchild;
};

int children_add(children_t *cs, child_t *child)
{
    dbg_return_if(cs == NULL || child == NULL, ~0);

    TAILQ_INSERT_TAIL(&cs->clist, child, np);
    cs->nchild++;

    return 0;
}

int children_del(children_t *cs, child_t *child)
{
    dbg_return_if(cs == NULL || child == NULL, ~0);

    TAILQ_REMOVE(&cs->clist, child, np);
    cs->nchild--;

    return 0;
}

size_t children_count(children_t *cs)
{
    dbg_return_if(cs == NULL, 0);

    return cs->nchild;
}

int children_clear(children_t *cs)
{
    child_t *c;

    dbg_err_if(cs == NULL);

    /* free all variables */
    while((c = TAILQ_FIRST(&cs->clist)) != NULL)
    {
        if(!children_del(cs, c))
            child_free(c);
    }

    return 0;
err:
    return 0;
}

int children_getn(children_t *cs, size_t i, child_t **pc)
{
    child_t *c;

    dbg_return_if(cs == NULL || pc == NULL, ~0);

    dbg_err_if(i >= cs->nchild); /* out of bounds */

    TAILQ_FOREACH(c, &cs->clist, np)
    {
        if(i-- == 0)
        {
            *pc = c;
            return 0; /* found */
        }
    }

err:
    return ~0;
}

int children_get_by_pid(children_t *cs, pid_t pid, child_t **pc)
{
    child_t *c;

    dbg_return_if(cs == NULL || pc == NULL, ~0);

    TAILQ_FOREACH(c, &cs->clist, np)
    {
        if(c->pid == pid)
        {
            *pc = c;
            return 0; /* found */
        }
    }

err:
    return ~0; /* not found */
}

int children_free(children_t *cs)
{
    if(cs)
    {
        /* del and free all child_t objects */
        dbg_if(children_clear(cs));
        U_FREE(cs);
    }

    return 0;
}

int children_create(children_t **pcs)
{
    children_t *cs;

    dbg_err_if(pcs == NULL);

    cs = u_zalloc(sizeof(children_t));
    dbg_err_if(cs == NULL);

    TAILQ_INIT(&cs->clist);

    *pcs = cs;

    return 0;
err:
    return ~0;
}


int child_create(pid_t pid, backend_t *be, child_t **pchild)
{
    child_t *c;

    dbg_err_if(pchild == NULL);

    c = u_zalloc(sizeof(child_t));
    dbg_err_if(c == NULL);

    c->pid = pid;
    c->be = be;
    c->birth = time(0);

    *pchild = c;

    return 0;
err:
    return ~0;
}

int child_free(child_t *child)
{
    if(child)
        U_FREE(child);

    return 0;
}

