#include <unistd.h>
#include <klone/io.h>
#include <klone/debug.h>
#include <klone/utils.h>
#include <klone/io.h>
#include <klone/ioprv.h>
#include <klone/codec.h>

enum { 
    IO_RD_DEFBUFSZ = 4096, 
    IO_WR_DEFBUFSZ = 4096 
};

/**
 *  \defgroup io_t io_t - Input/Output abstraction object
 *  \ingroup refapi
 *  \{
 *      \par
 */

/**
 * \brief  Write the input stream to the output stream 
 *
 * Read all data from \a in and copy it to \a out
 *
 * \param out    output IO object
 * \param in    input IO object
 *
 * \return the number of bytes read from \a in and written to \a out 
 *    or -1 on error
 *
 */
ssize_t io_pipe(io_t *out, io_t *in)
{
    enum { BUFSZ = 4096 };
    char buf[BUFSZ];
    size_t count = 0;   /* # of bytes piped */
    ssize_t c;

    for(;;)
    {
        c = io_read(in, buf, BUFSZ);
        dbg_err_if(c < 0);
        if(c == 0)
            break; /* eof */

        /* write it out */
        dbg_err_if(io_write(out, buf, c) < 0);

        count += c;
    }

    return count;
err:
    return -1;
}

/**
 * \brief  Duplicate an IO handle
 *
 * Create a copy of \a io and store it to \a *pio.
 * The returned object will share the same underlaying IO device, the same
 * codecs connected to \a io and the same input and output buffers.
 * Buffers, codecs and IO devices will not be released until all io_t object
 * associated to it will be freed.
 *
 * \param io    the io to be dupped
 * \param pio   on success will contain the duplicated io object
 *
 * \return 0 on success, not zero on failure
 *
 * \sa io_free
 */
int io_dup(io_t *io, io_t **pio)
{
    io->refcnt++;

    *pio = io;

    return 0;
}

/**
 * \brief  Copy a block of data between two io objects
 *
 * Read from \a in a block of data \a size bytes long and write it to
 * the \a out output IO object
 *
 * \param out    output IO object
 * \param in     input IO object
 * \param size   number of bytes to copy
 *
 * \return the number of bytes copied (that can be less the \a size 
 *    in case of EOF on \a in) or -1 on error. 
 *
 */
ssize_t io_copy(io_t *out, io_t *in, size_t size)
{
    enum { BUFSZ = 4096 };
    char buf[BUFSZ];
    size_t rem = size;
    ssize_t c;

    while(rem)
    {
        c = io_read(in, buf, MIN(BUFSZ, rem));
        dbg_err_if(c < 0);
        if(c == 0)
            break; /* eof */

        /* write it out */
        dbg_err_if(io_write(out, buf, c) < 0);

        rem -= c;
    }

    return size - rem;
err:
    return -1;
}


/**
 * \brief  Seek to the given position
 *
 *  Moves the read/write file offset so that the next read or the next write
 *  will start reading or writing to the given position.
 *  Note that not all IO devices support seeking (for ex. sockets don't) so
 *  this function will always fail when used on those devices.
 *
 * \param io     the IO object
 * \param off    absolute offset to move to
 *
 * \return the given offset on success, -1 on error
 */
ssize_t io_seek(io_t *io, size_t off)
{
    dbg_err_if(io_flush(io));

    if(io->seek)
        return io->seek(io, off);
err:
    return -1;
}

/**
 * \brief  Return the current file position
 *
 * Return the current file position. There's a unique read and write position
 * offset.
 *
 * \param io     the IO object
 *
 * \return the given offset on success, -1 on error
 */
ssize_t io_tell(io_t *io)
{
    dbg_err_if(io_flush(io));

    if(io->tell)
        return io->tell(io);
err:
    return -1;
}

static int io_rbuf_alloc(io_t *io)
{
    if(io->rbsz == 0)
        io->rbsz = IO_RD_DEFBUFSZ; /* set default buffer size */

    /* if a dup'd io_t already alloc'd a buffer then use it */
    io->rbuf = (char*)u_malloc(io->rbsz);
    dbg_err_if(io->rbuf == NULL);
    io->roff = 0;

    return 0;
err:
    return ~0;
}

