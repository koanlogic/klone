/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ppc_fork_child.c,v 1.10 2007/11/09 22:06:26 tat Exp $
 */

#include "klone_conf.h"
#include <string.h>
#include <klone/klog.h>
#include <klone/context.h>
#include <klone/server.h>
#include <klone/backend.h>
#include <klone/ppc.h>
#include <klone/ppc_cmd.h>
#include <klone/server_ppc_cmd.h>
#include "server_s.h"

/* struct used for ppc command PPC_CMD_NOP */
struct ppc_fork_child_s
{
    int bid; /* backend id */
};

typedef struct ppc_fork_child_s ppc_fork_child_t;

/* client function */
int server_ppc_cmd_fork_child(server_t *s, backend_t *be)
{
    ppc_fork_child_t pfc;

    nop_err_if (s == NULL);
    nop_err_if (s->ppc == NULL);
    nop_err_if (be == NULL);

    memset(&pfc, 0, sizeof(ppc_fork_child_t));

    pfc.bid = be->id;

    /* send the command request */
    nop_err_if(ppc_write(s->ppc, ctx->pipc, PPC_CMD_FORK_CHILD, (char*)&pfc, 
        sizeof(ppc_fork_child_t)) < 0);

    return 0;
err:
    return ~0;
}

/* [parent] */
int server_ppc_cb_fork_child(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size, void *vso)
{
    backend_t *be = NULL;
    ppc_fork_child_t *ppfc;

    u_unused_args(ppc, fd, cmd, size, vso);

    dbg_err_if (data == NULL);

    ppfc = (void *) data;

    dbg_err_if(server_get_backend_by_id(ctx->server, ppfc->bid, &be) || 
        be == NULL);

    u_dbg("[parent] ppc spawn child");

    /* try to fork now, if we can't (resource limit or max_child exceeded) 
       then try later */
    if(server_spawn_child(ctx->server, be))
        be->fork_child++; /* increase # of child to spawn when possible */

    return 0;
err:
    return ~0;
}

