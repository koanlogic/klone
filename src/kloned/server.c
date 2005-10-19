#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <klone/server.h>
#include <klone/backend.h>
#include <klone/os.h>
#include <klone/timer.h>
#include <klone/context.h>
#include <klone/ppc.h>
#include <klone/ppc_cmd.h>
#include <klone/addr.h>
#include <klone/klog.h>
#include <u/libu.h>
#include "server_s.h"
#include "server_ppc_cmd.h"

#define SERVER_MAX_BACKENDS 8

enum watch_fd_e
{
    WATCH_FD_READ   = 1 << 1,
    WATCH_FD_WRITE  = 1 << 2,
    WATCH_FD_EXCP   = 1 << 3
};

static void server_watch_fd(server_t *s, int fd, unsigned int mode);
static void server_clear_fd(server_t *s, int fd, unsigned int mode);
static void server_close_fd(server_t *s, int fd);

static int server_be_listen(backend_t *be)
{
    enum { DEFAULT_BACKLOG = 10 };
    int d = 0, backlog = 0, val = 1;
    u_config_t *subkey;

    switch(be->addr->type)
    {
        case ADDR_IPV4:
            dbg_err_if((d = socket(AF_INET, SOCK_STREAM, 0)) < 0);
            dbg_err_if(setsockopt(d, SOL_SOCKET, SO_REUSEADDR, (void *)&val, 
                sizeof(int)) < 0);
            dbg_err_if( bind(d, (void*)&be->addr->sa.sin, 
                sizeof(struct sockaddr_in)));
            break;
        case ADDR_IPV6:
        case ADDR_UNIX:
        default:
            dbg_err_if("unupported addr type");
    }

    if(!u_config_get_subkey(be->config, "backlog", &subkey))
        backlog = atoi(u_config_get_value(subkey));

    if(!backlog)
        backlog = DEFAULT_BACKLOG;

    dbg_err_if(listen(d, backlog));

    be->ld = d;

    return 0;
err:
    dbg_strerror(errno);
    if(d)
        close(d);
    return ~0;
}

static void server_recalc_hfd(server_t *s)
{
    register int i;
    fd_set *prdfds, *pwrfds, *pexfds;

    prdfds = &s->rdfds;
    pwrfds = &s->wrfds;
    pexfds = &s->exfds;

    /* set s->hfd to highest value */
    for(i = s->hfd, s->hfd = 0; i > 0; --i)
    {
        if(FD_ISSET(i, prdfds) || FD_ISSET(i, pwrfds) || FD_ISSET(i, pexfds))
        {
            s->hfd = i;
            break;
        }
    }
}

static void server_clear_fd(server_t *s, int fd, unsigned int mode)
{
    if(mode & WATCH_FD_READ)
        FD_CLR(fd, &s->rdfds);

    if(mode & WATCH_FD_WRITE)
        FD_CLR(fd, &s->wrfds);

    if(mode & WATCH_FD_EXCP)
        FD_CLR(fd, &s->exfds);

    server_recalc_hfd(s);
}

static void server_watch_fd(server_t *s, int fd, unsigned int mode)
{
    if(mode & WATCH_FD_READ)
        FD_SET(fd, &s->rdfds);

    if(mode & WATCH_FD_WRITE)
        FD_SET(fd, &s->wrfds);

    if(mode & WATCH_FD_EXCP)
        FD_SET(fd, &s->exfds);

    s->hfd = MAX(s->hfd, fd);
}

static void server_close_fd(server_t *s, int fd)
{
    server_clear_fd(s, fd, WATCH_FD_READ | WATCH_FD_WRITE | WATCH_FD_EXCP);
    close(fd);
}


static int server_be_accept(server_t *s, backend_t *be, int* pfd)
{
    struct sockaddr sa;
    int sa_len = sizeof(struct sockaddr);
    int ad;

    u_unused_args(s);

again:
    ad = accept(be->ld, &sa, &sa_len);
    if(ad == -1 && errno == EINTR)
        goto again; /* interrupted */
    dbg_err_if(ad == -1); /* accept error */

    *pfd = ad;

    return 0;
err:
    if(ad < 0)
        dbg_strerror(errno);
    return ~0;
}