static int io_wbuf_alloc(io_t *io)
{
    if(io->wbsz == 0)
        io->wbsz = IO_WR_DEFBUFSZ; /* set default buffer size */

    /* if a dup'd io_t already alloc'd a buffer then use it */
    io->wbuf = (char*)u_malloc(io->wbsz);
    dbg_err_if(io->wbuf == NULL);

    return 0;
err:
    return ~0;
}


/* note: codecs will be ONLY flushed on io_free() NOT on io_flush() */
static ssize_t io_codec_flush(io_t *io)
{
    codec_t *fi = io->codec;

    if(io->wbuf == NULL)
        io_wbuf_alloc(io); /* to flush the codec wbuf must be alloc'd */

    if(fi && fi->flush && io->wbuf)
    {
        size_t sz = io->wbsz;
        ssize_t err;

        /* flush if the buf is full */
        if(io->wbsz - io->wcount == 0)
            dbg_err_if(io_flush(io)); 

        /* flush codec's buffer (if any) */
        for(sz = io->wbsz - io->wcount; 
            (err = fi->flush(fi, io->wbuf + io->woff, &sz)) > 0; 
            sz = io->wbsz)
        {
            io->wcount += sz;
            io->woff += sz;
            if(io->wbsz - io->wcount == 0)
                dbg_err_if(io_flush(io));
        }
        dbg_err_if(err < 0);
    }

    return 0;
err:
    return -1;
}


/**
 * \brief  Free an IO object
 *
 * Free the given IO object. If \a io has been dup'd and the 
 * reference count is not zero then this function will only decrement it 
 * and return. 
 *
 * Otherwise \a io will be flushed, the codec applied to it (if any) freed and
 * any other resource associated to it released.
 *
 * \param io     the IO object
 *
 * \return 0 on success, not zero on error
 *
 * \sa io_dup
 */
int io_free(io_t *io)
{
    dbg_err_if(io == NULL || io->refcnt == 0);

    /* skip if this io_t has been dup'd and there are still one or more 
       references in use */
    if(--io->refcnt)
        return 0;

    if(io->codec)
        dbg_if(io_codec_flush(io));

    dbg_if(io_flush(io));

    if(io->codec)
        codec_free(io->codec);
    io->codec = NULL;

    /* free per dev resources */
    dbg_if(io->term(io));

    if(io->rbuf)
        u_free(io->rbuf);

    if(io->wbuf)
        u_free(io->wbuf);

    if(io->name)
        u_free(io->name);

    u_free(io);

    return 0;
err:
    return ~0;
}

static ssize_t io_underflow(io_t *io)
{
    ssize_t c;

    if(io->rbuf == NULL)
        dbg_err_if(io_rbuf_alloc(io)); /* alloc the read buffer */

    if(io->rcount == 0)
    { /* buffer has been totally read, get more bytes from the device */
        dbg_err_if((c = io->read(io, io->rbuf, io->rbsz)) < 0);
        if(c == 0)
            return 0; /* eof */
        io->rcount = c;
        io->roff = 0;
    }
    return c;
err:
    dbg_strerror(errno);
    return -1;
}

static ssize_t io_transform(io_t *io, char *dst, size_t *dcount, 
        const char *src, size_t sz)
{
    if(io->codec && io->codec->transform) 
        return io->codec->transform(io->codec, dst, dcount, src, sz); 
    else {
        ssize_t wr = MIN(sz, *dcount); 
        memcpy(dst, src, wr);
        *dcount = wr;
        return wr;
    }
}

/**
 * \brief  Read a block of data from an IO object
 *
 * Read \a size bytes from \a io and save them to \a buf (that must be big
 * enough).
 *
 * \param io    the IO object
 * \param buf   the buffer that will contain the read bytes   
 * \param size  number of bytes to read
 *
 * \return the number of bytes read and saved to \a buf, zero on end of file
 *  (EOF) and -1 on error.
 *
 */
