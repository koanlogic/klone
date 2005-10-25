#ifndef _KLONE_CODEC_H_
#define _KLONE_CODEC_H_
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <u/libu.h>

enum { CODEC_FLUSH_COMPLETE, CODEC_FLUSH_CHUNK };

enum { CODEC_BUFSZ = 4096 };

TAILQ_HEAD(codec_chain_s, codec_s);
typedef struct codec_chain_s codec_chain_t; 

typedef ssize_t (*codec_transform_t) (struct codec_s *codec, 
    char *dst, size_t *dst_cnt, const char *src, size_t src_sz);

typedef ssize_t (*codec_flush_t) (struct codec_s *codec, 
    char *dst, size_t *dst_cnt);

typedef int (*codec_free_t) (struct codec_s *codec);

typedef struct codec_s
{
    codec_transform_t transform;
    codec_flush_t flush;
    codec_free_t free;

    /* codec buffer */
    char cbuf[CODEC_BUFSZ];
    size_t ccount, coff;

    /* chain next & prev pointer */
    TAILQ_ENTRY(codec_s) np; 
} codec_t;

int codec_free(codec_t *codec);

#endif
