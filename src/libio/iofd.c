/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: iofd.c,v 1.14 2007/07/20 14:12:30 tat Exp $
 */

#include "klone_conf.h"
#include <fcntl.h>
#include <unistd.h>
#include <u/libu.h>
#include <klone/io.h>
#include <klone/io.h>
#include <klone/ioprv.h>

struct io_fd_s
{
    struct io_s io; /* must be the first item */
    int fd;
    int flags;
    int issock;     /* >0 if fd is a SOCKET (win32 only) */
};

typedef struct io_fd_s io_fd_t;

static ssize_t io_fd_read(io_fd_t *io, char *buf, size_t size);
static ssize_t io_fd_write(io_fd_t *io, const char *buf, size_t size);
static ssize_t io_fd_seek(io_fd_t *io, size_t off);
static ssize_t io_fd_tell(io_fd_t *io);
static int io_fd_close(io_fd_t *io);
static int io_fd_free(io_fd_t *io);

static ssize_t io_fd_read(io_fd_t *ifd, char *buf, size_t size)
{
    ssize_t c;

    dbg_return_if (ifd == NULL, ~0);
    dbg_return_if (buf == NULL, ~0);
    
again:
    if(!ifd->issock)
        c = read(ifd->fd, buf, size);
    else
        c = recv(ifd->fd, buf, size, 0);
    /* if(c < 0 && (errno == EINTR || errno == EAGAIN)) */
    if(c < 0 && errno == EINTR)
        goto again; 

    dbg_err_sif(c == -1); 

    return c;
err:
    return -1;
}

static ssize_t io_fd_write(io_fd_t *ifd, const char *buf, size_t size)
{
    ssize_t c;

    dbg_return_if (ifd == NULL, ~0);
    dbg_return_if (buf == NULL, ~0);
 
again:
    if(!ifd->issock)
        c = write(ifd->fd, buf, size);
    else
        c = send(ifd->fd, buf, size, 0);
    /* if(c < 0 && (errno == EINTR || errno == EAGAIN)) */
    if(c < 0 && errno == EINTR)
        goto again; 

    dbg_err_sif(c == -1); 

    return c;
err:
    return -1;
}

static ssize_t io_fd_seek(io_fd_t *ifd, size_t off)
{
    dbg_return_if (ifd == NULL, -1);

    return lseek(ifd->fd, off, SEEK_SET);
}

static ssize_t io_fd_tell(io_fd_t *ifd)
{
    dbg_return_if (ifd == NULL, -1);

    return lseek(ifd->fd, 0, SEEK_CUR);
}

/* close the underlaying fd (may be called more then once) */
static int io_fd_close(io_fd_t *ifd)
{
    dbg_return_if (ifd == NULL, ~0);
    
    if(ifd->flags & IO_FD_CLOSE && ifd->fd != -1)
    {
#ifdef OS_WIN
        if(ifd->issock)
        {
            closesocket(ifd->fd);
        } else
            close(ifd->fd);
#else
            close(ifd->fd);
#endif
        ifd->fd = -1;
    }

    return 0;
}

static int io_fd_free(io_fd_t *ifd)
{
    dbg_if(io_fd_close(ifd));

    return 0;
}

int io_fd_get_d(io_t *io)
{
    io_fd_t *ifd = (io_fd_t*)io;
    dbg_err_if(io == NULL);
    dbg_err_if(ifd->io.type != IO_TYPE_FD);

    return ifd->fd;
err:
    return -1;
}

int io_fd_create(int fd, int flags, io_t **pio)
{
    io_fd_t *ifd = NULL;

    dbg_err_if (pio == NULL);

    dbg_err_if(io_create(io_fd_t, (io_t**)&ifd));

    ifd->io.type = IO_TYPE_FD;

    ifd->fd = fd;
    ifd->flags = flags;
    ifd->io.read = (io_read_op) io_fd_read;
    ifd->io.write = (io_write_op) io_fd_write;
    ifd->io.seek = (io_seek_op) io_fd_seek;
    ifd->io.tell = (io_tell_op) io_fd_tell;
    ifd->io.close = (io_close_op) io_fd_close; 
    ifd->io.free = (io_free_op) io_fd_free; 
    
#ifdef OS_WIN
    u_long ret;
    if(ioctlsocket(fd, FIONREAD, &ret) == 0)
        ifd->issock++; /* is a socket, use recv/send instead of read/write  */
    _setmode(fd, _O_BINARY); /* we never want Windows CR/LF translation */
#endif

    *pio = (io_t*)ifd;

    return 0;
err:
    if(ifd)
        io_free((io_t*)ifd);
    return ~0;
}
