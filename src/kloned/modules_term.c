#include <klone/session.h>
#include <klone/context.h>
#include <u/libu.h>

/* this function will be called just before closing; put here your 
   destructors */
int modules_term(void)
{
    dbg_err_if (ctx == NULL);
    return 0;
err:
    return ~0;
}
