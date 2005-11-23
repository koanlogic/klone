#ifndef _KLONE_SUPPLIER_H_
#define _KLONE_SUPPLIER_H_

#include <klone/request.h>
#include <klone/response.h>
#include <klone/page.h>

typedef struct supplier_s
{
    const char *name;       /* descriptive name          */
    int (*init)(void);
    void (*term)(void);
    int (*is_valid_uri)(const char *buf, size_t len, time_t *mtime);
    int (*serve)(request_t *, response_t*);
} supplier_t;

#endif
