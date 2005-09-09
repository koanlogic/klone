#ifndef _KLONE_CODEC_H_
#define _KLONE_CODEC_H_
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>

typedef struct codec_s
{
    ssize_t (*transform)(struct codec_s *codec, char *dst, size_t *dst_cnt,
            const char *src, size_t src_sz);
    ssize_t (*flush)(struct codec_s *codec, char *dst, size_t *dst_cnt);
    int (*free)(struct codec_s *codec);
} codec_t;


int codec_free(codec_t *codec);

#endif
