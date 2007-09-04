#include <klone/context.h>
#include <klone/hook.h>
#include <klone/hookprv.h>

/**
 * \brief   Set a hook to be notified when new children get forked
 *
 * When the main server forks a child \p func will be called in the child
 * context so it'll have the opportunity to initialize any one-per-process
 * resources.
 *
 * Note that a single child can (and probably will) handle more then one HTTP
 * connection.
 *
 * Pass a NULL pointer to disable a previously set hook.
 *
 * \param func      the function pointer
 *
 * \return 0 on success, not zero on error
 */
int hook_child_init( hook_child_init_t func )
{
    hook_t *hook;

    dbg_err_if(ctx == NULL);
    dbg_err_if(ctx->hook == NULL);

    ctx->hook->child_init = func; /* may be NULL */

    return 0;
err:
    return ~0;
}

/**
 * \brief   Set a hook to be notified when a child is about to die
 *
 * This hook executes just before the child dies; you may use it to 
 * free resources allocated by the hook_child_init hook.
 *
 * Pass a NULL pointer to disable a previously set hook.
 *
 * \param func      the function pointer
 *
 * \return 0 on success, not zero on error
 */
int hook_child_term( hook_child_term_t func )
{
    hook_t *hook;

    dbg_err_if(ctx == NULL);
    dbg_err_if(ctx->hook == NULL);

    ctx->hook->child_term = func;

    return 0;
err:
    return ~0;
}

