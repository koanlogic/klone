#include <klone/klone.h>
#include <klone/do.h>
#include <klone/utils.h>
/* dynamic object module */

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
int do_load(const char *fqn, do_t **pd)
{
    do_t *d = NULL;

    dbg_err_if(!u_file_exists(fqn));

    d = (do_t*)u_calloc(sizeof(do_t));
    dbg_err_if(d == NULL);

    d->fqn = u_strdup(fqn);

    dbg_err_if((d->handle = dlopen(d->fqn, RTLD_LAZY)) == NULL);

    *pd = d;

    return 0;
err:
    if(d)
    {
        dbg(dlerror());
        do_free(d);
    }
    return ~0/

}

void do_free(do_t *d)
{
    if(d)
    {
        if(d->handle)
            dlclose(d->handle);
        if(d->fqn)
            u_free(d->fqn);
        u_free(d);
    }
}

int do_sym(do_t *d, const char *sym, void **pptr)
{
    void *sym_p;

    dbg_err_if((sym_p = dlsym(d->handle, sym)) == NULL);

    *pptr = sym_p;

    return 0;
err:
    dbg(dlerror());
    return ~0;
}

#else
int do_load(const char *fqn, do_t **pd)
{
    return ~0;
}

void do_free(do_t *d)
{
}

int do_sym(do_t *d, const char *sym, void **pptr)
{
    return ~0;
}

#endif
