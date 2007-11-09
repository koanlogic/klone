#ifndef _KLONE_ACCESS_
#define _KLONE_ACCESS_
#include <u/libu.h>
#include <klone/header.h>
#include <klone/request.h>
#include <klone/response.h>

int access_log(http_t*, u_config_t *, request_t *, response_t *);

#endif
