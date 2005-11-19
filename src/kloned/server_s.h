#ifndef _KLONE_SERVER_S_H_
#define _KLONE_SERVER_S_H_
#include <stdlib.h>
#include <klone/ppc.h>
#include <klone/backend.h>
#include <klone/klog.h>
#include <klone/timer.h>
#include <sys/types.h>
#include <u/libu.h>

enum { SERVER_MAX_CHILD_COUNT = 1024 };

struct server_s 
{
    u_config_t *config;     /* server config                    */
    ppc_t *ppc;             /* parent procedure call            */
    backends_t bes;         /* backend list                     */
    klog_t *klog;           /* klog device                      */
    alarm_t *al_klog_flush; /* klog flush alarm                 */
    fd_set rdfds, wrfds, exfds;
    pid_t child_pid[SERVER_MAX_CHILD_COUNT]; /* children pid    */
    char *chroot;           /* server chroot dir                */
    int uid, gid;           /* uid/gid used to run the server   */
    int hfd;                /* highest set fd in fd_sets        */
    size_t nbackend;        /* # of servers                     */
    size_t nchild;          /* # of child (only in prefork mode)*/
    int stop;               /* >0 will stop the loop            */
    int model;              /* server model                     */
    int klog_flush;         /* >0 will flush the klog           */
    int reap_childs;        /* >0 will reap children (waitpid)  */
};

int server_get_backend(server_t *s, int id, backend_t **pbe);
int server_spawn_child(server_t *s, backend_t *be);

#endif
