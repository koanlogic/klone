/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: rsfilter.c,v 1.14 2007/12/03 16:05:55 tat Exp $
 */

#include "klone_conf.h"
#include <time.h>
#include <u/libu.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/session.h>
#include <klone/utils.h>
#include <klone/io.h>
#include <klone/codec.h>
#include <klone/http.h>
#include <klone/response.h>
#include <klone/rsfilter.h>
#include <klone/ses_prv.h>

/* this filter prints the HTTP header before any body part of the web page. 
 * the first RFBUFSZ bytes (at most) of the response will be buffered to 
 * postpone the header printing (the header can be modified until filter flush)
 */

enum { 
    RFS_BUFFERING,
    RFS_FLUSHING
};

struct response_filter_s
{
    codec_t codec;          /* must be the first item in the struct */
    request_t *rq;
    response_t *rs;
    session_t *ss;
    int state, feeded;
    char buf[RFBUFSZ], *ptr;
    size_t off;
    io_t *iob;
};

static int rf_init_iob(response_filter_t *rf)
{
    char *h;
    size_t hsz, htell;

    dbg_err_if (rf == NULL);
    
    hsz = response_get_max_header_size(rf->rs) + rf->off;

    h = (char *)u_zalloc(hsz);
    dbg_err_if(h == NULL);

    dbg_err_if(io_mem_create(h, hsz, 0, &rf->iob));

    /* write the header to the memory io_t */
    response_print_header_to_io(rf->rs, rf->iob);

    if(response_get_method(rf->rs) != HM_HEAD)
    {
        /* append the rf->buf to the iob */
        dbg_err_if(io_write(rf->iob, rf->buf, rf->off) < 0);
    }
    dbg_err_if(io_flush(rf->iob));

    htell = io_tell(rf->iob);

    dbg_if(io_free(rf->iob));
    rf->iob = NULL;

    /* create another in-memory io to read from it */
    dbg_err_if(io_mem_create(h, htell, IO_MEM_FREE_BUF, &rf->iob));

    return 0;
err:
    return ~0;
}

static int rf_flush(codec_t *codec, char *dst, size_t *dcount)
{
    response_filter_t *rf = (response_filter_t*)codec;
    ssize_t c;


    dbg_err_if (codec == NULL);
    dbg_err_if (dst == NULL);
    dbg_err_if (dcount == NULL);

    if(rf->state == RFS_BUFFERING)
    {
        rf->state = RFS_FLUSHING;

        /* create a in-memory io_t and fill it with header and rf->buf */
        dbg_err_if(rf_init_iob(rf));
    }

    if(rf->iob)
    {
        dbg_err_if((c = io_read(rf->iob, dst, *dcount)) < 0);
        if(c == 0)
        { /* eof */
            io_free(rf->iob);
            rf->iob = NULL;
        } else {
            *dcount = c;
            return CODEC_FLUSH_CHUNK;
        }
    }

    return CODEC_FLUSH_COMPLETE;
err:
    return -1;
}

static ssize_t rf_transform(codec_t *codec, 
        char *dst, size_t *dcount, 
        const char *src, size_t src_sz)
{
    response_filter_t *rf = (response_filter_t*)codec;
    size_t max;
    ssize_t c;

    dbg_err_if (codec == NULL);
    dbg_err_if (dst == NULL);
    dbg_err_if (dcount == NULL);
    dbg_err_if (src == NULL);

    rf->feeded = 1;

    /* if this's a HEAD request don't print the body of the page */
    if(response_get_method(rf->rs) == HM_HEAD)
    {
        *dcount = 0;    /* zero output byte written */
        return src_sz;  /* all input bytes consumed */
    }

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
            rf->state = RFS_FLUSHING;

            /* here's the last chance to modify HTTP header so, if not already
               set, add a session ID to the cookie list */
            if(rf->ss && strlen(rf->ss->id) == 0)
                dbg_err_if(session_priv_set_id(rf->ss, NULL));

            /* create a in-memory io_t and fill it with header and rf->buf */
            dbg_err_if(rf_init_iob(rf));
        }
    }

    if(rf->iob)
    {
        dbg_err_if((c = io_read(rf->iob, dst, *dcount)) < 0);
        if(c == 0)
        { /* eof */
            io_free(rf->iob);
            rf->iob = NULL;
        } else {
            *dcount = c;
            return 0;
        }
    }

    /* copyout the next data block */
    max = U_MIN(*dcount, src_sz);
    memcpy(dst, src, max);
    *dcount = max;
    return max;
err:
    return -1;
}

static int rf_free(codec_t *codec)
{
    response_filter_t *rf;

    dbg_return_if (codec == NULL, 0);   /* it's ok */

    rf = (response_filter_t*)codec;
 
    if(rf->iob)
        io_free(rf->iob);

    U_FREE(rf);

    return 0;
}

int response_filter_feeded(codec_t *codec)
{
    response_filter_t *rf;

    dbg_return_if (codec == NULL, 0);   /* it's ok */

    rf = (response_filter_t*)codec;

    return rf->feeded; /* 1 if at least 1 byte has been written */
}

int response_filter_create(request_t *rq, response_t *rs, session_t *ss,
    codec_t **prf)
{
    response_filter_t *rf = NULL;

    dbg_err_if (rs == NULL);
    dbg_err_if (prf == NULL);
    
    rf = u_zalloc(sizeof(response_filter_t));
    dbg_err_if(rf == NULL);

    rf->rq = rq;
    rf->rs = rs;
    rf->ss = ss;
    rf->codec.transform = rf_transform;
    rf->codec.flush = rf_flush;
    rf->codec.free = rf_free;
    rf->ptr = rf->buf;
    rf->iob = NULL;

    *prf = (codec_t*)rf;

    return 0;
err:
    U_FREE(rf);
    return ~0;
}
