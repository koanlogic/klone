/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ppc.c,v 1.12 2006/01/09 12:38:38 tat Exp $
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/ppc.h>

struct ppc_header_s
{
    unsigned char cmd;
    size_t size;
};

typedef struct ppc_header_s ppc_header_t;

struct ppc_s
{
    ppc_cb_t ftb[256];  /* function pointers            */
    void *arg[256];     /* opaque function arguments    */
};

int ppc_register(ppc_t *ppc, unsigned char cmd, ppc_cb_t cb, void *arg)
{
    dbg_err_if (ppc == NULL);
    dbg_err_if (cb == NULL);

    ppc->ftb[cmd] = cb;
    ppc->arg[cmd] = arg;

    return 0;
err:
    return ~0;
}

static ssize_t ppc_do_read(int fd, char *data, size_t size)
{
    ssize_t n;

    dbg_return_if (fd < 0, -1);
    dbg_return_if (data == NULL, -1);

again:
    n = read(fd, data, size);
    if(n < 0 && errno == EINTR)
        goto again;

    return n;
}

static ssize_t ppc_do_write(int fd, char *data, size_t size)
{
    ssize_t n;

    dbg_return_if (fd < 0, -1);
    dbg_return_if (data == NULL, -1);

again:
    n = write(fd, data, size);
    if(n < 0 && errno == EINTR)
        goto again;

    return n;
}

ssize_t ppc_write(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size)
{
    ssize_t n;
    ppc_header_t h;

    dbg_return_if (ppc == NULL, -1);
    dbg_return_if (data == NULL, -1);
    dbg_return_if (fd < 0, -1);

    memset(&h, 0, sizeof(ppc_header_t));
    h.cmd = cmd;
    h.size = size;

    n = ppc_do_write(fd, (char*)&h, sizeof(ppc_header_t));
    if(n <= 0) /* error */
        return n;

    n = ppc_do_write(fd, data, size);
    if(n <= 0) /* error */
        return n;

    return 1;
}

ssize_t ppc_read(ppc_t *ppc, int fd, unsigned char *pcmd, char *data, 
    size_t size)
{
    ppc_header_t h;
    ssize_t n;

    dbg_return_if (ppc == NULL, -1);
    dbg_return_if (pcmd == NULL, -1);
    dbg_return_if (data == NULL, -1);
    dbg_return_if (fd < 0, -1);

    n = ppc_do_read(fd, (char*)&h, sizeof(ppc_header_t));
    if(n <= 0) /* error or eof */
        return n;

    /* buffer too small or cmd bigger then max allowed size */
    dbg_return_ifm (h.size > size || h.size > PPC_MAX_DATA_SIZE, -1,
            "ppc error h.cmd: %d, h.size: %lu", h.cmd, (unsigned long) h.size); 

    n = ppc_do_read(fd, data, h.size);
    if(n <= 0) /* error or eof */
        return n;

    *pcmd = h.cmd;

    return h.size;
}

int ppc_dispatch(ppc_t *ppc, int fd, unsigned char cmd, char *data, size_t size)
{
    dbg_err_if (ppc == NULL);
    dbg_err_if (ppc->ftb[cmd] == NULL);

    ppc->ftb[cmd](ppc, fd, cmd, data, size, ppc->arg[cmd]);

    return 0;
err:
    return ~0;
}

int ppc_free(ppc_t *ppc)
{
    U_FREE(ppc);
    return 0;
}

int ppc_create(ppc_t **pppc)
{
    ppc_t *ppc = NULL;

    dbg_err_if (pppc == NULL);
    
    ppc = u_zalloc(sizeof(ppc_t));
    dbg_err_if(ppc == NULL);

    *pppc = ppc;

    return 0;
err:
    return ~0;
}

