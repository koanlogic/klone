#include <klone/debug.h>
#include <klone/session.h>
#include "context.h"

/* this function will be called just before closing; put here your 
   destructors */
int modules_term(context_t *ctx)
{
    dbg_err_if (ctx == NULL);
    return 0;
err:
    return ~0;
}
