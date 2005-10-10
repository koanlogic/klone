#ifndef _KLONE_CONTEXT_H_
#define _KLONE_CONTEXT_H_
#include <klone/klone.h>
#include <klone/debug.h>
#include <klone/server.h>
#include <klone/config.h>
#include <klone/backend.h>
#include <klone/os.h>
#include "conf.h"

/* global applicaton context */
typedef struct context_s
{
    server_t *server;   /* server object                   */
    config_t *config;   /* server config object            */
    backend_t *backend; /* the backend served by this child */
    char *ext_config;   /* additional external config file */
    int debug;          /* debugging on/off                */
    int daemon;         /* daemon/service mode on/off      */
    char **arg;         /* cmd line args array             */
    size_t narg;        /* # of cmd line args              */
    int pipc;           /* parent IPC socket descriptor    */

    #ifdef OS_WIN
    SERVICE_STATUS_HANDLE hServiceStatus;
    SERVICE_STATUS status;     
    enum { SERV_NOP, SERV_INSTALL, SERV_REMOVE } serv_op;        
                        /* install/remove service bindings */
    #endif
} context_t;

/* exported variable (see entry.c) */
extern context_t *ctx;

#endif
