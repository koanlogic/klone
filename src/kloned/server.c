#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <klone/server.h>
#include <klone/backend.h>
#include <klone/os.h>
#include <klone/timer.h>
#include <klone/context.h>
#include <klone/ppc.h>
#include <klone/ppc_cmd.h>
#include <klone/addr.h>
#include <klone/utils.h>
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
    enum { DEFAULT_BACKLOG = 128 };
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

/* remove a child process whose pid is 'pid' to children list */
static void server_reap_child(server_t *s, pid_t child)
{
    pid_t *pid;
    register int i;

    if(s->nchild)
    {
        pid = s->child_pid;
        for(i = 0; i < SERVER_MAX_CHILD_COUNT; ++i, ++pid)
        {
            if(*pid == child)
            {
                *pid = 0;
                break;
            }
        }

        if(i < SERVER_MAX_CHILD_COUNT)
            s->nchild--; /* decrement child count */
    }
}

/* add a child to the list */
static void server_add_child(server_t *s, pid_t child)
{
    pid_t *pid;
    register int i;

    pid = s->child_pid;
    for(i = 0; i < SERVER_MAX_CHILD_COUNT; ++i, ++pid)
    {
        if(*pid == 0)
        {   /* found an empty slot */
            dbg("new child [%lu]", child);
            *pid = child;
            s->nchild++;
            break;
        }
    }
}

/* send 'sig' signal to all children process */
static void server_signal_childs(server_t *s, int sig)
{
    pid_t *pid;
    register int i;

    if(s->nchild)
    {
        pid = s->child_pid;
        for(i = 0; i < SERVER_MAX_CHILD_COUNT; ++i, ++pid)
        {
            if(*pid != 0)
            {
                dbg("killing child [%lu]", *pid);
                dbg_err_if(kill(*pid, sig) < 0);
            }
        }

        if(i == SERVER_MAX_CHILD_COUNT)
            return; /* no child found */

        dbg_err_if(s->nchild == 0);
        s->nchild--; /* decrement child count */
    }

    return;
err:
    dbg_strerror(errno);
}

static void server_sigint(int sig)
{
    u_unused_args(sig);
    dbg("SIGINT");
    server_stop(ctx->server);
}

static void server_sigterm(int sig)
{
    u_unused_args(sig);
    dbg("SIGTERM");
    server_stop(ctx->server);
}

