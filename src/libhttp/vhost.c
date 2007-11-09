#include <klone/vhost.h>

int vhost_create(vhost_t **pv)
{
    vhost_t *v = NULL;

    dbg_err_if(pv == NULL);

    v = u_zalloc(sizeof(vhost_t));
    dbg_err_if(v == NULL);

    *pv = v;

    return 0;
err:
    return ~0;

}

int vhost_free(vhost_t *v)
{
    dbg_err_if(v == NULL);

    u_free(v);

    return 0;
err:
    return ~0;
}

/* insert the element at the tail of the list */
int vhost_list_add(vhost_list_t *vs, vhost_t *vhost)
{
    vhost_t *first, *last, *elm;

    last = NULL;
    LIST_FOREACH(elm, vs, np)
        last = elm;

    if(last) 
        LIST_INSERT_AFTER(last, vhost, np);
    else
        LIST_INSERT_HEAD(vs, vhost, np);

    return 0;
err:
    return ~0;
}

vhost_t* vhost_list_get(vhost_list_t *vs, const char *host)
{
    vhost_t *item;

    LIST_FOREACH(item, vs, np)
    {
        if(strcmp(item->host, host) == 0)
            return item;  /* found */
    }

    return NULL; /* not found */
}

vhost_t* vhost_list_get_n(vhost_list_t *vs, int n)
{
    vhost_t *item;

    LIST_FOREACH(item, vs, np)
    {
        if(n-- == 0)
            return item;
    }

    return NULL; /* not found */
}

int vhost_list_create(vhost_list_t **pvs)
{
    vhost_list_t *vs = NULL;

    dbg_err_if(pvs == NULL);

    vs = u_zalloc(sizeof(vhost_list_t));
    dbg_err_if(vs == NULL);

    LIST_INIT(vs);

    *pvs = vs;

    return 0;
err:
    if(pvs)
        u_free(pvs);
    return ~0;
}

int vhost_list_free(vhost_list_t *vs)
{
    dbg_err_if(vs == NULL);

    u_free(vs);

    return 0;
err:
    return ~0;
}

