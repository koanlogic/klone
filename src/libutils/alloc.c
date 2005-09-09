#include <stdlib.h>

void* u_malloc(size_t sz)
{
    return malloc(sz);
}

void* u_calloc(size_t sz)
{
    return calloc(1, sz);
}

void* u_realloc(void *ptr, size_t sz)
{
    return realloc(ptr, sz);
}

void u_free(void *p)
{
    if(p)
        free(p);
}
