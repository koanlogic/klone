#include <unistd.h>
#include <klone/io.h>
#include <klone/io.h>
#include <klone/ioprv.h>
#include <klone/codec.h>

#include <u/libu.h>

enum { 
    IO_RD_BUFSZ = 4096, 
    IO_WR_BUFSZ = 4096
};

#define IO_WBUF_AVAIL(io) (io->wbsz - io->wcount)
#define IO_WBUF_FULL(io) (io->wbsz == io->wcount)

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

/* alloc the read buffer */
static int io_rbuf_alloc(io_t *io)
{
    io->rbsz = IO_RD_BUFSZ; /* set the buffer size */

    /* if a dup'd io_t already alloc'd a buffer then use it */
    io->rbuf = (char*)u_malloc(io->rbsz);
    dbg_err_if(io->rbuf == NULL);
    io->roff = 0;

    return 0;
err:
    return ~0;
}

/* alloc the write buffer */
static int io_wbuf_alloc(io_t *io)
{
    io->wbsz = IO_WR_BUFSZ; /* set the buffer size */

    /* if a dup'd io_t already alloc'd a buffer then use it */
    io->wbuf = (char*)u_malloc(io->wbsz);
    dbg_err_if(io->wbuf == NULL);

    return 0;
err:
    return ~0;
}

static ssize_t io_transform(io_t *io, codec_t *codec, 
    char *dst, size_t *dcount, const char *src, size_t sz)
{
    enum { BUFSZ = (IO_RD_BUFSZ > IO_WR_BUFSZ ? IO_RD_BUFSZ : IO_WR_BUFSZ) };
    char buf[BUFSZ];
    size_t bufsz, cavail;
    ssize_t wr, c, ct;
    size_t sdcount = *dcount; /* saved dcount */

    if(codec == TAILQ_LAST(&io->codec_chain, codec_chain_s))
    {
        /* last codec in the chain */
        return codec->transform(codec, dst, dcount, src, sz); 
    } else {
#if 0
            GOOD
        c = 0;
        cavail = CODEC_BUFSZ - codec->ccount;
        if(sz && cavail)
        {
            dbg_err_if((c = codec->transform(codec, 
                codec->cbuf + codec->ccount, &cavail, src, sz)) < 0);

            codec->ccount += cavail;
        }

        if(codec->ccount)
        {
            dbg_err_if((ct = io_transform(io, TAILQ_NEXT(codec, np), 
                dst, dcount, codec->cbuf, codec->ccount)) < 0);

            codec->ccount -= ct;
            if(codec->ccount > 0) /* FIXME opt to remove memmove */
                memmove(codec->cbuf, codec->cbuf + ct, codec->ccount);

            return c;
        } else {
            *dcount = 0;
            return c;
        }
#endif
        c = 0;
        do
        {
            *dcount = sdcount;
            if(codec->ccount)
            {
                dbg_err_if((ct = io_transform(io, TAILQ_NEXT(codec, np), 
                    dst, dcount, codec->cbuf, codec->ccount)) < 0);
                codec->ccount -= ct;

                if(codec->ccount > 0) /* FIXME opt to remove memmove */
                    memmove(codec->cbuf, codec->cbuf + ct, codec->ccount);

                dbg_err_if(ct == 0 && *dcount == 0); // FIXME remove
                return c;
            } else
                *dcount = 0; /* no bytes written to 'dst' */
            c = 0; /* zero byte of 'src' consumed */
            cavail = CODEC_BUFSZ - codec->ccount;
            if(sz && cavail)
            {
                dbg_err_if((c = codec->transform(codec, 
                    codec->cbuf + codec->ccount, &cavail, src, sz)) < 0);

                codec->ccount += cavail;
                if(c)
                    return c;
            }
        } while(c == 0 && *dcount == 0 && (codec->ccount || sz));
        //dbg_err_if(c == 0 && *dcount == 0); // FIXME remove
        return c;
    }

err:
    return -1;
}
#if 0
static ssize_t io_transform(io_t *io, codec_t *codec, 
    char *dst, size_t *dcount, const char *src, size_t sz)
{
    enum { BUFSZ = (IO_RD_BUFSZ > IO_WR_BUFSZ ? IO_RD_BUFSZ : IO_WR_BUFSZ) };
    char buf[BUFSZ];
    size_t bufsz, cavail;
    ssize_t wr, c, ct;
    size_t sdcount = *dcount; /* saved dcount */

    if(codec == TAILQ_LAST(&io->codec_chain, codec_chain_s))
    {
        /* last codec in the chain */
        return codec->transform(codec, dst, dcount, src, sz); 
    } else {
        c = 0;
        do
        {
            *dcount = sdcount;
            if(codec->ccount)
            {
                dbg_err_if((ct = io_transform(io, TAILQ_NEXT(codec, np), 
                    dst, dcount, codec->cbuf, codec->ccount)) < 0);
                codec->ccount -= ct;

                dbg_err_if(ct == 0 && *dcount == 0); // FIXME remove
                return c;
            } else
                *dcount = 0; /* no bytes written to 'dst' */
            c = 0; /* zero byte of 'src' consumed */
            cavail = CODEC_BUFSZ - codec->ccount;
            if(sz && cavail)
            {
                dbg_err_if((c = codec->transform(codec, 
                    codec->cbuf + codec->ccount, &cavail, src, sz)) < 0);

                codec->ccount += cavail;
                if(c)
                    return c;
            }
        } while(c == 0 && *dcount == 0 && (codec->ccount || sz));
        //dbg_err_if(c == 0 && *dcount == 0); // FIXME remove
        return c;
    }

err:
    return -1;
}
#endif


