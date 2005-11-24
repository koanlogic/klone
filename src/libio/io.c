/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: io.c,v 1.27 2005/11/24 18:05:01 tho Exp $
 */

#include "klone_conf.h"
#include <unistd.h>
#include <u/libu.h>
#include <klone/io.h>
#include <klone/io.h>
#include <klone/ioprv.h>
#include <klone/codec.h>

enum { 
    IO_RD_BUFSZ = 4096, 
    IO_WR_BUFSZ = 4096
};

#define IO_WBUF_AVAIL(io) (io->wbsz - io->wcount)
#define IO_WBUF_FULL(io) (io->wbsz == io->wcount)

static inline int io_transform_codec_buffer(io_t *, codec_t *, char *, 
        size_t *);

/**
 *  \defgroup io_t io_t - input/output abstraction object
 *  \{
 *      \par
 */

/**
 * \brief  Write the input stream to the output stream 
 *
 * Read all data from \p in and copy it to \p out
 *
 * \param out   output IO object
 * \param in    input IO object
 *
 * \return the number of bytes read from \p in and written to \p out 
 *         or \c -1 on error
 *
 */
ssize_t io_pipe(io_t *out, io_t *in)
{
    enum { BUFSZ = 4096 };
    char buf[BUFSZ];
    size_t count = 0;   /* # of bytes piped */
    ssize_t c;

    dbg_err_if (out == NULL);
    dbg_err_if (in == NULL);
    
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
 * Create a copy of \p io and store it to \p *pio.
 * The returned object will share the same underlaying IO device, the same
 * codecs connected to \p io and the same input and output buffers.
 * Buffers, codecs and IO devices will not be released until all \c io_t object
 * associated to it will be freed.
 *
 * \param io    the \c io_t object to be dupped
 * \param pio   on success will contain the duplicated \c io_t object
 *
 * \return \c 0 on success, not zero on failure
 *
 * \sa io_free
 */
int io_dup(io_t *io, io_t **pio)
{
    dbg_return_if (io == NULL, ~0);
    dbg_return_if (pio == NULL, ~0);
    
    io->refcnt++;

    *pio = io;

    return 0;
}

/**
 * \brief  Copy a block of data between two \c io_t objects
 *
 * Read from \p in a block of data \p size bytes long and write it to
 * the \p out output \c io_t object
 *
 * \param out    output \c io_t object
 * \param in     input \c io_t object
 * \param size   number of bytes to copy
 *
 * \return the number of bytes copied (that can be less the \p size 
 *         in case of EOF on \p in) or \c -1 on error. 
 */
ssize_t io_copy(io_t *out, io_t *in, size_t size)
{
    enum { BUFSZ = 4096 };
    char buf[BUFSZ];
    size_t rem = size;
    ssize_t c;

    dbg_err_if (in == NULL);
    dbg_err_if (out == NULL);

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
 *  will start at the given position.
 *  Note that not all \c io_t devices support seeking (e.g. sockets don't) so
 *  this function will always fail when used on those devices.
 *
 * \param io     the \c io_t object
 * \param off    absolute offset to move to
 *
 * \return the given offset on success, -1 on error
 */
ssize_t io_seek(io_t *io, size_t off)
{
    dbg_err_if (io == NULL);
    dbg_err_if (io_flush(io));

    if(io->seek)
        return io->seek(io, off);
err:
    return -1;
}

/**
 * \brief  Return the current file position
 *
 * Return the current file position.  There exists a unique read and write 
 * position offset.
 *
 * \param io     the \c io_t object
 *
 * \return the given offset on success, -1 on error
 */
ssize_t io_tell(io_t *io)
{
    dbg_err_if (io == NULL);
    dbg_err_if (io_flush(io));

    if(io->tell)
        return io->tell(io);
err:
    return -1;
}

/* alloc the read buffer */
static int io_rbuf_alloc(io_t *io)
{
    dbg_err_if (io == NULL);
    
    io->rbsz = IO_RD_BUFSZ; /* set the buffer size */

    /* if a dup'd io_t already alloc'd a buffer then use it */
    io->rbuf = (char*)u_malloc(io->rbsz);
    dbg_err_if(io->rbuf == NULL);
    io->roff = 0;

    io->ubuf = (char*)u_malloc(io->rbsz);
    dbg_err_if(io->ubuf == NULL);

    return 0;
err:
    return ~0;
}

/* alloc the write buffer */
static int io_wbuf_alloc(io_t *io)
{
    dbg_err_if (io == NULL);

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
    ssize_t c;
    size_t cavail, sdcount = *dcount; /* saved dcount */

    dbg_err_if (io == NULL);
    dbg_err_if (codec == NULL);
    dbg_err_if (src == NULL);
    //dbg_err_if (dst == NULL);
    dbg_err_if (dcount == NULL);

    if(codec == TAILQ_LAST(&io->codec_chain, codec_chain_s))
    {
        /* last codec in the chain */
        c = codec->transform(codec, dst, dcount, src, sz); 
        dbg_err_if(c == 0 && *dcount == 0);
        return c;
    } else {
        c = 0;
        do
        {
            *dcount = sdcount;
            if(codec->ccount)
                dbg_err_if(io_transform_codec_buffer(io, codec, dst, dcount));
            else
                *dcount = 0; /* no bytes written to 'dst' */

            c = 0; /* zero byte of 'src' consumed */
            cavail = CODEC_BUFSZ - codec->ccount - codec->coff;
            if(sz && cavail)
            {
                dbg_err_if((c = codec->transform(codec, 
                    codec->cbuf + codec->coff + codec->ccount, 
                    &cavail, src, sz)) < 0);

                codec->ccount += cavail;
                dbg_err_if(c == 0 && cavail == 0);
            }
        } while(c == 0 && *dcount == 0 && (codec->ccount || sz));

        return c;
    }

err:
    return -1;
}

static inline int io_transform_codec_buffer(io_t *io, codec_t *codec, 
    char *dst, size_t *dcount)
{
    ssize_t ct;

    dbg_err_if (io == NULL);
    dbg_err_if (codec == NULL);
    //dbg_err_if (dst == NULL);
    dbg_err_if (dcount == NULL);

    if(codec->ccount)
    {
        dbg_err_if((ct = io_transform(io, TAILQ_NEXT(codec, np), 
            dst, dcount, codec->cbuf + codec->coff,codec->ccount)) < 0);

        codec->ccount -= ct;

        if(codec->ccount > 0)
            codec->coff += ct;
        else
            codec->coff = 0;
    }

    return 0;
err:
    return ~0;
}

static inline ssize_t io_transfer(io_t *io, char *dst, size_t *dcount, 
        const char *src, size_t sz)
{
    dbg_err_if (io == NULL);
    dbg_err_if (src == NULL);
    //dbg_err_if (dst == NULL);
    dbg_err_if (dcount == NULL);

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
err:
    return -1;
}

static int io_chain_flush_chunk(io_t *io, char *dst, size_t *dcount)
{
    int er;
    codec_t *codec;
    size_t sz, sdcount;

    dbg_err_if (io == NULL);
    dbg_err_if (dst == NULL);
    dbg_err_if (dcount == NULL);

    sdcount = *dcount;

    TAILQ_FOREACH(codec, &io->codec_chain, np)
    {
        *dcount = sdcount;
        if(codec == TAILQ_LAST(&io->codec_chain, codec_chain_s))
        {
            return codec->flush(codec, dst, dcount);
        } else {
            for(;;)
            {
                *dcount = sdcount;
                if(codec->ccount)
                {
                    int rc = io_transform_codec_buffer(io, codec, dst, dcount);
                    dbg_err_if (rc);

                    if(*dcount)
                        return CODEC_FLUSH_CHUNK; /* call flush again */
                } else
                    *dcount = 0; /* no bytes written to 'dst' */

                sz = CODEC_BUFSZ - codec->ccount - codec->coff;
                dbg_err_if((er = codec->flush(codec, 
                    codec->cbuf + codec->coff + codec->ccount, &sz)) < 0);

                codec->ccount += sz;

                if(er == CODEC_FLUSH_COMPLETE && codec->ccount == 0)
                    break; /* flush of this codec completed */
            } /* for */
        }
    }

    return CODEC_FLUSH_COMPLETE;
err:
    return -1;
}

static int io_chain_flush(io_t *io)
{
    enum { BUFSZ = 4096 };
    int er;
    size_t sz, count;
    char buf[BUFSZ], *ptr;

    dbg_err_if (io == NULL);

    for(;;)
    {
        /* note: we cannot write straight to the wbuf otherwise codec 
         * transform & flush functions cannot call io_write safely; so we use 
         * a temp buffer and memcpy it to the wbuf after flushing */
        count = BUFSZ;
        if((er = io_chain_flush_chunk(io, buf, &count)) == CODEC_FLUSH_COMPLETE)
            break; /* flush complete */
        
        dbg_err_if(er < 0);

        for(ptr = buf; count; )
        {
            if(IO_WBUF_FULL(io))
                dbg_err_if(io_flush(io));

            sz = MIN(count, IO_WBUF_AVAIL(io));
            memcpy(io->wbuf + io->wcount, ptr, sz);

            count -= sz;
            io->wcount += sz;
            ptr += sz;
        }
    } 

    return 0;
err:
    return ~0;
}

/**
 * \brief  Free an \c io_t object
 *
 * Free the given \c io_t object. If \p io has been dup'd and the reference 
 * count is not zero then this function will only decrement it and return. 
 * Otherwise \p io will be flushed, the codec applied to it (if any) freed and
 * any other resource associated to it released.
 *
 * \param io     the \c io_t object to be free'd
 *
 * \return \c 0 on success, non-zero on error
 *
 * \sa io_dup
 */
int io_free(io_t *io)
{
    dbg_err_if (io == NULL);
    dbg_err_if (io->refcnt == 0);

    /* skip if this io_t has been dup'd and there are still one or more 
       references in use */
    if(--io->refcnt)
        return 0;

    /* flush, remove and free all codecs */
    dbg_if(io_codecs_remove(io));

    dbg_if(io_flush(io));

    /* free per dev resources */
    dbg_if(io->term(io));

    U_FREE(io->rbuf);
    U_FREE(io->ubuf);
    U_FREE(io->wbuf);
    U_FREE(io->name);
    U_FREE(io);

    return 0;
err:
    return ~0;
}

/* refill the read buffer */
static ssize_t io_underflow(io_t *io)
{
    ssize_t c;
    size_t sz;

    dbg_err_if (io == NULL);

    if(io->rbuf == NULL)
        dbg_err_if(io_rbuf_alloc(io)); /* alloc the read buffer */

    while(io->rcount == 0)
    {
        if(io->ucount == 0)
        {   /* fetch some bytes from the device and fill the rbuffer */
            dbg_err_if((c = io->read(io, io->ubuf, io->rbsz)) < 0);
            if(c == 0)
            {   /* eof, try to get some buffered (already transformed) bytes 
                   from the codec */
                if(!TAILQ_EMPTY(&io->codec_chain))
                {
                    sz = io->rbsz;
                    c = io_chain_flush_chunk(io, io->rbuf, &sz);
                    dbg_err_if(c < 0);
                    io->rcount += sz;
                    io->roff = 0;
                    if(c == 0)
                        io->eof++;
                }
                break;
            }
            io->ucount += c;
        }

        /* transform all data in the buffer */
        sz = io->rbsz - io->rcount;
        dbg_err_if((c = io_transfer(io, io->rbuf, &sz, 
            io->ubuf + io->uoff, io->ucount)) < 0);
        dbg_err_if(c < 0);
        dbg_err_if(c == 0 && sz == 0);
        io->ucount -= c;
        if(io->ucount == 0)
            io->uoff = 0;
        else
            io->uoff += c;

        io->rcount = sz;
        io->roff = 0;
    }

    return io->rcount;
err:
    return -1;
}

/**
 * \brief  Read a block of data from an \c io_t object
 *
 * Read \p size bytes from \p io and save them to \p buf (that must be big
 * enough).
 *
 * \param io    the \c io_t object onto which the read operation is performed
 * \param buf   the buffer that will contain the read bytes   
 * \param size  number of bytes to read
 *
 * \return the number of bytes read and saved to \p buf, \c 0 on end of file
 *         condition, and \c -1 on error.
 *
 */
ssize_t io_read(io_t *io, char *buf, size_t size)
{
    char *out = buf;
    size_t wr;
    ssize_t c;

    dbg_err_if (io == NULL);
    dbg_err_if (buf == NULL);
    
    if(io->eof)
        return 0;

    while(size)
    {
        if(io->rcount == 0)
        {
            dbg_err_if((c = io_underflow(io)) < 0);
            if(c == 0)
                break;
        }
        /* copy out data */
        wr = MIN(io->rcount, size); 
        memcpy(out, io->rbuf + io->roff, wr);
        io->rcount -= wr;
        io->roff += wr;
        out += wr;
        size -= wr;
    }

    return out - buf;
err:
    return -1;
}

/**
 * \brief  Write a string to \p io using printf-style format strings
 *
 * Printf-like function used to easily write strings to \p io using 
 * well-known printf format strings. See printf(3) manual for format 
 * description.
 *
 * \param io    the \c io_t object to write to
 * \param fmt   printf-style format string
 * \param ...   format string arguments
 *
 * \return the number of chars written on success, \c -1 on error
 */
ssize_t io_printf(io_t *io, const char *fmt, ...)
{
    enum { BUFSZ = 2048 };
    char buf[BUFSZ], *bbuf = NULL; 
    va_list ap, ap2;
    int sz;

    dbg_err_if (io == NULL);
    dbg_err_if (fmt == NULL);

    /* build the message to send to the log system */
    va_start(ap, fmt); /* init variable list arguments */

    sz = vsnprintf(buf, BUFSZ, fmt, ap);

    va_end(ap);

    if(sz >= BUFSZ)
    { /* stack buffer too small, alloc a bigger one on the heap */
        va_start(ap2, fmt); /* don't know if ap can be reused... */

        bbuf = (char*)u_malloc(++sz);
        dbg_err_if(bbuf == NULL);

        if((sz = vsnprintf(bbuf, sz, fmt, ap2)) > 0)
        {
            dbg_err_if(io_write(io, bbuf, sz) < 0);
        }

        va_end(ap2);

        U_FREE(bbuf);
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
 * \param io    the \c io_t object to be flushed
 *
 * \return zero on success, \c -1 on error
 */
ssize_t io_flush(io_t *io)
{
    ssize_t c;
    size_t off = 0;

    dbg_err_if (io == NULL);
 
    while(io->wcount)
    {
        c = io->write(io, io->wbuf + off, io->wcount);
        dbg_err_if(c < 0);
        if(c == 0)
            break;
    
        io->wcount -= c;
        off += c;
    }

    return 0;
err:
    return -1;
}

/**
 * \brief  Write a block of data to an \c io_t object
 *
 * Write \p size bytes of \p buf to \p io.
 *
 * \param io    the \c io_t object
 * \param buf   the buffer with the bytes to write
 * \param size  number of bytes to write
 *
 * \return the number of bytes written or -1 on error.
 */
ssize_t io_write(io_t *io, const char *buf, size_t size)
{
    size_t sz = 0, rem = size;
    ssize_t c = 0;

    dbg_err_if (io == NULL);
    dbg_err_if (buf == NULL);
    
    if(io->wbuf == NULL)
        dbg_err_if(io_wbuf_alloc(io)); /* alloc the write buffer */

    while(rem)
    {
        if(IO_WBUF_FULL(io)) /* if there's no more free space */
            dbg_err_if(io_flush(io));

        sz = IO_WBUF_AVAIL(io);
        c = io_transfer(io, io->wbuf + io->wcount, &sz, buf, rem);
        dbg_err_if(c < 0);
        dbg_err_if(c == 0 && sz == 0); /* some bytes MUST be read or written */

        io->wcount += sz;
        buf += c;
        rem -= c;
    }

    return size;
err:
    return -1;
}

/**
 * \brief  Write a char to an \c io_t object
 *
 * Write the character \p c to \p io
 *
 * \param io    the \c io_t object
 * \param c     the char to write
 *
 * \return the number of bytes written (i.e. \c 1) on success or \c -1 on error.
 */
inline ssize_t io_putc(io_t *io, char c)
{
    return io_write(io, &c, 1);
}

/**
 * \brief  Read a char from an \c io_t object
 *
 * Read a char from the \c io_t object \p io and save it at \p *pc.
 *
 * \param io    the \c io_t object
 * \param pc    on success will hold the read character
 *
 * \return the number of bytes read (i.e. \c 1) on success or \c -1 on error.
 */
inline ssize_t io_getc(io_t *io, char *pc)
{
    return io_read(io, pc, 1);
}

static inline char *io_strnchr(char *buf, size_t sz, char c)
{
    register char *p;

    dbg_goto_if (buf == NULL, end);

    p = buf;

    while(sz--)
    {
        if(*p == c)
            return p;
        ++p;
    }
end:
    return NULL;
}

/**
 * \brief  Read a line from an \c io_t object
 *
 * Read a line from \p in and save it to \p buf that must be at least \p size
 * bytes long.
 *
 * \param io    the \c io_t object
 * \param buf   destination buffer
 * \param size  size of \p buf
 *
 * \return the length of the line on success, \c 0 on EOF or \c -1 on error.
 */
ssize_t io_gets(io_t *io, char *buf, size_t size)
{
    ssize_t wr, c, len = 0;
    char *p;

    dbg_err_if (io == NULL);
    dbg_err_if (buf == NULL);

    if(size < 2)
        return -1; /* buf too small */

    --size; /* save a char for \0 */

    if(io->rcount == 0)
        dbg_err_if(io_underflow(io) < 0);

    if(io->rcount == 0)
        return 0;

    for(;;)
    {
        if((p = io_strnchr(io->rbuf + io->roff, io->rcount, '\n')) != NULL)
        {
            p++; /* jump over newline */
            wr = MIN(p - (io->rbuf + io->roff), size);
            memcpy(buf, io->rbuf + io->roff, wr);
            buf[wr] = 0;
            io->rcount -= wr;
            io->roff += wr;
            len += wr;
            break;
        } else {
            if(size >= io->rcount)
            {
                memcpy(buf, io->rbuf + io->roff, io->rcount);
                len += io->rcount;
                buf += io->rcount;
                size -= io->rcount;
                io->rcount = 0;
                io->roff = 0;
                dbg_err_if((c = io_underflow(io)) < 0);
                if(c == 0)
                    break;
            } else {
                /* buffer too small, return a partial line */
                memcpy(buf, io->rbuf + io->roff, size);
                len += size;
                io->rcount -= size;
                io->roff += size;
                break;
            }
        }
    }

    buf[len] = 0;
    return len; /* return the # of chars in the line (strlen(line)) */
err:
    return -1;
}

/**
 * \brief  Insert a codec at the head the codec chain
 *
 * \param io    the \c io_t object
 * \param c     the codec to append
 *
 * \return \c 0 on success, non-zero on error
 */
int io_codec_add_head(io_t *io, codec_t* c)
{
    dbg_return_if (io == NULL, ~0);
    dbg_return_if (c == NULL, ~0);

    /* insert the codec at the head of the chain */
    TAILQ_INSERT_HEAD(&io->codec_chain, c, np);

    return 0;
}

/**
 * \brief  Append a codec to the codec chain
 *
 * \param io    the \c io_t object
 * \param c     the codec to append
 *
 * \return \c 0 on success, non-zero on error
 */
int io_codec_add_tail(io_t *io, codec_t* c)
{
    dbg_return_if (io == NULL, ~0);
    dbg_return_if (c == NULL, ~0);

    /* insert the codec at the end of the chain */
    TAILQ_INSERT_TAIL(&io->codec_chain, c, np);

    return 0;
}

/**
 * \brief  Flush, remove and free all codecs in the codec chain
 *
 * \param io    the \c io_t object
 *
 * \return \c 0 on success, non-zero on error
 */
int io_codecs_remove(io_t *io)
{
    codec_t *codec;
    int rc = 0;

    dbg_return_if (io == NULL, ~0);

    if(!TAILQ_EMPTY(&io->codec_chain))
    {
        if(io->wbuf)
            dbg_ifb(io_chain_flush(io))
                rc++;

        while((codec = TAILQ_FIRST(&io->codec_chain)) != NULL)
        {
            TAILQ_REMOVE(&io->codec_chain, codec, np);
            codec_free(codec);
        }
    }

    return rc;
}

/**
 * \brief  Set the name of an \c io_t object
 *
 * Set the name of the given \p io to \p name.  A name is a label that can 
 * be used to store any naming scheme (file names, URI, etc.)
 *
 * \param io    the \c io_t object
 * \param name  the name to be given to \p io
 *
 * \return \c 0 on success, non-zero on error
 */
int io_name_set(io_t *io, const char *name)
{
    char *n;

    dbg_err_if (io == NULL);
    dbg_err_if (name == NULL);
    
    n = u_strdup(name);
    dbg_err_if(n == NULL);

    U_FREE(io->name);

    io->name = n;

    return 0;
err:
    return ~0;
}

/**
 * \brief  Return the name of the given \c io_t object
 *
 * Save in \p name the name of \p io.
 *
 * \param io    the \c io_t object
 * \param name  on success will contain the name of the given \p io
 * \param sz    size of \p name
 *
 * \return \c 0 on success, non-zero on error
 */
int io_name_get(io_t *io, char *name, size_t sz)
{
    size_t min = 0;

    dbg_err_if (io == NULL);
    dbg_err_if (io->name == NULL);
    dbg_err_if (name == NULL);
    dbg_err_if (sz < 2);

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

    dbg_err_if (pio == NULL);

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
    U_FREE(io);
    return -1;
}

/**
 *  \}
 */

