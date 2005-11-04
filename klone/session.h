#ifndef _KLONE_SESSION_H_
#define _KLONE_SESSION_H_
#include <klone/request.h>
#include <klone/response.h>
#include <klone/vars.h>
#include <u/libu.h>

struct session_s;
typedef struct session_s session_t;

int session_free(session_t*);
int session_remove(session_t*);
int session_clean(session_t*);
int session_age(session_t*);

vars_t *session_get_vars(session_t*);
const char *session_get(session_t*, const char*);
int session_set(session_t*, const char*, const char*);
int session_del(session_t*, const char*);
int session_save_to_io(session_t*, const char*);

#endif