/* transform the data if one or more codecs are set */
static inline ssize_t io_transfer(io_t *io, char *dst, size_t *dcount, 
        const char *src, size_t sz)
{
    if(!TAILQ_EMPTY(&io->codec_chain))
    {
        return io_transform(io, TAILQ_FIRST(&io->codec_chain), 
            dst, dcount, src, sz); 
    } else {
        ssize_t wr = MIN(sz, *dcount); 
        memcpy(dst, src, wr);
        *dcount = wr;
        return wr;
    }
}

static int io_transform_chunk(io_t *io, codec_t *codec, const char *buf, 
    size_t bufsz)
{
    ssize_t c;
    size_t sz;

    while(bufsz)
    {
        if(IO_WBUF_FULL(io))
            dbg_err_if(io_flush(io));

        sz = IO_WBUF_AVAIL(io);  /* avail bytes in the wbuffer */

        /* push 'out' bytes into next codec (repeat until all bytes 
           have been written) */
        dbg_err_if((c = io_transform(io, codec, 
            io->wbuf + io->woff, &sz, buf, bufsz)) < 0);

        /* some bytes MUST be read or written otherwise 
           we'll dead lock */
        dbg_err_if(c == 0 && sz == 0); 

        io->wcount += sz;
        io->woff += sz;
        buf += c;
        bufsz -= c;
    } 

    return 0;
err:
    return ~0;
}

static int io_flush_2_wbuf(io_t *io, codec_t *codec)
{
    int er;
    size_t sz;

    do
    {
        if(IO_WBUF_FULL(io))
            dbg_err_if(io_flush(io));

        sz = IO_WBUF_AVAIL(io);

        /* save flushed data to the write buffer */
        dbg_err_if((er = codec->flush(codec, io->wbuf + io->woff, 
            &sz)) < 0);

        io->woff += sz;
        io->wcount += sz;
    } while(er > 0);

    return 0;
err:
    return ~0;
}

static int io_flush_codec_cbuf(io_t *io, codec_t *codec)
{
    size_t ct, sz;

    while(codec->ccount)
    {
        if(IO_WBUF_FULL(io))
            dbg_err_if(io_flush(io));

        sz = IO_WBUF_AVAIL(io);

        dbg_err_if((ct = io_transform(io, TAILQ_NEXT(codec, np), 
            io->wbuf + io->woff, &sz, codec->cbuf, codec->ccount)) < 0);

        codec->ccount -= ct;

        if(codec->ccount > 0) /* FIXME opt to remove memmove */
            memmove(codec->cbuf, codec->cbuf + ct, codec->ccount);

        io->wcount += sz;
        io->woff += sz;

        dbg_err_if(ct == 0 && sz == 0); // FIXME remove
    } 

    return 0;
err:
    return ~0;
}

