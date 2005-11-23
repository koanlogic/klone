#ifndef _KLONE_VAR_PRV_H_
#define _KLONE_VAR_PRV_H_

#include <u/libu.h>

struct var_s
{
    TAILQ_ENTRY(var_s) np;  /* next & prev pointers   */
    u_string_t *sname;      /* var string name        */
    u_string_t *svalue;     /* var string value       */
    char *data;
    size_t size;
};

#endif
