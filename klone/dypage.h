#ifndef _KLONE_DYPAGE_H_
#define _KLONE_DYPAGE_H_
#include <klone/request.h>
#include <klone/response.h>
#include <klone/session.h>

#ifdef __cplusplus
extern "C" {
#endif 

struct dypage_args_s;
typedef struct dypage_args_s dypage_args_t;

typedef void (*dypage_fun_t)(dypage_args_t *);

enum { DYPAGE_MAX_PARAMS = 16 };

typedef struct dypage_param_s
{
    const char *key, *val;
} dypage_param_t;

const char *dypage_get_param(dypage_args_t *args, const char *key);

struct dypage_args_s
{
    request_t *rq;
    response_t *rs;
    session_t *ss;
    dypage_fun_t fun;   /* callback function */
    void *opaque;       /* additional opaque callback argument */

    size_t argc;        /* # of argv */
    const char **argv;  /* regex submatches (#0 is the full url) */
    size_t nparams;     /* # of named params */
    dypage_param_t *params; /* array of named parameters */
};

int dypage_serve(dypage_args_t *args);

#ifdef __cplusplus
}
#endif 

#endif