ssize_t io_read(io_t *io, char *buf, size_t size)
{
    char *out = buf;
    size_t sz;
    ssize_t c;

    if(io->eof)
        return 0;

    while(size)
    {
        if(io->rcount == 0)
        {
            /* fetch some bytes and fill the buffer */
            dbg_err_if((c = io_underflow(io)) < 0);
            if(c == 0)
            {   /* eof, try to get some buffered byte from the codec */
                codec_t *fi = io->codec;
                if(fi && fi->flush)
                {
                    sz = size;
                    dbg_err_if((c = fi->flush(fi, out, &sz)) < 0);
                    if(c == 0)
                        io->eof++;
                    out += sz;
                }
                break; /* return */
            }
        }
        /* copy out bytes in the read buffer */
        sz = size;
        c = io_transform(io, out, &sz, io->rbuf + io->roff, io->rcount);
        dbg_err_if(c < 0);
        dbg_err_if(c == 0 && sz == 0);
        io->rcount -= c;
        io->roff += c;
        out += sz;
        size -= sz;
    }

    return out - buf;
err:
    return -1;
}

/**
 * \brief  Write a string to \a io using printf-style format strings
 *
 * Printf-like function used to easily write strings to \a io using 
 * well-known printf format strings. See printf manual for format description.
 *
 * \param io    the IO object
 * \param fmt   printf-style format
 * \param ...   variable argument list
 *
 * \return zero on success, -1 on error
 *
 */
ssize_t io_printf(io_t *io, const char *fmt, ...)
{
    enum { BUFSZ = 2048 };
    char buf[BUFSZ], *bbuf = NULL; 
    va_list ap, ap2;
    int sz;

    /* build the message to send to the log system */
    va_start(ap, fmt); /* init variable list arguments */

    sz = vsnprintf(buf, BUFSZ, fmt, ap);

    va_end(ap);

    if(sz >= BUFSZ)
    { /* stack buffer too small alloc a bigger one on the heap */
        va_start(ap2, fmt); /* don't know if ap can be reused... */

        bbuf = (char*)u_malloc(++sz);
        dbg_err_if(bbuf == NULL);

        if((sz = vsnprintf(bbuf, sz, fmt, ap2)) > 0)
        {
            dbg_err_if(io_write(io, bbuf, sz) < 0);
        }

        va_end(ap2);

        u_free(bbuf);
    } else if(sz > 0) {
        dbg_err_if(io_write(io, buf, sz) < 0);
    }

    return 0;
err:
    return -1;
}

/**
 * \brief  Flush the write buffer
 *
 * Force a write of all buffered data to the output device.
 *
 * \param io    the IO object
 *
 * \return zero on success, -1 on error
 *
 */
ssize_t io_flush(io_t *io)
{
    ssize_t c;
    size_t off = 0;
 
    while(io->wcount)
    {
        c = io->write(io, io->wbuf + off, io->wcount);
        dbg_err_if(c < 0);
        if(c == 0)
            break;
    
        io->wcount -= c;
        off += c;
    }

    io->woff = 0;

    return 0;
err:
    return -1;
}

/**
 * \brief  Write a block of data to an IO object
 *
 * Write \a size bytes of \a buf to \a io.
 *
 * \param io    the IO object
 * \param buf   the buffer that will contain the read bytes   
 * \param size  number of bytes to read
 *
 * \return the number of bytes written or -1 on error.
 *
 */
ssize_t io_write(io_t *io, const char *buf, size_t size)
{
    size_t sz = 0, rem = size;
    ssize_t c = 0;

    while(rem)
    {
        if(io->wbuf == NULL)
            dbg_err_if(io_wbuf_alloc(io)); /* alloc the write buffer */

        if(io->wcount == io->wbsz) /* if there's not more free space */
            dbg_err_if(io_flush(io));

        sz = io->wbsz - io->wcount;
        c = io_transform(io, io->wbuf + io->woff, &sz, buf, rem);
        dbg_err_if(c < 0);
        dbg_err_if(c == 0 && sz == 0); /* some bytes MUST be read or written */
        io->woff += sz;
        io->wcount += sz;
        buf += c;
        rem -= c;
    }

    return size;
err:
    return -1;
}

