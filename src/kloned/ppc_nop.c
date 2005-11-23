#include "klone_conf.h"
#include <string.h>
#include <klone/klog.h>
#include <klone/context.h>
#include <klone/server.h>
#include <klone/ppc.h>
#include <klone/ppc_cmd.h>
#include "server_s.h"
#include "server_ppc_cmd.h"

/* struct used for ppc command PPC_CMD_NOP */
typedef struct ppc_nop_s
{
    int dummy;
} ppc_nop_t;

/* client function */
int server_ppc_cmd_nop(server_t *s)
{
    ppc_nop_t nop;

    nop_err_if(s->ppc == NULL);

    /* send the command request */
    nop_err_if(ppc_write(s->ppc, ctx->pipc, PPC_CMD_NOP, (char*)&nop, 
        sizeof(nop)) < 0);

    return 0;
err:
    return ~0;
}

/* [parent] nop op */
int server_ppc_cb_nop(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size, void *vso)
{
    u_unused_args(ppc, fd, cmd, data, size, vso);

    dbg("ppc nop cmd callback");
    return 0;
}