static int io_chain_flush(io_t *io)
{
    codec_t *codec;
    enum { BUFSZ = 4096 };
    char buf[BUFSZ], *out;
    size_t c, sz;
    int er;
    
    dbg(__FUNCTION__);

    TAILQ_FOREACH(codec, &io->codec_chain, np)
    {
        if(codec == TAILQ_LAST(&io->codec_chain, codec_chain_s))
        {
            dbg("LAST CODEC!");

            dbg("last ccount: %lu",codec->ccount);
            //dbg_err_if(io_flush_2_wbuf(io, codec));

        } else {
            for(;;)
            {
                if(codec->ccount)
                    dbg_err_if(io_flush_codec_cbuf(io, codec));

                sz = CODEC_BUFSZ - codec->ccount;
                dbg_err_if((er = codec->flush(codec, codec->cbuf+codec->ccount, 
                    &sz)) < 0);
                codec->ccount += sz;

                if(er == 0 && codec->ccount == 0)
                    break; /* flush of this codec completed */

                if(codec->ccount)
                    dbg_err_if(io_flush_codec_cbuf(io, codec));

            } /* for */
        }
    }

    return 0;
err:
    return ~0;
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
    codec_t *codec;

    dbg_err_if(io == NULL || io->refcnt == 0);

    /* skip if this io_t has been dup'd and there are still one or more 
       references in use */
    if(--io->refcnt)
        return 0;

    if(!TAILQ_EMPTY(&io->codec_chain))
        dbg_if(io_chain_flush(io));

    dbg_if(io_flush(io));

    /* free per dev resources */
    dbg_if(io->term(io));

    while((codec = TAILQ_FIRST(&io->codec_chain)) != NULL)
    {
        TAILQ_REMOVE(&io->codec_chain, codec, np);
        codec_free(codec);
    }

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

/* refill the read buffer */
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
                // FIXME
                #if 0
                codec_t *fi = io->codec;
                if(fi && fi->flush)
                {
                    sz = size;
                    dbg_err_if((c = fi->flush(fi, out, &sz)) < 0);
                    if(c == 0)
                        io->eof++;
                    out += sz;
                }
                #endif
                break; /* return */
            }
        }
        /* copy out bytes in the read buffer */
        sz = size;
        c = io_transfer(io, out, &sz, io->rbuf + io->roff, io->rcount);
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

         if(IO_WBUF_FULL(io)) /* if there's not more free space */
            dbg_err_if(io_flush(io));

        sz = IO_WBUF_AVAIL(io);
        c = io_transfer(io, io->wbuf + io->woff, &sz, buf, rem);
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
int io_codec_set(io_t *io, codec_t* c)
{
    return io_codec_add_tail(io, c);
}

int io_codec_add_head(io_t *io, codec_t* c)
{
    dbg_return_if(io == NULL || c == NULL, ~0);

    /* insert the codec at the head of the chain */
    TAILQ_INSERT_HEAD(&io->codec_chain, c, np);

    return 0;
}

int io_codec_add_tail(io_t *io, codec_t* c)
{
    dbg_return_if(io == NULL || c == NULL, ~0);

    /* insert the codec at the end of the chain */
    TAILQ_INSERT_TAIL(&io->codec_chain, c, np);

    return 0;
}

int io_codec_remove(io_t *io, codec_t* c)
{
    TAILQ_REMOVE(&io->codec_chain, c, np);

    return 0;
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
int io_name_set(io_t *io, const char *name)
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
int io_name_get(io_t *io, char* name, size_t sz)
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

    io = u_zalloc(dev_sz);
    dbg_err_if(io == NULL);

    TAILQ_INIT(&io->codec_chain);

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


