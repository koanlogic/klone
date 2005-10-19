#ifndef _KLONE_PPC_COMMAND_H_
#define _KLONE_PPC_COMMAND_H_
#include <stdio.h>
#include <klone/server.h>

/* helpers */
int syslog_to_klog(int level);

/* add log */
int server_ppc_cmd_log_add(server_t *s, int level, const char *str);
int server_ppc_cb_log_add(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size, void *vso);

#endif