static int server_backend_detach(server_t *s, backend_t *be)
{
    s->nserver--;

    addr_free(be->addr);
    be->server = NULL;
    be->addr = NULL;
    be->config = NULL;

    close(be->ld);
    be->ld = -1;

    backend_free(be);

    return 0;
}

static int cb_term_child(alarm_t *al, void *arg)
{
    pid_t child = (int)arg;

    u_unused_args(al);
    
    dbg("sending SIGTERM to child [%d]", child);

    dbg_err_if(kill(child, SIGTERM) == -1);

    return 0;
err:
    dbg_strerror(errno);
    return ~0;
}

static int server_child_serve(server_t *s, backend_t *be, int ad)
{
    pid_t child;
    int socks[2];

    u_unused_args(s);

    /* create a parent<->child IPC channel */
    dbg_err_if(socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0);

    if((child = fork()) == 0)
    {   /* child */

        /* close on end of the channel */
        close(socks[0]);

        /* save parent IPC socket and close the other */
        ctx->pipc = socks[1];
        ctx->backend = be;

        /* close this be listening descriptor */
        close(be->ld);

        /* serve the page */
        dbg_if(backend_serve(be, ad));

        /* close client socket and die */
        close(ad);
        server_stop(be->server); 

    } else if(child > 0) {
        /* parent */

        /* close one end of the channel */
        close(socks[1]);

        /* close the accepted socket */
        close(ad);

        /* watch the socket connected to the child */
        server_watch_fd(s, socks[0], WATCH_FD_READ);
    } else {
        warn_err("fork error");
    }

    return 0;
err:
    warn_strerror(errno);
    return ~0;
}

static int server_be_serve(server_t *s, backend_t *be, int ad)
{
    switch(be->model)
    {
    case SERVER_MODEL_FORK:
        /* spawn a child to handle the request */
        dbg_err_if(server_child_serve(s, be, ad));
        break;

    case SERVER_MODEL_ITERATIVE:
        /* serve the page */
        dbg_if(backend_serve(be, ad));
        close(ad);
        break;

    default:
        warn_err_if("server model not supported");
    }

    return 0;
err:
    return ~0;
}

int server_stop(server_t *s)
{
    /* this could be protected to avoid races but it's not really needed */
    s->stop = 1;

    return 0;
}

static int server_listen(server_t *s)
{
    backend_t *be;

    LIST_FOREACH(be, &s->bes, np)
    {
        /* bind to be->addr */
        dbg_err_if(server_be_listen(be));

        /* watch the listening socket */
        server_watch_fd(s, be->ld, WATCH_FD_READ);
    }

    return 0;
err:
    return ~0;
}

int server_cgi(server_t *s)
{
    backend_t *be;

    /* use the first http backend as the CGI backend */
    LIST_FOREACH(be, &s->bes, np)
    {
        if(strcasecmp(be->proto, "http") == 0)
        {
            dbg_if(backend_serve(be, 0));
            return 0;
        }
    }

    return ~0;
}

ppc_t* server_get_ppc(server_t *s)
{
    return s->ppc;
}

static int server_process_ppc(server_t *s, int fd)
{
    unsigned char cmd;
    char data[PPC_MAX_DATA_SIZE];
    ssize_t n;

    /* get a ppc request */
    n = ppc_read(s->ppc, fd, &cmd, data, PPC_MAX_DATA_SIZE); 
    if(n > 0)
    {   
        /* process a ppc (parent procedure call) request */
        dbg_err_if(ppc_dispatch(s->ppc, fd, cmd, data, n));
    } else if(n == 0) {
        /* child has exit or closed the channel. close our side of the sock 
           and remove it from the watch list */
        server_close_fd(s, fd);
    } else {
        /* ppc error. close fd and remove it from the watch list */
        server_close_fd(s, fd);
    }

    return 0;
err:
    return ~0;
}

