#ifndef _KLONE_PPC_COMMAND_H_
#define _KLONE_PPC_COMMAND_H_
#include <stdio.h>
#include <klone/server.h>

/* nop */
int server_ppc_cmd_nop(server_t *s);
int server_ppc_cb_nop(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size, void *vso);

/* add log */
int syslog_to_klog(int level);
int server_ppc_cmd_log_add(server_t *s, int level, const char *str);
int server_ppc_cb_log_add(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size, void *vso);

int server_ppc_cmd_fork_child(server_t *s);
int server_ppc_cb_fork_child(ppc_t *ppc, int fd, unsigned char cmd, char *data,
    size_t size, void *vso);

#endif
