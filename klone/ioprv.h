/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ioprv.h,v 1.14 2007/07/20 10:24:47 tat Exp $
 */

#ifndef _KLONE_IO_PRV_H_
#define _KLONE_IO_PRV_H_

#include "klone_conf.h"
#ifdef HAVE_STDINT
#include <stdint.h>
#endif /* HAVE_STDINT */
#include <klone/codec.h>
#include <klone/utils.h>

#ifdef __cplusplus
extern "C" {
#endif 

/* functions used by io devices */

/** alloc sizeof(type) and initialize the io_t object */
#define io_create(type, pio) io_prv_create(sizeof(type), pio)
int io_prv_create(size_t dev_sz, io_t **pio);

typedef ssize_t (*io_read_op) (io_t*, char*, size_t);
typedef ssize_t (*io_write_op) (io_t*, const char*, size_t);
typedef ssize_t (*io_seek_op) (io_t*, size_t);
typedef ssize_t (*io_tell_op) (io_t*);
typedef int (*io_close_op) (io_t*);
typedef int (*io_free_op) (io_t*);

struct io_s
{
    char *name;
    codec_chain_t codec_chain; 
    int eof;
    int type;
    size_t size;

    /* reference count (used by dup'd io_t) */
    unsigned int refcnt; 

    /* !0 for encrypted connections */
    int is_secure;

    /* io ops */
    io_read_op read;
    io_write_op write;
    io_seek_op seek;
    io_tell_op tell;
    io_close_op close;
    io_free_op free;

    /* input buffer */

    char *rbuf;     /* read buffer                                            */
    size_t rbsz;    /* read buffer size                                       */
    size_t rcount;  /* available bytes in the buffer                          */
    size_t roff;    /* offset of the first byte to return                     */

    size_t rtot;    /* total number of bytes read from this io                */

    /* underflow buffer */
    char *ubuf;     /* underflow buffer                                       */
    size_t ucount;  /* available bytes in the ubuffer                         */
    size_t uoff;    /* offset of the first byte to return                     */


    /* output buffer */

    char *wbuf;     /* write buffer                                           */
    size_t wbsz;    /* write buffer size                                      */
    size_t wcount;  /* # of non-empty bytes in the buffer                     */
    size_t woff;    /* offset of the head of the buffer                       */

};

#ifdef __cplusplus
}
#endif 

#endif
