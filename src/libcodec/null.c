#include <klone/codec.h>
#include <klone/cnull.h>
#include <klone/utils.h>
#include <u/libu.h>
#include "conf.h"

struct codec_null_s
{
    codec_t codec;
};

static ssize_t null_flush(codec_null_t *iz, char *dst, size_t *dcount)
{
    *dcount = 0;
    return CODEC_FLUSH_COMPLETE;
}

static ssize_t null_transform(codec_null_t *iz, char *dst, size_t *dcount, 
        const char *src, size_t src_sz)
{
    ssize_t wr;
    
    dbg_err_if(src == NULL || dst == NULL || *dcount == 0 || src_sz == 0);

    wr = MIN(src_sz, *dcount); 
    memcpy(dst, src, wr);
    *dcount = wr;

    dbg_err_if(wr == 0);
    return wr;
err:
    return -1;
}

static int null_free(codec_null_t *iz)
{
    u_free(iz);

    return 0;
}


int codec_null_create(codec_null_t **piz)
{
    codec_null_t *iz = NULL;

    iz = u_zalloc(sizeof(codec_null_t));
    dbg_err_if(iz == NULL);

    iz->codec.transform = null_transform;
    iz->codec.flush = null_flush;
    iz->codec.free = null_free;      

    *piz = iz;

    return 0;
err:
    if(iz)
        u_free(iz);
    return ~0;
}

