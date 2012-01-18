#include <klone/context.h>
#include <klone/hook.h>
#include <klone/hookprv.h>

/**
 * \brief   Set a hook that executes when on server startup
 *
 * This hook will be executed just once on server startup. 
 *
 * Pass a NULL pointer to disable a previously set hook.
 *
 * \param func      the function pointer
 *
 * \return 0 on success, not zero on error
 */
int hook_server_init( hook_server_init_t func )
{
    hook_t *hook;

    dbg_err_if(ctx == NULL);
    dbg_err_if(ctx->hook == NULL);

    ctx->hook->server_init = func; /* may be NULL */

    return 0;
err:
    return ~0;
}

/**
 * \brief   Set a hook that executes just before quitting the server
 *
 * This hook is called during the shutdown procedure of the Klone server.
 *
 * Pass a NULL pointer to disable a previously set hook.
 *
 * \param func      the function pointer
 *
 * \return 0 on success, not zero on error
 */
int hook_server_term( hook_server_term_t func )
{
    hook_t *hook;

    dbg_err_if(ctx == NULL);
    dbg_err_if(ctx->hook == NULL);

    ctx->hook->server_term = func; /* may be NULL */

    return 0;
err:
    return ~0;
}

/**
 * \brief   Set a hook that executes at each main loop
 *
 * This hook is called at each main loop (approx once per second).
 *
 * Pass a NULL pointer to disable a previously set hook.
 *
 * \param func      the function pointer
 *
 * \return 0 on success, not zero on error
 */
int hook_server_loop( hook_server_loop_t func )
{
    hook_t *hook;

    dbg_err_if(ctx == NULL);
    dbg_err_if(ctx->hook == NULL);

    ctx->hook->server_loop = func; /* may be NULL */

    return 0;
err:
    return ~0;
}
