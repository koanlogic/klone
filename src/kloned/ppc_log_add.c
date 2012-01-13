/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ppc_log_add.c,v 1.13 2007/11/09 22:06:26 tat Exp $
 */

#include "klone_conf.h"
#include <string.h>
#include <klone/klog.h>
#include <klone/context.h>
#include <klone/server.h>
#include <klone/ppc.h>
#include <klone/ppc_cmd.h>
#include <klone/server_ppc_cmd.h>
#include "server_s.h"

/* struct used for ppc command PPC_CMD_LOG_ADD */
struct ppc_log_add_s
{
    int bid;                        /* calling backend ID       */
    int level;                      /* log level                */
    char log[U_MAX_LOG_LENGTH];     /* log line                 */
};

typedef struct ppc_log_add_s ppc_log_add_t;

int syslog_to_klog(int level)
{
    static int klog_lev[] = 
    { 
        KLOG_EMERG,
        KLOG_ALERT,
        KLOG_CRIT,
        KLOG_ERR,
        KLOG_WARNING,
        KLOG_NOTICE,
        KLOG_INFO,
        KLOG_DEBUG
    };

    if(level < LOG_EMERG || level > LOG_DEBUG)
        return KLOG_ALERT;

    return klog_lev[level];
}

/* client function */
int server_ppc_cmd_log_add(server_t *s, int level, const char *str)
{
    ppc_log_add_t la;

    nop_err_if (s == NULL);
    nop_err_if (s->ppc == NULL);
    nop_err_if (str == NULL);

    memset(&la, 0, sizeof(ppc_log_add_t));

    la.bid = ctx->backend->id;
    la.level = level;
    u_strlcpy(la.log, str, sizeof la.log);

    /* send the command request */
    nop_err_if(ppc_write(s->ppc, ctx->pipc, PPC_CMD_LOG_ADD, (char*)&la, 
        sizeof(la)) < 0);

    return 0;
err:
    return ~0;
}

/* [parent] log a new message */
int server_ppc_cb_log_add(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size, void *vso)
{
    server_t *s;
    ppc_log_add_t *pla;
    backend_t *be;
    klog_t *kl;

    u_unused_args(ppc, fd, cmd, size);

    nop_err_if (vso == NULL);
    nop_err_if (data == NULL);

    pla = (ppc_log_add_t *) data;
    s = (server_t *) vso;

    /* by default use server logger */
    kl = s->klog;

    /* get the logger obj of the calling backend (if any) */
    if(!server_get_backend_by_id(s, pla->bid, &be) && be->klog)
        kl = be->klog;

    /* log the line */
    if(kl)
        nop_err_if(klog(kl, syslog_to_klog(pla->level), "%s", pla->log));

    return 0;
err:
    return ~0;
}

