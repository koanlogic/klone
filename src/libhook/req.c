#include <klone/context.h>
#include <klone/hook.h>
#include <klone/hookprv.h>

/**
 * \brief   Set a hook that executes on every HTTP request
 *
 * This hook will be executed on every HTTP request; for dynamic pages the hook
 * will fire after the execution of the code of the dynamic page.
 *
 * Pass a NULL pointer to disable a previously set hook.
 *
 * \param func      the function pointer
 *
 * \return 0 on success, not zero on error
 */
int hook_request( hook_request_t func )
{
    hook_t *hook;

    dbg_err_if(ctx == NULL);
    dbg_err_if(ctx->hook == NULL);

    ctx->hook->request = func; /* may be NULL */

    return 0;
err:
    return ~0;
}

