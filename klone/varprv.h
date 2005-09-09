#ifndef _KLONE_VAR_PRV_H_
#define _KLONE_VAR_PRV_H_
#include <klone/queue.h>
#include <klone/str.h>

struct var_s
{
    TAILQ_ENTRY(var_s) np;/* next & prev pointers   */
    string_t *sname;      /* var string name        */
    string_t *svalue;     /* var string value       */
};

#endif
