#include <string.h>
#include <klone/klog.h>
#include <klone/context.h>
#include <klone/server.h>
#include <klone/ppc.h>
#include <klone/ppc_cmd.h>
#include "server_s.h"
#include "server_ppc_cmd.h"

/* struct used for ppc command PPC_CMD_NOP */
typedef struct ppc_fork_child_s
{
    int bid; /* backend id */
} ppc_fork_child_t;

/* client function */
int server_ppc_cmd_fork_child(server_t *s)
{
    ppc_fork_child_t pfc;

    nop_err_if(s->ppc == NULL);

    pfc.bid = ctx->backend->id;

    /* send the command request */
    nop_err_if(ppc_write(s->ppc, ctx->pipc, PPC_CMD_FORK_CHILD, (char*)&pfc, 
        sizeof(pfc)) < 0);

    return 0;
err:
    return ~0;
}

/* [parent] nop op */
int server_ppc_cb_fork_child(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size, void *vso)
{
    backend_t *be = NULL;
    ppc_fork_child_t *ppfc = (void*)data;

    u_unused_args(ppc, fd, cmd, data, size, vso);

    dbg_err_if(server_get_backend(ctx->server, ppfc->bid, &be) || be == NULL);

    dbg("[parent] ppc spawn child");

    dbg_if(server_spawn_child(ctx->server, be));

    return 0;
err:
    return ~0;
}