static int server_dispatch(server_t *s, int fd)
{
    backend_t *be;
    int ad; 

    /* find the backend that listen on fd */
    LIST_FOREACH(be, &s->bes, np)
    {
        if(be->ld == fd)
        {
            /* accept the pending connection */
            dbg_if(server_be_accept(s, be, &ad));

            dbg_if(server_be_serve(s, be, ad));

            return 0;
        }
    }

    /* a child is calling, get a ppc (parent procedure call) request */
    dbg_if(server_process_ppc(s, fd));

    return 0;
}

int server_loop(server_t *s)
{
    struct timeval tv;
    int rc, fd;
    fd_set rdfds, wrfds, exfds;

    dbg_err_if(s == NULL || s->config == NULL);
    
    dbg_err_if(server_listen(s));

    for(; !s->stop; )
    {
        memcpy(&rdfds, &s->rdfds, sizeof(fd_set));
        memcpy(&wrfds, &s->wrfds, sizeof(fd_set));
        memcpy(&exfds, &s->exfds, sizeof(fd_set));

        /* wake up every second */
        tv.tv_sec = 1; tv.tv_usec = 0;

    again:
        rc = select(1 + s->hfd, &rdfds, &wrfds, &exfds, &tv); 
        if(rc == -1 && errno == EINTR)
            goto again; /* interrupted */
        dbg_err_if(rc == -1); /* select error */

        /* for each signaled listening descriptor */
        for(fd = 0; rc && fd < 1 + s->hfd; ++fd)
        { 
            if(FD_ISSET(fd, &rdfds))
            {
                --rc;
                /* dispatch the request to the right backend */
                server_dispatch(s, fd);
            } 
        } /* for each ready fd */

    } /* infinite loop */

    return 0;
err:
    return ~0;
}


int server_free(server_t *s)
{
    backend_t *be;

    dbg_err_if(s == NULL);

    if(s->klog)
    {
        klog_close(s->klog);
        s->klog = NULL;
    }

    while((be = LIST_FIRST(&s->bes)) != NULL)
    {
        LIST_REMOVE(be, np);
        server_backend_detach(s, be);
    }

    dbg_if(ppc_free(s->ppc));

#ifdef OS_WIN
    WSACleanup();
#endif

    u_free(s);
    return 0;
err:
    return ~0;
}

static int server_setup_backend(server_t *s, backend_t *be)
{
    u_config_t *subkey;

    /* server count */
    s->nserver++;

    /* parse and create the bind addr_t */
    dbg_err_if(u_config_get_subkey(be->config, "addr", &subkey));

    dbg_err_if(addr_create(&be->addr));

    dbg_err_if(addr_set_from_config(be->addr, subkey));

    return 0;
err:
    if(be->addr)
    {
        addr_free(be->addr);
        be->addr = NULL;
    }
    return ~0;
}

static int server_log_hook(void *arg, int level, const char *str)
{
    server_t *s = (server_t*)arg;
    u_log_hook_t old = NULL;
    void *old_arg = NULL;

    /* if both the server and the calling backend has no log then exit */
    if(s->klog == NULL && (ctx->backend == NULL || ctx->backend->klog == NULL))
        return 0; /* log is disabled */

    /* disable log hooking in the hook itself otherwise an infinite loop 
       may happen if a log function is called from inside the hook */
    u_log_set_hook(NULL, NULL, &old, &old_arg);

    if(ctx->pipc)
    {   /* children context */
        dbg_err_if(server_ppc_cmd_log_add(s, level, str));
    } else {
        /* parent context */
        if(s->klog)
            dbg_err_if(klog(s->klog, syslog_to_klog(level), str));
    }

    /* re-set the old hook */
    u_log_set_hook(old, old_arg, NULL, NULL);

    return 0;
err:
    if(old)
        u_log_set_hook(old, old_arg, NULL, NULL);
    return ~0;
}

