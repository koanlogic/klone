#ifndef _MIME_MAP_H_
#define _MIME_MAP_H_

typedef struct mime_map_s
{
    const char *ext, *mime_type;
    int comp;   /* if >0 compression is recommended */
} mime_map_t;

extern mime_map_t mime_map[];

#endif
