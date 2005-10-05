#include <klone/debug.h>
#include <klone/session.h>
#include <klone/config.h>
#include <klone/context.h>

/* this function will be called just after app initialization and before 
   running any "useful" code; add here your initialization function calls */
int modules_init(void)
{
    dbg_err_if (ctx == NULL);

    return 0;
err:
    return ~0;
}
