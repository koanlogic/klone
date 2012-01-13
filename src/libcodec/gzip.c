/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: gzip.c,v 1.25 2007/10/26 08:57:59 tho Exp $
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/codec.h>
#include <klone/cgzip.h>
#include <klone/utils.h>

#include <zlib.h>

struct codec_gzip_s
{
    codec_t codec;              /* parent structure block           */
    int action;                 /* GZIP_COMPRESS or GZIP_UNCOMPRESS */
    int err;                    /* last error code                  */
    z_stream zstr;              /* zlib internal structure          */
    int (*op)(z_streamp, int);  /* inflate or deflate               */
    int (*opEnd)(z_streamp);    /* inflateEnd or deflateEnd         */
    char dummy;                 /* ZLIB < 1.2 workaround dummy byte */
};

typedef struct codec_gzip_s codec_gzip_t;

static ssize_t gzip_flush(codec_t *codec, char *dst, size_t *dcount)
{
    codec_gzip_t *iz;

    dbg_return_if (codec == NULL, -1);
    dbg_return_if (dst == NULL, -1);
    dbg_return_if (dcount == NULL, -1);

    iz = (codec_gzip_t*)codec;
    
    /* can't set it to NULL even if zlib must not use it (avail_in == 0) */
    iz->zstr.next_in = (char*)0xDEADBEEF;
    iz->zstr.avail_in = 0;

#if !defined(ZLIB_VERNUM) || ZLIB_VERNUM < 0x1200
    /* zlib < 1.2.0 workaround: push a dummy byte at the end of the 
       stream when inflating (see zlib ChangeLog) */
    if(iz->action == GZIP_UNCOMPRESS && iz->dummy == 0)
    { 
        iz->zstr.next_in = &iz->dummy; /* dummy byte */
        iz->zstr.avail_in = 1; 
        iz->dummy++;
    }
#endif

    iz->zstr.next_out = dst;
    iz->zstr.avail_out = *dcount;

    /* should be Z_STREAM_END while uncompressing */
    if(iz->err != Z_STREAM_END)
    {
        iz->err = iz->op(&iz->zstr, 
            iz->action == GZIP_COMPRESS ? Z_FINISH : Z_NO_FLUSH);
        dbg_err_if(iz->err != Z_OK && iz->err != Z_STREAM_END);
    } 

    *dcount = *dcount - iz->zstr.avail_out;   /* written */

    return iz->err == Z_STREAM_END && *dcount == 0 ? 
        CODEC_FLUSH_COMPLETE : CODEC_FLUSH_CHUNK;
err:
    u_dbg("%s", zError(iz->err));
    return -1;
}

static ssize_t gzip_transform(codec_t *codec, char *dst, size_t *dcount, 
        const char *src, size_t src_sz)
{
    codec_gzip_t *iz;
    size_t consumed;
 
    dbg_return_if (codec == NULL, -1);
    dbg_return_if (src == NULL, -1);
    dbg_return_if (dst == NULL, -1); 
    dbg_return_if (dcount == NULL || *dcount == 0, -1);
    dbg_return_if (src_sz == 0, -1);

    iz = (codec_gzip_t*)codec;
    
    iz->zstr.next_out = dst;
    iz->zstr.avail_out = *dcount;

    iz->zstr.next_in = (char*)src;
    iz->zstr.avail_in = src_sz;

    iz->err = iz->op(&iz->zstr, Z_NO_FLUSH);
    dbg_err_if(iz->err != Z_OK && iz->err != Z_STREAM_END);

    consumed = src_sz - iz->zstr.avail_in;  /* consumed */
    *dcount = *dcount - iz->zstr.avail_out; /* written */

    return consumed; /* # of consumed input bytes */
err:
    u_dbg("%s", zError(iz->err));
    return -1;
}

static int gzip_free(codec_t *codec)
{
    codec_gzip_t *iz;
    int err;

    nop_return_if (codec == NULL, 0);
    
    iz = (codec_gzip_t*)codec;
    dbg_err_if((err = iz->opEnd(&iz->zstr)) != Z_OK);
    U_FREE(iz);

    return 0;
err:
    u_dbg("%s", zError(err));
    return ~0;
}

/**
 *  \addtogroup filters
 *  \{
 */

/**
 * \brief   Create a cipher \c codec_t object 
 *  
 * Create a gzip \c codec_t object at \p *piz suitable for compression or
 * decompression (depending on \p op).  
 *  
 * \param   op      one of \c GZIP_COMPRESS or \c GZIP_UNCOMPRESS
 * \param   piz     the created codec as a value-result arguement
 *  
 * \return \c 0 on success, \c ~0 otherwise
 */
int codec_gzip_create(int op, codec_t **piz)
{
    codec_gzip_t *iz = NULL;

    dbg_return_if (piz == NULL, ~0);

    iz = u_zalloc(sizeof(codec_gzip_t));
    dbg_err_if(iz == NULL);

    iz->codec.transform = gzip_transform;
    iz->codec.flush = gzip_flush;
    iz->codec.free = gzip_free;
    iz->action = op; 

    switch(op)
    {
    case GZIP_COMPRESS:
        iz->op = deflate;
        iz->opEnd = deflateEnd;
        dbg_err_if(deflateInit2(&iz->zstr, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                    -MAX_WBITS, 8, Z_DEFAULT_STRATEGY));
        break;
    case GZIP_UNCOMPRESS:
        iz->op = inflate;
        iz->opEnd = inflateEnd;
        dbg_err_if(inflateInit2(&iz->zstr, -MAX_WBITS) != Z_OK);
        break;
    default:
        dbg_err_if("bad gzip op");
    }

    *piz = (codec_t*)iz;

    return 0;
err:
    U_FREE(iz);
    return ~0;
}

/**
 *  \}
 */
