#ifndef _KLONED_CHILD_S_H_
#define _KLONED_CHILD_S_H_
#include "klone_conf.h"
#include <time.h>
#include <u/libu.h>
#include <klone/backend.h>

struct child_s
{
    TAILQ_ENTRY(child_s) np;    /* next & prev pointers */
    pid_t pid;
    backend_t *be;
    time_t birth;
};

typedef struct child_s child_t;

struct children_s;
typedef struct children_s children_t;

int child_create(pid_t pid, backend_t *be, child_t **pchild);
int child_free(child_t *child);

backend_t* child_backend(child_t *child);
pid_t* child_pid(child_t *child);
time_t* child_birth(child_t *child);

/* list of child objects */
int children_create(children_t **pcs);
int children_free(children_t *cs);
int children_clear(children_t *cs);
size_t children_count(children_t *cs);
int children_del(children_t *cs, child_t *child);
int children_add(children_t *cs, child_t *child);
int children_getn(children_t *cs, size_t i, child_t **pc);
int children_get_by_pid(children_t *cs, pid_t pid, child_t **pc);

#endif