int server_get_logger(server_t *s, klog_t **pkl)
{
    klog_t *kl = NULL;

    kl = s->klog; /* may be NULL */

    if(ctx->backend)
        kl = ctx->backend->klog; /* may be NULL */

    *pkl = kl;

    return 0;
}

int server_get_backend(server_t *s, int id, backend_t **pbe)
{
    backend_t *be;

    LIST_FOREACH(be, &s->bes, np)
    {
        if(be->id == id)
        {
            *pbe = be;
            return 0;
        }
    }

    return ~0; /* not found */
}

int server_create(u_config_t *config, int model, server_t **ps)
{
    server_t *s = NULL;
    u_config_t *bekey = NULL, *log_c = NULL;
    backend_t *be = NULL;
    const char *list, *type;
    char *n = NULL, *name = NULL;
    int i, id;

#ifdef OS_WIN
    WORD ver;
    WSADATA wsadata;

    ver = MAKEWORD(1,1);
    dbg_err_if(WSAStartup(ver, &wsadata));
#endif

    s = u_zalloc(sizeof(server_t));
    dbg_err_if(s == NULL);

    *ps = s; /* we need it before backend inits */

    s->config = config;
    s->model = model;

    /* init fd_set */
    memset(&s->rdfds, 0, sizeof(fd_set));
    memset(&s->wrfds, 0, sizeof(fd_set));
    memset(&s->exfds, 0, sizeof(fd_set));

    /* init backend list */
    LIST_INIT(&s->bes);

    dbg_err_if(ppc_create(&s->ppc));

    /* create the log device if requested */
    if(!u_config_get_subkey(config, "log", &log_c))
        dbg_if(klog_open_from_config(log_c, &s->klog));

    /* register the log ppc callbacks */
    dbg_err_if(ppc_register(s->ppc, PPC_CMD_LOG_ADD, server_ppc_cb_log_add, s));

    /* redirect logs to the server_log_hook function */
    dbg_err_if(u_log_set_hook(server_log_hook, s, NULL, NULL));

    /* parse server list and build s->bes */
    list = u_config_get_subkey_value(config, "server_list");
    dbg_err_if(list == NULL);

    name = n = u_zalloc(strlen(list) + 1);
    dbg_err_if(name == NULL);
    
    /* load config and init backend for each server in server.list */
    for(i = strlen(list), id = 0; 
        i > 0 && sscanf(list, "%[^ \t]", name); 
        i -= 1 + strlen(name), list += 1 + strlen(name), name[0] = 0)
    {
        dbg("configuring backend: %s", name);

        /* just SERVER_MAX_BACKENDS supported */
        dbg_err_if(s->nserver == SERVER_MAX_BACKENDS);

        /* get config tree of this backend */
        warn_err_ifm(u_config_get_subkey(config, name, &bekey),
            "missing [%s] backend configuration", name);

        type = u_config_get_subkey_value(bekey, "type");
        dbg_err_if(type == NULL);

        /* create a new backend and push into the be list */
        dbg_err_if(backend_create(type, bekey, &be));

        be->server = s;
        be->config = bekey;
        be->id = id++;
        if(be->model == SERVER_MODEL_UNSET)
            be->model = s->model; /* inherit server model */

        /* create the log device (may fail if logging is not configured) */
        if(!u_config_get_subkey(bekey, "log", &log_c))
            dbg_if(klog_open_from_config(log_c, &be->klog));

        #ifdef OS_WIN
        if(be->model == SERVER_MODEL_FORK)
            warn_err("child-based server model is not "
                     "yet supported on Windows");
        #endif

        LIST_INSERT_HEAD(&s->bes, be, np);

        dbg_err_if(server_setup_backend(s, be));
    }

    u_free(n);

    return 0;
err:
    if(n)
        u_free(n);
    if(s)
        server_free(s);
    return ~0;
}

