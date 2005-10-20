#include <string.h>
#include <klone/klog.h>
#include <klone/context.h>
#include <klone/server.h>
#include <klone/ppc.h>
#include <klone/ppc_cmd.h>
#include "server_s.h"
#include "server_ppc_cmd.h"

/* struct used to for ppc command PPC_CMD_LOG_ADD */
typedef struct ppc_log_add_s
{
    int bid;                        /* calling backend ID       */
    int level;                      /* log level                */
    char log[U_MAX_LOG_LENGTH];     /* log line                 */
} ppc_log_add_t;

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

    nop_err_if(s->ppc == NULL);

    la.bid = ctx->backend->id;
    la.level = level;
    strncpy(la.log, str, U_MAX_LOG_LENGTH);

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
    server_t *s = (server_t *)vso;
    ppc_log_add_t *pla = (ppc_log_add_t*)data;
    backend_t *be;
    klog_t *kl;

    u_unused_args(ppc, fd, cmd, size);

    /* by default use server logger */
    kl = s->klog;

    /* get the logger obj of the calling backend (if any) */
    if(!server_get_backend(s, pla->bid, &be) && be->klog)
        kl = be->klog;

    /* log the line */
    if(kl)
        dbg_err_if(klog(kl, syslog_to_klog(pla->level), pla->log));

    return 0;
err:
    return ~0;
}