static void server_sigchld(int sig)
{
    pid_t pid = -1;
    server_t *s = ctx->server;
    int status;

    u_unused_args(sig);

    /* detach from child processes */
    while((pid = waitpid(-1, &status, WNOHANG)) > 0) 
    {
        if(WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
            warn("pid [%u], exit code [%d]", pid, WEXITSTATUS(status));

        if(WIFSIGNALED(status))
            warn("pid [%u], signal [%d]", pid, WTERMSIG(status));

        /* decrement child count */
        server_reap_child(s, pid);
    }
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
    s->nbackend--;

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

static int server_fork_child(server_t *s, backend_t *be)
{
    backend_t *obe; /* other backed */
    pid_t child;
    int socks[2];

    u_unused_args(s);

    if(s->nchild == SERVER_MAX_CHILD_COUNT)
        return ~0; /* too much children */

    /* create a parent<->child IPC channel */
    dbg_err_if(socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0);

    if((child = fork()) == 0)
    {   /* child */

        /* never flush, the parent process will */
        s->klog_flush = 0;

        /* reseed the PRNG */
        srand(rand() + getpid() + time(0));

        /* close on end of the channel */
        close(socks[0]);

        /* save parent PPC socket and close the other */
        ctx->pipc = socks[1];
        ctx->backend = be;

        /* close listening sockets of other backends */
        LIST_FOREACH(obe, &s->bes, np)
        {
            if(obe == be)
                continue;
            close(obe->ld);
            obe->ld = -1;
        }

    } else if(child > 0) {
        /* parent */

        /* save child pid and increment child count */
        server_add_child(s, child);

        /* close one end of the channel */
        close(socks[1]);

        /* watch the PPC socket connected to the child */
        server_watch_fd(s, socks[0], WATCH_FD_READ);
    } else {
        warn_err("fork error");
    }

    return child;
err:
    warn_strerror(errno);
    return ~0;
}

static int server_child_serve(server_t *s, backend_t *be, int ad)
{
    pid_t child;
    int socks[2];

    u_unused_args(s);

    /* create a parent<->child IPC channel */
    dbg_err_if(socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0);

    dbg_err_if((child = server_fork_child(s, be)) < 0);

    if(child == 0)
    {   /* child */

        /* close this be listening descriptor */
        close(be->ld);

        /* serve the page */
        dbg_if(backend_serve(be, ad));

        /* close client socket and die */
        close(ad);
        server_stop(be->server); 

    } else {
        /* parent */

        /* close the accepted socket */
        close(ad);
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
    case SERVER_MODEL_PREFORK:
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
    if(ctx->pipc)
    {   /* child process */

        dbg_err_if(ctx->backend == NULL);

        /* close child listening sockets to force accept(2) to exit */
        close(ctx->backend->ld);
    }

    /* stop the parent process */
    s->stop = 1;

    return 0;
err:
    return ~0;
}

static int server_listen(server_t *s)
{
    backend_t *be;

    LIST_FOREACH(be, &s->bes, np)
    {
        /* bind to be->addr */
        dbg_err_if(server_be_listen(be));

        /* watch the listening socket */
        if(be->model != SERVER_MODEL_PREFORK)
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

static int server_set_socket_opts(server_t *s, int sock)
{
    int on = 1; 

    u_unused_args(s);

    /* disable Nagle algorithm */
    dbg_err_if(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, 
        (void*) &on, sizeof(int)) < 0);

    return 0;
err:
    return ~0;
}

static int server_dispatch(server_t *s, int fd)
{
    backend_t *be;
    int ad = -1; 

    /* find the backend that listen on fd */
    LIST_FOREACH(be, &s->bes, np)
        if(be->ld == fd)
            break;

    if(be == NULL) /* a child is ppc-calling */
        return server_process_ppc(s, fd);

    /* accept the pending connection */
    dbg_err_if(server_be_accept(s, be, &ad));

    /* set socket options on accepted socket */
    dbg_err_if(server_set_socket_opts(s, ad));

    /* serve the page */
    dbg_err_if(server_be_serve(s, be, ad));

    return 0;
err:
    if(ad != -1)
        close(ad);
    return ~0;
}

int server_cb_klog_flush(alarm_t *a, void *arg)
{
    server_t *s = (server_t*)arg;

    u_unused_args(a);

    /* alarmt expired, it has been free'd by timerm_t */
    s->al_klog_flush = NULL;

    /* set a flag to flush the klog object in server_loop */
    s->klog_flush++;

    return 0;
}

/* spawn pre-fork child processes */
static int server_spawn_children(server_t *s)
{
    backend_t *be;
    size_t i, c;
    int rc;

    /* spawn N child process that will sleep asap into accept(2) */
    LIST_FOREACH(be, &s->bes, np)
    {
        if(be->model != SERVER_MODEL_PREFORK)
            continue;

        /* spawn be->start_child child processes */
        for(i = 0; i < be->start_child; ++i)
        {
            dbg_err_if((rc = server_fork_child(s, be)) < 0);

            if(rc == 0)
            {   /* child */

                for(c = 0; c < be->max_rq_xchild; ++c)
                {
                    /* wait for a new client (will block on accept(2)) */
                    dbg_err_if(server_dispatch(s, be->ld));
                }
                break;
            } else {
                /* parent */
                ;
            }
        }
    }

    return 0;
err:
    return ~0;
}

int server_loop(server_t *s)
{
    struct timeval tv;
    int rc, fd;
    fd_set rdfds, wrfds, exfds;

    dbg_err_if(s == NULL || s->config == NULL);
    
    dbg_err_if(server_listen(s));

    dbg_err_if(server_spawn_children(s));

    dbg("server child count: %lu", s->nchild);

    /* children exit here */
    if(ctx->pipc)
        return 0;

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

        /* call klog_flush if flush timeout has expired and select() timeouts */
        if(s->klog_flush && ctx->pipc == NULL)
        {
            /* flush the log buffer */
            klog_flush(s->klog);

            /* reset the flag */
            s->klog_flush = 0; 

            /* re-set the timer */
            dbg_err_if(timerm_add(SERVER_LOG_FLUSH_TIMEOUT, 
                server_cb_klog_flush, s, &s->al_klog_flush));
        }
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

    /* shutdown all children */
    server_signal_childs(s, SIGTERM);

    sleep(1);

    /* brute kill children process */
    if(s->nchild)
    {
        dbg("sending SIGKILL");
        server_signal_childs(s, SIGKILL);
    }

    return 0;
err:
    return ~0;
}


int server_free(server_t *s)
{
    backend_t *be;

    dbg_err_if(s == NULL);

    /* remove the hook (that needs the server_t object) */
    u_log_set_hook(NULL, NULL, NULL, NULL);

    /* remove klog flushing alarm */
    if(s->al_klog_flush)
    {
        timerm_del(s->al_klog_flush);
        s->al_klog_flush = NULL;
    }

    if(s->klog)
    {
        /* child processes cann't close klog when in 'file' mode, because 
           klog_file_t will flush data that the parent already flushed 
           (children inherit a "used" FILE* that will usually contain, on close,
           not-empty buffer that fclose flushes). same thing may happen with
           different log devices when buffers are used.  */
        if(ctx->pipc == NULL)
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
    s->nbackend++;

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

    /* syslog klog doesn't go through ppc */
    if(s->klog->type == KLOG_TYPE_SYSLOG || ctx->pipc == NULL)
    {   /* syslog klog or parent context */
        if(s->klog)
            dbg_err_if(klog(s->klog, syslog_to_klog(level), "%s", str));
    } else {
        /* children context */
        dbg_err_if(server_ppc_cmd_log_add(s, level, str));
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

    if(ctx->backend)
        kl = ctx->backend->klog; /* may be NULL */

    if(kl == NULL)
        kl = s->klog; /* may be NULL */

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
    FD_ZERO(&s->rdfds);
    FD_ZERO(&s->wrfds);
    FD_ZERO(&s->exfds);

    /* init backend list */
    LIST_INIT(&s->bes);

    dbg_err_if(ppc_create(&s->ppc));

    /* create the log device if requested */
    if(!u_config_get_subkey(config, "log", &log_c))
    {
        dbg_if(klog_open_from_config(log_c, &s->klog));
        s->klog_flush = 1;
    }

    /* register the log ppc callbacks */
    dbg_err_if(ppc_register(s->ppc, PPC_CMD_NOP, server_ppc_cb_nop, s));
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
        dbg_err_if(s->nbackend == SERVER_MAX_BACKENDS);

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

    /* init done, set signal handlers */
    dbg_err_if(u_signal(SIGINT, server_sigint));
    dbg_err_if(u_signal(SIGTERM, server_sigterm));
    #ifdef OS_UNIX 
    dbg_err_if(u_signal(SIGPIPE, SIG_IGN));
    dbg_err_if(u_signal(SIGCHLD, server_sigchld));
    #endif

    return 0;
err:
    if(n)
        u_free(n);
    if(s)
    {
        server_free(s);
        *ps = NULL;
    }
    return ~0;
}

