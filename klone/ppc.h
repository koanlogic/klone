/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ppc.h,v 1.10 2006/01/09 12:38:37 tat Exp $
 */

#ifndef _KLONE_PPC_H_
#define _KLONE_PPC_H_

#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { PPC_MAX_DATA_SIZE = 8192 }; 

struct ppc_s;
typedef struct ppc_s ppc_t;

typedef int (*ppc_cb_t)(ppc_t*, int fd, unsigned char cmd, char *data, 
    size_t size, void*arg);

int ppc_create(ppc_t **pppc);
int ppc_free(ppc_t *ppc);
int ppc_register(ppc_t *ppc, unsigned char cmd, ppc_cb_t func, void *arg);
int ppc_dispatch(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size);
ssize_t ppc_write(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size);
ssize_t ppc_read(ppc_t *ppc, int fd, unsigned char *cmd, char *data, 
    size_t size);

#ifdef __cplusplus
}
#endif 

#endif
