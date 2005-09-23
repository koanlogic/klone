#ifndef _KLONE_HTTP_H_
#define _KLONE_HTTP_H_
#include <klone/config.h>

enum http_method_e
{ 
    HM_UNKNOWN, 
    HM_GET, 
    HM_HEAD, 
    HM_POST, 
    HM_PUT, 
    HM_DELETE 
};

struct http_s;
typedef struct http_s http_t;

struct session_opt_s;;

config_t *http_get_config(http_t* http);
struct session_opt_s *http_get_session_opt(http_t* http);

int http_alias_resolv(http_t *h, char *dst, const char *filename, size_t sz);
const char* http_get_status_desc(int status);

#endif
