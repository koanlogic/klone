#ifndef _KLONE_BROKER_H_
#define _KLONE_BROKER_H_

#include <klone/request.h>
#include <klone/response.h>
#include <klone/page.h>

struct broker_s;
typedef struct broker_s broker_t;

int broker_create(broker_t **pb);
int broker_free(broker_t* b);
int broker_is_valid_uri(broker_t *b, const char *buf, size_t len);
int broker_serve(broker_t *b, request_t *rq, response_t *rs);

#endif
