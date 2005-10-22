#include <klone/codec.h>
#include <klone/cgzip.h>
#include <u/libu.h>
#include "conf.h"

#ifdef HAVE_LIBZ
#include <zlib.h>

struct codec_gzip_s
{
    codec_t codec;              /* parent structure block           */
    int action;                 /* GZIP_COMPRESS or GZIP_UNCOMPRESS */
    int err;                    /* last error code                  */
    z_stream zstr;              /* zlib internal structure          */
    int (*op)(z_streamp, int);  /* inflate or deflate               */
    int (*opEnd)(z_streamp);    /* inflateEnd or deflateEnd         */
};

static ssize_t gzip_flush(codec_gzip_t *iz, char *dst, size_t *dcount)
{
    static char c = 0;

    dbg("%s: flush dcount=%lu", iz->action == GZIP_COMPRESS ? "zip" : "unzip",
        *dcount);

    /* can't set it to NULL even if zlib must not use it (avail_in == 0) */
    iz->zstr.next_in = 0xDEADBEEF;
    iz->zstr.avail_in = 0;

    #if !defined(ZLIB_VERNUM) || ZLIB_VERNUM < 0x1200
    /* zlib < 1.2.0 workaround: push a dummy byte at the end of the 
       stream when inflating (see zlib ChangeLog) */
    if(iz->action == GZIP_UNCOMPRESS && c == 0)
    { 
        iz->zstr.next_in = &c; /* dummy byte */
        iz->zstr.avail_in = 1; 
        ++c;
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

    if(iz->err == Z_STREAM_END && *dcount == 0)
        dbg("%s: flush all done", 
            iz->action == GZIP_COMPRESS ? "zip" : "unzip");
    else
        dbg("%s: flush call again", 
            iz->action == GZIP_COMPRESS ? "zip" : "unzip");

    return iz->err == Z_STREAM_END && *dcount == 0 ? 
        0 /* all done           */: 
        1 /* call flush() again */;
err:
    dbg("%s", zError(iz->err));
    return -1;
}

static ssize_t gzip_transform(codec_gzip_t *iz, char *dst, size_t *dcount, 
        const char *src, size_t src_sz)
{
    size_t consumed;
    
    dbg_err_if(src == NULL || dst == NULL || *dcount == 0 || src_sz == 0);

    dbg("%s: transform dcount=%lu  src_sz=%lu", 
        iz->action == GZIP_COMPRESS ? "zip" : "unzip",
        *dcount, src_sz);

    iz->zstr.next_out = dst;
    iz->zstr.avail_out = *dcount;

    iz->zstr.next_in = src;
    iz->zstr.avail_in = src_sz;

    iz->err = iz->op(&iz->zstr, Z_NO_FLUSH);
    dbg_err_if(iz->err != Z_OK && iz->err != Z_STREAM_END);

    consumed = src_sz - iz->zstr.avail_in;  /* consumed */
    *dcount = *dcount - iz->zstr.avail_out; /* written */

    return consumed; /* # of consumed input bytes */
err:
    dbg("%s", zError(iz->err));
    return -1;
}

static int gzip_free(codec_gzip_t *iz)
{
    int err;

    dbg_err_if((err = iz->opEnd(&iz->zstr)) != Z_OK);

    u_free(iz);

    return 0;
err:
    dbg("%s", zError(err));
    return ~0;
}

int codec_gzip_create(int op, codec_gzip_t **piz)
{
    codec_gzip_t *iz = NULL;

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
        dbg_err_if("bad op");
    }

    *piz = iz;

    return 0;
err:
    if(iz)
        u_free(iz);
    return ~0;
}

#else /* zlib not found */

struct codec_gzip_s
{
    codec_t codec;
};

int codec_gzip_create(int op, codec_gzip_t **piz)
{
    codec_gzip_t *iz = NULL;

    iz = u_zalloc(sizeof(codec_gzip_t));
    dbg_err_if(iz == NULL);

    iz->codec.transform = NULL;     /* nop */
    iz->codec.flush = NULL;         /* nop */
    iz->codec.free = NULL;          /* nop */

    *piz = iz;

    return 0;
err:
    if(iz)
        u_free(iz);
    return ~0;
}

#endif /* if HAVE_LIBZ */
