#ifndef _KLONE_IO_PRV_H_
#define _KLONE_IO_PRV_H_

#include "conf.h"
#ifdef HAVE_STDINT
#include <stdint.h>
#endif /* HAVE_STDINT */
#include <klone/codec.h>

/* functions used by io devices */

/** alloc sizeof(type) and initialize the io_t object */
#define io_create(type, pio) io_prv_create(sizeof(type), pio)
int io_prv_create(size_t dev_sz, io_t **pio);

typedef ssize_t (*io_read_op) (io_t*, char*, size_t);
typedef ssize_t (*io_write_op) (io_t*, const char*, size_t);
typedef ssize_t (*io_seek_op) (io_t*, size_t);
typedef ssize_t (*io_tell_op) (io_t*);
typedef int (*io_term_op) (io_t*);

struct io_s
{
    char *name;
    codec_chain_t codec_chain; 
    int eof;
    size_t size;

    /* reference count (used by dup'd io_t) */
    unsigned int refcnt; 

    /* io ops */
    io_read_op read;
    io_write_op write;
    io_seek_op seek;
    io_tell_op tell;
    io_term_op term;

    /* input buffer */

    char *rbuf;     /* read buffer                                            */
    size_t rbsz;    /* read buffer size                                       */
    size_t rcount;  /* available bytes in the buffer                          */
    size_t roff;    /* offset of the first byte to return                     */


    /* output buffer */

    char *wbuf;     /* write buffer                                           */
    size_t wbsz;    /* write buffer size                                      */
    size_t wcount;  /* # of non-empty bytes in the buffer                     */
    size_t woff;    /* offset of the head of the buffer                       */

};

#endif