/**
 * \brief  Write a char to an io object
 *
 * Write \a c to \a io
 *
 * \param io    the IO object
 * \param c     the char to write
 *
 * \return the number of bytes written (1) on success or -1 on error.
 *
 */
inline ssize_t io_putc(io_t *io, char c)
{
    return io_write(io, &c, 1);
}

/**
 * \brief  Read a char from an io object
 *
 * Read a char from an io object and save it to \a *pc.
 *
 * \param io    the IO object
 * \param pc    on success will receive the read character
 *
 * \return the number of bytes read (1) on success or -1 on error.
 *
 */
inline ssize_t io_getc(io_t *io, char *pc)
{
    return io_read(io, pc, 1);
}

/**
 * \brief  Read a line from an io object
 *
 * Read a line from \a in and save it to \a buf that must be at least \a size
 * bytes long.
 *
 * \param io    the IO object
 * \param buf   destination buffer
 * \param size  size of \a buf
 *
 * \return the length of the line on success, 0 on EOF or -1 on error.
 *
 */
ssize_t io_gets(io_t *io, char *buf, size_t size)
{
    char ch, *p = buf;
    ssize_t rc;

    if(size < 2)
        return -1; /* buf too small */

    --size; /* save a char for \0 */

    for(; size; --size)
    {
        dbg_err_if((rc = io_getc(io, &ch)) < 0);

        if(rc == 0)
            break;  /* eof */

        if((*p++ = ch) == '\n')
            break;
    }
    *p = 0;

    return p - buf;
err:
    return -1;
}

/**
 * \brief  Apply a codec to an IO 
 *
 * Apply \a codec to \a io so that every data read or written from and to the
 * IO object will be processed (and possibly modified) by the codec.
 *
 * \param io    the IO object
 * \param codec the codec to apply
 *
 * \return 0 on success, not zero on error
 *
 */
int io_set_codec(io_t *io, codec_t* codec)
{
    if(io->codec)
    {
        dbg_if(io_codec_flush(io));
        dbg_if(codec_free(io->codec));
    }

    io->codec = codec;
    return 0;
}

/**
 * \brief  Return the applied codec
 *
 * Return the codec object applied to the given IO object
 *
 * \param io    the IO object
 *
 * \return the applied codec object or NULL on error or if no codec has been set
 *
 */
codec_t* io_get_codec(io_t *io)
{
    return io->codec;
}


/**
 * \brief  Set the name of an IO
 *
 * Set the name of the given io to \a name. A name is a label that can be used
 * to store any naming scheme (file names, URI, etc.)
 *
 * \param io    the IO object
 * \param name  the name of the IO
 *
 * \return 0 on success, not zero on error
 *
 */
int io_set_name(io_t *io, const char *name)
{
    char *n;

    n = u_strdup(name);
    dbg_err_if(n == NULL);

    if(io->name)
        u_free(io->name);

    io->name = n;

    return 0;
err:
    return ~0;
}

/**
 * \brief  Return the name of the given IO
 *
 * Save in \a name the name of \a io. 
 *
 * \param io    the IO object
 * \param name  on success will contain the name of the given io
 * \param sz    size of \a name
 *
 * \return 0 on success, not zero on error
 *
 */
int io_get_name(io_t *io, char* name, size_t sz)
{
    size_t min = 0;

    if(io->name == NULL)
        return 0;

    dbg_err_if(sz < 2);

    min = MIN(sz-1, strlen(io->name));

    memcpy(name, io->name, min);
    name[min] = 0;
    
    return 0;
err:
    return ~0;
}


/* used by io devices init functions: alloc (used dev_sz block size) 
   and initialize an io_t */
int io_prv_create(size_t dev_sz, io_t **pio)
{
    io_t *io = NULL;

    io = u_calloc(dev_sz);
    dbg_err_if(io == NULL);

    /* set refcnt to 1 */
    io->refcnt++;

    /* size of the io device struct (that's also a 'castable' io_t) */
    io->size = dev_sz;

    *pio = io;

    return 0;
err:
    if(io)
        u_free(io);
    return -1;
}

/**
 *  \}
 */


