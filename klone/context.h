/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: context.h,v 1.13 2009/10/23 14:08:28 tho Exp $
 */

#ifndef _KLONE_CONTEXT_H_
#define _KLONE_CONTEXT_H_

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/server.h>
#include <klone/backend.h>
#include <klone/os.h>
#include <klone/hook.h>

#ifdef __cplusplus
extern "C" {
#endif 

/* global applicaton context */
typedef struct context_s
{
    server_t *server;   /* server object                                */
    u_config_t *config; /* server config object                         */
    backend_t *backend; /* the backend served by this child             */
    hook_t *hook;       /* object that keep track of hooks              */
    char *ext_config;   /* additional external config file              */
    int cmd_config;     /* whether to override config from command-line */
    char *pid_file;     /* path of the PID file                         */
    int debug;          /* debugging on/off                             */
    int daemon;         /* daemon/service mode on/off                   */
    char **arg;         /* cmd line args array                          */
    size_t narg;        /* # of cmd line args                           */
    int pipc;           /* parent IPC socket descriptor                 */
    int cgi;            /* if we're in cgi mode                         */
    int nochdir;        /* do not chdir when daemon'izing               */

    #ifdef OS_WIN
    SERVICE_STATUS_HANDLE hServiceStatus;
    SERVICE_STATUS status;     
    enum { SERV_NOP, SERV_INSTALL, SERV_REMOVE } serv_op;        
                        /* install/remove service bindings */
    #endif
} context_t;

/* exported variable (see entry.c) */
extern context_t *ctx;

#ifdef __cplusplus
}
#endif 

#endif
