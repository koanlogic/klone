#ifndef _KLONE_SERVER_S_H_
#define _KLONE_SERVER_S_H_
#include <stdlib.h>
#include <klone/ppc.h>
#include <klone/backend.h>
#include <klone/klog.h>
#include <sys/types.h>
#include <u/libu.h>

struct server_s 
{
    u_config_t *config;     /* server config                    */
    ppc_t *ppc;             /* parent procedure call            */
    backends_t bes;         /* backend list                     */
    klog_t *klog;           /* klog device                      */
    fd_set rdfds, wrfds, exfds;
    int hfd;                /* highest set fd in fd_sets        */
    size_t nserver;         /* # of servers                     */
    int stop;               /* >0 will stop the loop            */
    int model;              /* server model                     */
};

int server_get_backend(server_t *s, int id, backend_t **pbe);

#endif
