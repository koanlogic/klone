/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 */

#include "klone_conf.h"
#include <unistd.h>
#include <klone/io.h>
#include <klone/ioprv.h>
#include <u/libu.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

struct io_ssl_s
{
    struct io_s io; /* must be the first item */
    SSL *ssl;
    int fd;
    int flags;
};

typedef struct io_ssl_s io_ssl_t;

static ssize_t io_ssl_read(io_ssl_t *io, char *buf, size_t size);
static ssize_t io_ssl_write(io_ssl_t *io, const char *buf, size_t size);

int io_ssl_get_SSL(io_t *io, SSL **pssl)
{
    io_ssl_t *io_ssl = (io_ssl_t*)io;

    dbg_err_if(io_ssl == NULL);
    dbg_err_if(pssl == NULL);
    dbg_err_if(io_ssl->io.type != IO_TYPE_SSL);

    dbg_err_if(io_ssl->ssl == NULL);

    *pssl = io_ssl->ssl;

    return 0;
err:
    return ~0;
}

static ssize_t io_ssl_read(io_ssl_t *io_ssl, char *buf, size_t size)
{
    ssize_t c;

    dbg_err_if (io_ssl == NULL);
    dbg_err_if (buf == NULL);

again:
    c = SSL_read(io_ssl->ssl, buf, size);
    /* if(c < 0 && (errno == EINTR || errno == EAGAIN)) */
    if(c < 0 && errno == EINTR)
        goto again; 

    dbg_err_if(c < 0); 

    return c;
err:
    return -1;
}

static ssize_t io_ssl_write(io_ssl_t *io_ssl, const char *buf, size_t size)
{
    ssize_t c;

    dbg_err_if (io_ssl == NULL);
    dbg_err_if (buf == NULL);

again:
    c = SSL_write(io_ssl->ssl, buf, size);
    /* if(c < 0 && (errno == EINTR || errno == EAGAIN)) */
    if(c < 0 && errno == EINTR)
        goto again; 

    dbg_err_if(c < 0); 

    return c;
err:
    return -1;
}

/* close the underlaying fd (may be called more then once) */
static int io_ssl_close(io_ssl_t *io_ssl)
{
    dbg_err_if(io_ssl == NULL);

    if(io_ssl->flags & IO_FD_CLOSE && io_ssl->fd != -1)
    {
        close(io_ssl->fd);
        io_ssl->fd = -1;
    }

    return 0;
err:
    return ~0;
}

/* free data alloc'ed by this io type */
static int io_ssl_free(io_ssl_t *io_ssl)
{
    dbg_err_if(io_ssl == NULL);

    dbg_if(io_ssl_close(io_ssl));

    if(io_ssl->ssl)
    {
        SSL_free(io_ssl->ssl);
        io_ssl->ssl = NULL;
    }

    return 0;
err:
    return -1;
}

static int io_ssl_connect(io_ssl_t *io_ssl)
{
    int rc;

    /* accept a SSL connection */
    while((rc = SSL_connect(io_ssl->ssl)) <= 0)
    {
        #ifdef SSL_OPENSSL
        /* will return 1 if accept has been blocked by a signal or async IO */
        if(BIO_sock_should_retry(rc))
            continue;
        #endif

        if(SSL_get_error(io_ssl->ssl, rc) == SSL_ERROR_WANT_READ)
            break; 

        warn_err("SSL accept error: %d", SSL_get_error(io_ssl->ssl, rc));
    }

    return 0;
err:
    return ~0;
}

static int io_ssl_accept(io_ssl_t *io_ssl)
{
    int rc;

    /* accept a SSL connection */
    while((rc = SSL_accept(io_ssl->ssl)) <= 0)
    {
        #ifdef SSL_OPENSSL
        /* will return 1 if accept has been blocked by a signal or async IO */
        if(BIO_sock_should_retry(rc))
            continue;
        #endif

        if(SSL_get_error(io_ssl->ssl, rc) == SSL_ERROR_WANT_READ)
            break; 

        warn_err("SSL accept error: %d", SSL_get_error(io_ssl->ssl, rc));
    }

    return 0;
err:
    return ~0;
}

int io_ssl_create(int fd, int flags, int client_mode, 
        SSL_CTX *ssl_ctx, io_t **pio)
{
    io_ssl_t *io_ssl = NULL;
    int rc = 0;
    long vfy;

    dbg_return_if (pio == NULL, ~0);
    dbg_return_if (ssl_ctx == NULL, ~0);

    dbg_err_if(io_create(io_ssl_t, (io_t**)&io_ssl));

    io_ssl->io.type = IO_TYPE_SSL;

    io_ssl->fd = fd;
    io_ssl->flags = flags;

    io_ssl->ssl = SSL_new(ssl_ctx);
    dbg_err_if(io_ssl->ssl == NULL);

    /* assign a working descriptor to the SSL stream */
    dbg_err_if(SSL_set_fd(io_ssl->ssl, fd) == 0);

    io_ssl->io.read = (io_read_op) io_ssl_read;
    io_ssl->io.write = (io_write_op) io_ssl_write;
    io_ssl->io.close = (io_close_op) io_ssl_close; 
    io_ssl->io.free = (io_free_op) io_ssl_free; 
    io_ssl->io.size = 0;

    /* set the secure flag (encrypted connection) */
    io_ssl->io.is_secure = 1;
    
    /* wait for the peer to start the TLS handshake */
    if(client_mode)
        dbg_err_if(io_ssl_connect(io_ssl));
    else
        dbg_err_if(io_ssl_accept(io_ssl));

    *pio = (io_t*)io_ssl;

    return 0;
err:
    if(io_ssl && io_ssl->ssl)
    {
        #ifdef SSL_OPENSSL
        /* print a warning message for bad client certificates */
        if((vfy = SSL_get_verify_result(io_ssl->ssl)) != X509_V_OK)
            warn("SSL client cert verify error: %s", 
                X509_verify_cert_error_string(vfy));
        SSL_set_shutdown(io_ssl->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
        #endif
        #ifdef SSL_CYASSL
        warn("SSL %s error", client_mode ? "connect" : "accept");
        #endif
    }
    if(io_ssl)
        io_free((io_t *)io_ssl);
    return ~0;
}

