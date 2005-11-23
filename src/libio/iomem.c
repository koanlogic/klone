#include "klone_conf.h"
#include <unistd.h>
#include <u/libu.h>
#include <klone/io.h>
#include <klone/io.h>
#include <klone/ioprv.h>

typedef struct
{
    struct io_s io; /* must be the first item */
    char *buf;
    size_t size;
    size_t off;
    int flags;
} io_mem_t;

static ssize_t io_mem_read(io_mem_t *io, char *buf, size_t size);
static ssize_t io_mem_write(io_mem_t *io, const char *buf, size_t size);
static ssize_t io_mem_seek(io_mem_t *io, size_t off);
static ssize_t io_mem_tell(io_mem_t *im);
static int io_mem_term(io_mem_t *io);

static ssize_t io_mem_tell(io_mem_t *im)
{
    return im->off;
}

static ssize_t io_mem_seek(io_mem_t *im, size_t off)
{
    if(off >= im->size)
        return -1;

    im->off = off;

    return off;
}

static ssize_t io_mem_read(io_mem_t *im, char *buf, size_t size)
{
    char *ptr;
    size_t sz;

    sz = MIN(size, im->size - im->off);
    if(sz)
    {
        ptr = im->buf + im->off; 
        memcpy(buf, ptr, sz);
        im->off += sz;
    }

    return sz;
}

static ssize_t io_mem_write(io_mem_t *im, const char *buf, size_t size)
{
    char *ptr;
    size_t sz;

    sz = MIN(size, im->size - im->off);
    if(sz)
    {
        ptr = im->buf + im->off; 
        memcpy(ptr, buf, sz);
        im->off += sz;
    }

    return sz;
}

static int io_mem_term(io_mem_t *im)
{
    if(im->flags & IO_MEM_FREE_BUF)
    {
        u_free(im->buf);
        im->buf = NULL;
        im->size = im->off = 0;
    }

    return 0;
}

int io_mem_create(char *buf, size_t size, int flags, io_t **pio)
{
    io_mem_t *im = NULL;

    dbg_err_if(io_create(io_mem_t, (io_t**)&im));

    im->buf = buf;
    im->size = size;
    im->flags = flags;
    im->off = 0;

    im->io.read     = (io_read_op) io_mem_read;
    im->io.write    = (io_write_op) io_mem_write;
    im->io.seek     = (io_seek_op) io_mem_seek;
    im->io.tell     = (io_tell_op) io_mem_tell;
    im->io.term     = (io_term_op) io_mem_term; 

    *pio = (io_t*)im;

    return 0;
err:
    if(im)
        io_free((io_t *)im);
    return ~0;
}


