/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: server_s.h,v 1.15 2007/06/04 16:30:58 tat Exp $
 */

#ifndef _KLONE_SERVER_S_H_
#define _KLONE_SERVER_S_H_
#include <stdlib.h>
#include <klone/ppc.h>
#include <klone/backend.h>
#include <klone/klog.h>
#include <klone/timer.h>
#include <klone_conf.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT
#include <sys/select.h>
#endif
#include <u/libu.h>
#include "child.h"

enum { SERVER_MAX_CHILD_COUNT = 1024 };

struct server_s 
{
    u_config_t *config;     /* server config                                */
    ppc_t *ppc;             /* parent procedure call                        */
    backends_t bes;         /* backend list                                 */
    klog_t *klog;           /* klog device                                  */
    talarm_t *al_klog_flush;/* klog flush alarm                             */
    children_t *children;   /* children list                                */
    fd_set rdfds, wrfds, exfds;
    const char *chroot;     /* server chroot dir                            */
    int uid, gid;           /* uid/gid used to run the server               */
    int hfd;                /* highest set fd in fd_sets                    */
    size_t nbackend;        /* # of servers                                 */
    size_t nchild;          /* # of child (only in prefork mode)            */
    size_t max_child;       /* max # of children                            */
    /* int fork_child;    *//* # of child to fork when possible             */
    int stop;               /* >0 will stop the loop                        */
    int model;              /* server model                                 */
    int klog_flush;         /* >0 will flush the klog                       */
    int reap_children;      /* >0 will reap children (waitpid)              */
    int allow_root;         /* >0 allow root as the owner of kloned process */
    int blind_chroot;       /* if blind chroot mode is enabled or disabled  */
};

int server_get_backend_by_id(server_t *s, int id, backend_t **pbe);
int server_spawn_child(server_t *s, backend_t *be);
int server_cb_klog_flush(talarm_t *a, void *arg);

#endif
