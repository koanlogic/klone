#ifndef _KLONE_CODEC_H_
#define _KLONE_CODEC_H_
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <u/libu.h>

TAILQ_HEAD(codec_chain_s, codec_s);
typedef struct codec_chain_s codec_chain_t; 

typedef struct codec_s
{
    ssize_t (*transform)(struct codec_s *codec, char *dst, size_t *dst_cnt,
            const char *src, size_t src_sz);
    ssize_t (*flush)(struct codec_s *codec, char *dst, size_t *dst_cnt);
    int (*free)(struct codec_s *codec);
    TAILQ_ENTRY(codec_s) np; /* next & prev pointers */
} codec_t;

int codec_free(codec_t *codec);

#endif
