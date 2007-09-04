#include <stdlib.h>
#include <u/libu.h>
#include <klone/hook.h>
#include <klone/hookprv.h>

int hook_create(hook_t **phook)
{
    hook_t *h = NULL;

    dbg_err_if(phook == NULL);

    h = u_zalloc(sizeof(hook_t));
    dbg_err_if(h == NULL);

    *phook = h;

    return 0;
err:
    return ~0;
}

int hook_free(hook_t *hook)
{
    dbg_err_if(hook == NULL);

    u_free(hook);

    return 0;
err:
    return ~0;
}

