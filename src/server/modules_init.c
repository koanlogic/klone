#include <klone/debug.h>
#include <klone/session.h>
#include <klone/config.h>
#include "context.h"

/* funcs declaration */
int session_client_module_init(config_t *config);

/* this function will be called just after app initialization and before 
   running any "useful" code; add here your initialization function calls */
int modules_init(context_t *ctx)
{
    /* init client-side session data structures */
    dbg_err_if(session_client_module_init(ctx->config));

    return 0;
err:
    return ~0;
}
