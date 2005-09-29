#include <time.h>
#include <klone/response.h>
#include <klone/debug.h>
#include <klone/utils.h>
#include <klone/io.h>
#include <klone/codec.h>
#include <klone/http.h>
#include <klone/response.h>
#include "rsfilter.h"

/* this filter prints the HTTP header before any body part of the web page. 
 * the first RFBUFSZ bytes (at most) of the response will be buffered to 
 * postpone the header printing (so it can be modified).
 */

enum { 
    RFS_BUFFERING,
    RFS_SENDING_HEADER,
    RFS_SENDING_BODY
};

static int rf_flush(response_filter_t *rf, char *buf, size_t *sz)
{
    if(rf->state == RFS_BUFFERING)
    {
        /* print the header and flush the buffer */
        rf->state = RFS_SENDING_HEADER;
        response_print_header(rf->rs);

        rf->state = RFS_SENDING_BODY;
        if(rf->off > 0)
            dbg_err_if(io_write(response_io(rf->rs), rf->buf, rf->off) < 0);
    }

    return 0;
err:
    return -1;
}

static ssize_t rf_transform(response_filter_t *rf, 
        char *dst, size_t *dcount, 
        const char *src, size_t src_sz)
{
    size_t max;

    if(rf->state == RFS_BUFFERING)
    {
        if(rf->off + src_sz < RFBUFSZ)
        {
            memcpy(rf->buf + rf->off, src, src_sz);
            rf->off += src_sz;
            *dcount = 0;    /* zero output byte written */
            return src_sz;  /* src_sz input byte consumed */
        } else {
            /* the buffer is full, print the header and flush the buffer */
            dbg_err_if(rf_flush(rf, NULL, NULL));

            /* write out the current block */
            dbg_err_if(io_write(response_io(rf->rs), src, src_sz) < 0);
            
            *dcount = 0;   /* zero output byte written */
            return src_sz; /* all input byte consumed */
        }
    } else if(rf->state == RFS_SENDING_BODY) {
        /* if this's a HEAD request don't print the body of the page */
        if(response_get_method(rf->rs) == HM_HEAD)
        {
            *dcount = 0;    /* zero output byte written */
            return src_sz;  /* all in bytes consumed    */
        }
    }

    /* copyout the next data block */
    max = MIN(*dcount, src_sz);
    memcpy(dst, src, max);
    *dcount = max;
    return max;
err:
    return -1;
}

static int rf_free(response_filter_t *rf)
{
    u_free(rf);

    return 0;
}

int response_filter_create(response_t *rs, response_filter_t **prf)
{
    response_filter_t *rf = NULL;

    rf = u_calloc(sizeof(response_filter_t));
    dbg_err_if(rf == NULL);

    rf->rs = rs;
    rf->codec.transform = rf_transform;
    rf->codec.flush = rf_flush;
    rf->codec.free = rf_free;

    *prf = rf;

    return 0;
err:
    if(rf)
        u_free(rf);
    return ~0;
}
