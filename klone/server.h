/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: server.h,v 1.16 2006/01/09 12:38:38 tat Exp $
 */

#ifndef _KLONE_SERVER_H_
#define _KLONE_SERVER_H_

#include "klone_conf.h"
#include <klone/ppc.h>
#include <klone/klog.h>

#ifdef __cplusplus
extern "C" {
#endif

struct u_config_s;
struct server_s;
typedef struct server_s server_t;

enum { 
    SERVER_LOG_FLUSH_TIMEOUT = 5,   /* min # of seconds between two log flush */

    /* fork/prefork */
    SERVER_MAX_CHILD = 300,         /* total # of child process per-server    */
    SERVER_MAX_BACKEND_CHILD = 150, /* max # of child allowed to run at once 
                                       per-backend */

    /* prefork server model limits */
    SERVER_PREFORK_START_CHILD = 2, /* # of child to run on startup           */
    SERVER_PREFORK_MAX_RQ_CHILD = 10000 /* max # of rq a process can serve    */
};

enum { 
    SERVER_MODEL_UNSET,     /* uninitialized                                */
    SERVER_MODEL_FORK,      /* fork for each incoming connection            */
    SERVER_MODEL_ITERATIVE, /* serialize responses                          */
    SERVER_MODEL_PREFORK,   /* prefork a few child to serve more clients    */
    #ifdef OS_UNIX
    SERVER_MODEL_DEFAULT = SERVER_MODEL_PREFORK
    #else
    SERVER_MODEL_DEFAULT = SERVER_MODEL_ITERATIVE
    #endif
};

int server_create(struct u_config_s *config, int model, server_t **ps);
int server_free(server_t *s);
int server_loop(server_t *s);
int server_cgi(server_t *s);
int server_stop(server_t *s);
ppc_t* server_get_ppc(server_t *s);

int server_get_logger(server_t *s, klog_t **pkl);
int server_foreach_memlog_line(server_t *s, int (*cb)(const char*, void*), 
    void *arg);

#ifdef __cplusplus
}
#endif 

#endif
