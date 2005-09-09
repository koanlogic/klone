#ifndef _KLONE_SESPRV_H_
#define _KLONE_SESPRV_H_
#include <klone/session.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/vars.h>

typedef int (*session_load_t)(session_t*);
typedef int (*session_save_t)(session_t*);
typedef int (*session_remove_t)(session_t*);
typedef int (*session_term_t)(session_t*);

struct session_s
{
    vars_t *vars;               /* variable list                              */
    request_t *rq;              /* request bound to this session              */
    response_t *rs;             /* response bound to this session             */
    char filename[PATH_MAX];    /* session filename                           */
    char id[MD5_DIGEST_BUFSZ];  /* session ID                                 */
    int removed;                /* >0 if the calling session has been deleted */
    int mtime;                  /* last modified time                         */
    session_load_t load;        /* ptr to the driver load function            */
    session_save_t save;        /* ptr to the driver save function            */
    session_remove_t remove;    /* ptr to the driver remove function          */
    session_term_t term;        /* ptr to the driver term function            */
};

/* driver c'tor */
int session_file_create(config_t *, request_t*, response_t*, session_t**);
int session_mem_create(config_t *, request_t*, response_t*, session_t**);

/* private functions */
int session_prv_init(session_t *, request_t *, response_t *);
int session_prv_load(session_t *, io_t *);
int session_prv_save_var(var_t *, io_t *);

#endif
