/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: backend.h,v 1.17 2008/06/04 17:48:01 tat Exp $
 */

#ifndef _KLONE_BACKEND_H_
#define _KLONE_BACKEND_H_

#include <u/libu.h>
#include <klone/klog.h>

#ifdef __cplusplus
extern "C" {
#endif 

struct server_s;

/* define page list */
LIST_HEAD(backends_s, backend_s);

struct backend_s
{
    /* statically initialized values */
    const char *proto;
    int (*cb_init)(struct backend_s *);
    int (*cb_serve)(struct backend_s *, int);
    int (*cb_term)(struct backend_s *);

    /* runtime values */
    struct server_s *server;
    u_config_t *config;
    u_net_addr_t *na;
    int model;
    int ld;
    void *arg;
    klog_t *klog;
    int id;
    size_t nchild;
    size_t max_child;
    size_t start_child;
    size_t max_rq_xchild;
    size_t fork_child;
    pid_t *child_pid;           /* pid of children              */
    LIST_ENTRY(backend_s) np;   /* next & prev pointers         */
};

typedef struct backend_s backend_t;
typedef struct backends_s backends_t; /* backend_t list        */

#define BACKEND_STATIC_INITIALIZER(proto, init, connect, term)  \
    {                                                           \
        proto,                                                  \
        init,                                                   \
        connect,                                                \
        term,                                                   \
        NULL,   /* server       */                              \
        NULL,   /* config       */                              \
        NULL,   /* addr         */                              \
        0,      /* model        */                              \
        -1,     /* ld           */                              \
        NULL,   /* arg          */                              \
        NULL,   /* klog         */                              \
        -1,     /* id           */                              \
        0,      /* nchild       */                              \
        0,      /* max_child    */                              \
        0,      /* start_child  */                              \
        0,      /* max_rq_xchild*/                              \
        0,      /* fork_child   */                              \
        NULL,   /* children pids*/                              \
        LIST_ENTRY_NULL                                         \
    }


extern backend_t *backend_list[];

int backend_create(const char *name, u_config_t *, backend_t **);
int backend_serve(backend_t *, int fd);
int backend_free(backend_t *);

#ifdef __cplusplus
}
#endif 

#endif
