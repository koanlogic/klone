/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: server.c,v 1.70 2009/11/26 09:11:33 stewy Exp $
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_WAIT
#include <sys/wait.h>
#endif
#include <u/libu.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <klone/server.h>
#include <klone/backend.h>
#include <klone/os.h>
#include <klone/timer.h>
#include <klone/context.h>
#include <klone/ppc.h>
#include <klone/ppc_cmd.h>
#include <klone/utils.h>
#include <klone/klog.h>
#include <klone/hook.h>
#include <klone/hookprv.h>
#include <klone/server_ppc_cmd.h>
#include <klone/emb.h>
#include "server_s.h"
#include "child.h"

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
    dbg_return_if (be == NULL, ~0);
    dbg_return_if (be->na == NULL, ~0);

    /* Return a socket descriptor bound to the configured endpoint. */
    warn_return_if ((be->ld = u_net_sd_by_addr(be->na)) == -1, ~0);

    return 0;
}


#ifdef OS_UNIX
/* remove a child process whose pid is 'pid' to children list */
static int server_reap_child(server_t *s, pid_t pid)
{
    child_t *child;
    backend_t *be;

    dbg_err_if (s == NULL);
    
    /* get the child object */
    dbg_err_if(children_get_by_pid(s->children, pid, &child));

    /* remove the child from the list */
    dbg_err_if(children_del(s->children, child));
    be = child->be;

    /* check that the minimum number of process are active */
    be->nchild--;
    if(be->nchild < be->start_child)
        be->fork_child = be->start_child - be->nchild;

    U_FREE(child);

    return 0;
err:
    return ~0;
}

/* add a child to the list */
static int server_add_child(server_t *s, pid_t pid, backend_t *be)
{
    child_t *child = NULL;

    dbg_err_if (s == NULL);
    dbg_err_if (be == NULL);

    dbg_err_if(child_create(pid, be, &child));

    dbg_err_if(children_add(s->children, child));

    be->nchild++;

    return 0;
err:
    return ~0;
}

/* send 'sig' signal to all children process */
static int server_signal_children(server_t *s, int sig)
{
    child_t *child;
    ssize_t i;

    dbg_return_if (s == NULL, ~0);
    
    for(i = children_count(s->children) - 1; i >= 0; --i)
    {
        if(!children_getn(s->children, i, &child))
            dbg_err_if(kill(child->pid, sig) < 0);
    }

    return 0;
err:
    dbg_strerror(errno);
    return ~0;
}
#endif

static void server_term_children(server_t *s)
{
    dbg_ifb(s == NULL) return;
#ifdef OS_UNIX
    server_signal_children(s, SIGTERM);
#endif
    return;
}

static void server_kill_children(server_t *s)
{
    dbg_ifb(s == NULL) return;
#ifdef OS_UNIX
    server_signal_children(s, SIGKILL);
#endif
    return;
}

static void server_sigint(int sig)
{
    u_unused_args(sig);

    u_warn("SIGINT");

    if(ctx && ctx->server)
        server_stop(ctx->server);
    
    emb_term();
}

static void server_sigterm(int sig)
{
    u_unused_args(sig);

    /* child process die immediately.
     * note: don't call debug functions because the parent process could be
     * already dead if the user used the "killall kloned" command */
    if(ctx->pipc)
        _exit(0); 

    u_warn("SIGTERM");

    if(ctx && ctx->server)
        server_stop(ctx->server);
}

#ifdef OS_UNIX
static void server_sigchld(int sig)
{
    server_t *s = ctx->server;

    u_unused_args(sig);

    s->reap_children = 1;
}

static void server_waitpid(server_t *s)
{
    pid_t pid = -1;
    int status;

    dbg_ifb(s == NULL) return;
    
    u_sig_block(SIGCHLD);

    /* detach from child processes */
    while((pid = waitpid(-1, &status, WNOHANG)) > 0) 
    {
        if(WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
            u_warn("pid [%u], exit code [%d]", pid, WEXITSTATUS(status));

        if(WIFSIGNALED(status))
            u_warn("pid [%u], signal [%d]", pid, WTERMSIG(status));

        /* decrement child count */
        server_reap_child(s, pid);
    }

    s->reap_children = 0;

    u_sig_unblock(SIGCHLD);
}
#endif

static void server_recalc_hfd(server_t *s)
{
    register int i;
    fd_set *prdfds, *pwrfds, *pexfds;

    dbg_ifb(s == NULL) return;
    
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
    dbg_ifb(s == NULL) return;

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
    dbg_ifb(s == NULL) return;
    dbg_ifb(fd < 0) return;

    if(mode & WATCH_FD_READ)
        FD_SET(fd, &s->rdfds);

    if(mode & WATCH_FD_WRITE)
        FD_SET(fd, &s->wrfds);

    if(mode & WATCH_FD_EXCP)
        FD_SET(fd, &s->exfds);

    s->hfd = U_MAX(s->hfd, fd);
}

static void server_close_fd(server_t *s, int fd)
{
    dbg_ifb(s == NULL) return;
    dbg_ifb(fd < 0) return;

    server_clear_fd(s, fd, WATCH_FD_READ | WATCH_FD_WRITE | WATCH_FD_EXCP);
    close(fd);
}

static int server_be_accept(server_t *s, backend_t *be, int *pfd)
{
    struct sockaddr sa;
    int sa_len = sizeof(struct sockaddr);
    int ad;

    u_unused_args(s);

    dbg_return_if (be == NULL, ~0);
    dbg_return_if (pfd == NULL, ~0);

again:
    ad = accept(be->ld, &sa, &sa_len);
    if(ad == -1 && errno == EINTR)
        goto again; /* interrupted */
    dbg_err_if(ad == -1); /* accept error */

    *pfd = ad;

    return 0;
err:
    if(ad < 0)
        warn_strerror(errno);
    return ~0;
}

static int server_backend_detach(server_t *s, backend_t *be)
{
    s->nbackend--;

    dbg_return_if (s == NULL, ~0);
    dbg_return_if (be == NULL, ~0);

    u_net_addr_free(be->na);
    be->server = NULL;
    be->na = NULL;
    be->config = NULL;

    close(be->ld);
    be->ld = -1;

    backend_free(be);

    return 0;
}

#ifdef OS_UNIX
static int server_chroot_to(server_t *s, const char *dir)
{
    dbg_return_if (s == NULL, ~0);
    dbg_return_if (dir == NULL, ~0);

    u_unused_args(s);

    dbg_err_if(chroot(dir));

    dbg_err_if(chdir("/"));

    u_info("chroot'd: %s", dir);

    return 0;
err:
    warn_strerror(errno);
    return ~0;
}

static int server_foreach_cb(struct dirent *d, const char *path, void *arg)
{
    int *pfound = (int*)arg;

    u_unused_args(d, path);

    *pfound = 1;

    return ~0;
}

static int server_chroot_blind(server_t *s)
{
#ifdef HAVE_FORK
    enum { BLIND_DIR_MODE = 0100 }; /* blind dir mode must be 0100 */
    char dir[U_PATH_MAX];
    struct stat st;
    int fd_dir = -1, found;
    pid_t child;
    unsigned int mask;

    dbg_err_if (s == NULL);
    dbg_err_if (s->chroot == NULL);

    dbg_err_if(u_path_snprintf(dir, U_PATH_MAX, U_PATH_SEPARATOR,
        "%s/kloned_blind_chroot_%d.dir", s->chroot, getpid()));

    /* create the blind dir (0100 mode) */
    dbg_err_if(mkdir(dir, BLIND_DIR_MODE ));

    /* get the fd of the dir */
    dbg_err_if((fd_dir = open(dir, O_RDONLY, 0)) < 0);

    dbg_err_if((child = fork()) < 0);

    if(child == 0)
    {   /* child */

        /* delete the chroot dir and exit */
        sleep(1); // FIXME use a lock here
        u_dbg("[child] removing dir: %s\n", dir);
        rmdir(dir);
        _exit(0);
    }
    /* parent */

    /* do chroot */
    dbg_err_if(server_chroot_to(s, dir));

    /* do some dir sanity checks */

    /* get stat values */
    dbg_err_if(fstat(fd_dir, &st));

    /* the dir owned must be root */
    dbg_err_if(st.st_gid || st.st_uid);

    /* the dir mode must be 0100 */
    dbg_err_if((st.st_mode & 07777) != BLIND_DIR_MODE);

    /* the dir must be empty */
    found = 0;
    mask = S_IFIFO | S_IFCHR | S_IFDIR | S_IFBLK | S_IFREG | S_IFLNK | S_IFSOCK;
    dbg_err_if(u_foreach_dir_item("/", mask, server_foreach_cb, &found));

    /* bail out if the dir is not empty */
    dbg_err_if(found);

    close(fd_dir);

    return 0;
err:
    if(fd_dir >= 0)
        close(fd_dir);
    warn_strerror(errno);
    return ~0;
#else   /* !HAVE_FORK */
    u_unused_args(s);
    err("Blind chroot could not be honoured since fork(2) unavailable");
    return ~0;
#endif  /* HAVE_FORK */
}

static int server_chroot(server_t *s)
{
    dbg_return_if (s == NULL, ~0);

    if(s->blind_chroot)
        return server_chroot_blind(s);
    else
        return server_chroot_to(s, s->chroot);
}

static int server_drop_privileges(server_t *s)
{
    uid_t uid;
    gid_t gid;

    dbg_return_if (s == NULL, ~0);

    if(s->gid > 0)
    {
        gid = (gid_t)s->gid;

        /* remove all groups except gid */
        dbg_err_if(setgroups(1, &gid));

        /* set gid */
        dbg_err_if(setgid(gid));
        dbg_err_if(setegid(gid));

        /* verify */
        dbg_err_if(getgid() != gid || getegid() != gid);
    }

    if(s->uid > 0)
    {
        uid = (uid_t)s->uid;

        /* set uid */
        dbg_err_if(setuid(uid));
        dbg_err_if(seteuid(uid));

        /* verify */
        dbg_err_if(getuid() != uid || geteuid() != uid);
    }
    
    return 0;
err:
    warn_strerror(errno);
    return ~0;
}

static int server_fork_child(server_t *s, backend_t *be)
{
#ifdef HAVE_FORK
    backend_t *obe; /* other backed */
    pid_t child;
    int socks[2];

    dbg_return_if (s == NULL, -1);
    dbg_return_if (be == NULL, -1);
    /* exit on too much children */
    dbg_return_if (children_count(s->children) == s->max_child, -1);
    dbg_return_if (be->nchild == be->max_child, -1);

    /* create a parent<->child IPC channel */
    dbg_err_if(socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0);

    if((child = fork()) == 0)
    {   /* child */

        /* never flush, the parent process will */
        s->klog_flush = 0;

        /* reseed the PRNG */
        srand(rand() + getpid() + time(0));

        /* close one end of the channel */
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

        /* clear child copy of children list */
        dbg_err_if(children_clear(s->children));

    } else if(child > 0) {
        /* parent */

        /* save child pid and increment child count */
        server_add_child(s, child, be);

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
    return -1;
#else   /* !HAVE_FORK */
    u_unused_args(s, be);
    u_warn("Only iterative mode is enabled (fork(2) unsupported by target OS)");
    return -1;
#endif  /* HAVE_FORK */
}

static int server_child_serve(server_t *s, backend_t *be, int ad)
{
    pid_t child;

    dbg_return_if (s == NULL, ~0);
    dbg_return_if (be == NULL, ~0);

    dbg_err_if((child = server_fork_child(s, be)) < 0);

    if(child == 0)
    {   /* child */

        /* close this be listening descriptor */
        close(be->ld);

        hook_call(child_init);

        /* serve the page */
        dbg_if(backend_serve(be, ad));

        hook_call(child_term);

        /* close client socket and die */
        close(ad);
        server_stop(be->server); 
    }
    /* parent */

    return 0;
err:
    warn_strerror(errno);
    return ~0;
}

static int server_cb_spawn_child(talarm_t *al, void *arg)
{
    server_t *s = (server_t*)arg;

    u_unused_args(al);

    dbg_err_if (s == NULL);

    /* must be called by a child process */
    dbg_err_if(ctx->backend == NULL || ctx->pipc == 0);

    /* ask the parent to create a new worker child process */
    dbg_err_if(server_ppc_cmd_fork_child(s, ctx->backend));

    /* mark the current child process so it will die when finishes 
       serving this page */
    server_stop(s);

    return 0;
err:
    return ~0;
}
#endif /* ifdef OS_UNIX */

static int server_be_serve(server_t *s, backend_t *be, int ad)
{
    talarm_t *al = NULL;

    dbg_err_if (s == NULL);
    dbg_err_if (be == NULL);
    
    switch(be->model)
    {
#if defined(OS_UNIX) && defined(HAVE_FORK)
    case SERVER_MODEL_FORK:
        /* spawn a child to handle the request */
        dbg_err_if(server_child_serve(s, be, ad));
        break;

    case SERVER_MODEL_PREFORK: 
        /* FIXME lower timeout value may be needed */
        /* if _serve takes more then 1 second spawn a new worker process */
        dbg_err_if(timerm_add(1, server_cb_spawn_child, (void*)s, &al));

        /* serve the page */
        dbg_if(backend_serve(be, ad));

        /* remove and free the alarm */
        timerm_del(al); /* prefork */
        break;
#endif  /* OS_UNIX && HAVE_FORK */

    case SERVER_MODEL_ITERATIVE:
        /* serve the page */
        dbg_if(backend_serve(be, ad));
        break;

    default:
        warn_err_if("server model not supported");
    }

    /* close the accepted (already served) socket */
    close(ad);

    return 0;
err:
    close(ad);
    return ~0;
}

int server_stop(server_t *s)
{
    dbg_err_if (s == NULL);
    
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

    dbg_err_if (s == NULL);
    
    LIST_FOREACH(be, &s->bes, np)
    {
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

    dbg_err_if (s == NULL);

    /* use the first http backend as the CGI backend */
    LIST_FOREACH(be, &s->bes, np)
    {
        if(strcasecmp(be->proto, "http") == 0)
        {
            hook_call(server_init);

            dbg_if(backend_serve(be, 0));

            hook_call(server_term);

            return 0;
        }
    }

err: /* fall through if search loop exhausted */
    return ~0;
}

ppc_t* server_get_ppc(server_t *s)
{
    dbg_return_if (s == NULL, NULL);

    return s->ppc;
}

static int server_process_ppc(server_t *s, int fd)
{
    unsigned char cmd;
    char data[PPC_MAX_DATA_SIZE];
    ssize_t n;

    dbg_err_if (s == NULL);
    dbg_err_if (fd < 0);

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
    int ad = -1; 

    dbg_err_if (s == NULL);

    /* find the backend that listen on fd */
    LIST_FOREACH(be, &s->bes, np)
        if(be->ld == fd)
            break;

    if(be == NULL) /* a child is ppc-calling */
        return server_process_ppc(s, fd);

    /* accept the pending connection */
    dbg_err_if(server_be_accept(s, be, &ad));

    /* Disable Nagle on the socket. Note that this may fail if the socket is 
     * in the UNIX family or it is using the SCTP transport instead of TCP. */
    (void) u_net_nagle_off(ad);

    /* serve the page */
    dbg_err_if(server_be_serve(s, be, ad));

    return 0;
err:
    U_CLOSE(ad);
    return ~0;
}

int server_cb_klog_flush(talarm_t *a, void *arg)
{
    server_t *s = (server_t*)arg;

    u_unused_args(a);

    dbg_return_if (s == NULL, ~0);

    /* set a flag to flush the klog object in server_loop */
    s->klog_flush++;

    return 0;
}

#ifdef OS_UNIX
int server_spawn_child(server_t *s, backend_t *be)
{
    size_t c;
    int rc;

    dbg_err_if (s == NULL);
    dbg_err_if (be == NULL);

    dbg_err_if((rc = server_fork_child(s, be)) < 0);
    if(rc > 0)
        return 0; /* parent */

    /* call the hook that runs the on-child user code */
    hook_call(child_init);

    /* child main loop: 
       close on s->stop or if max # of request limit has reached (the 
       server will respawn a new process if needed) */
    for(c = 0; !s->stop && c < be->max_rq_xchild; ++c)
    {
        /* wait for a new client (will block on accept(2)) */
        dbg_err_if(server_dispatch(s, be->ld));
    }

    /* before child shutdowns call the term hook */
    hook_call(child_term);

    server_stop(s);

    return 0;
err:
    return ~0;
}

/* spawn pre-fork child processes */
static int server_spawn_children(server_t *s)
{
    backend_t *be;
    register size_t i;

    dbg_err_if (s == NULL);

    /* spawn N child process that will sleep asap into accept(2) */
    LIST_FOREACH (be, &s->bes, np)
    {
        if(be->model != SERVER_MODEL_PREFORK || be->fork_child == 0)
            continue;

        /* spawn be->fork_child child processes */
        for(i = 0; i < be->fork_child; ++i)
        {
            dbg_err_if(server_spawn_child(s, be));
            /* child context? */
            if(ctx->pipc)
                break; /* the child previously spawned is dying, exit */
            be->fork_child--;
        }
    }

    return 0;
err:
    return ~0;
}
#endif

int server_loop(server_t *s)
{
    struct timeval tv;
    int rc, fd;
    fd_set rdfds, wrfds, exfds;

    dbg_err_if (s == NULL);
    dbg_err_if (s->config == NULL);

    dbg_err_if(server_listen(s));

#ifdef OS_UNIX
    /* if it's configured chroot to the dst dir */
    if(s->chroot)
        dbg_err_if(server_chroot(s));

    /* set uid/gid to non-root user */
    warn_err_sifm(server_drop_privileges(s), "unable to drop priviledges");

    /* if allow_root is not set check that we're not running as root */
    if(!s->allow_root)
        warn_err_ifm(!getuid() || !geteuid() || !getgid() || !getegid(),
            "you must set the allow_root config option to run kloned as root");

#elif OS_WIN
    if(s->chroot)
        dbg_err_if(SetCurrentDirectory(s->chroot) == 0);

#endif

    /* server startup hook */
    hook_call(server_init);

    for(; !s->stop; )
    {
#ifdef OS_UNIX
        /* spawn new child if needed (may fail on resource limits) */
        dbg_if(server_spawn_children(s));
#endif

        /* children in pre-fork mode exit here */
        if(ctx->pipc)
            break;

        memcpy(&rdfds, &s->rdfds, sizeof(fd_set));
        memcpy(&wrfds, &s->wrfds, sizeof(fd_set));
        memcpy(&exfds, &s->exfds, sizeof(fd_set));

    again:
        /* wake up every second */
        tv.tv_sec = 1; tv.tv_usec = 0;

        rc = select(1 + s->hfd, &rdfds, &wrfds, &exfds, &tv); 
        if(rc == -1 && errno == EINTR)
            goto again; /* interrupted */
        dbg_err_if(rc == -1); /* select error */

#ifdef OS_UNIX
        if(s->reap_children)
            server_waitpid(s);
#endif

        /* parent only */
        if(ctx->pipc == 0)
        {
            /* call klog_flush if flush timeout has expired and select() timeouts */
            if(s->klog_flush)
            {
                /* flush the log buffer */
                klog_flush(s->klog);

                /* reset the flag */
                s->klog_flush = 0;

                U_FREE(s->al_klog_flush);

                /* re-set the timer */
                dbg_err_if(timerm_add(SERVER_LOG_FLUSH_TIMEOUT,
                    server_cb_klog_flush, s, &s->al_klog_flush));
            }

            /* server loop hook - trigger only upon timeout and not upon client request */
            if(rc == 0)
                hook_call(server_loop);
        }

        /* for each signaled listening descriptor */
        for(fd = 0; rc && fd < 1 + s->hfd; ++fd)
        { 
            if(FD_ISSET(fd, &rdfds))
            {
                --rc;
                /* dispatch the request to the right backend */
                dbg_if(server_dispatch(s, fd));
            } 
        } /* for each ready fd */

    } /* !s->stop */

    /* children in fork mode exit here */
    if(ctx->pipc)
        return 0;

    /* server shutdown hook */
    hook_call(server_term);

    /* shutdown all children */
    server_term_children(s);

    sleep(1);

    /* brute kill children process */
    if(s->nchild)
        server_kill_children(s);

    return 0;
err:
    return ~0;
}

int server_free(server_t *s)
{
    backend_t *be;

    dbg_err_if (s == NULL);

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
        /* child processes must not close klog when in 'file' mode, because 
           klog_file_t will flush data that the parent already flushed 
           (children inherit a "used" FILE* that will usually contain, on close,
           not-empty buffer that fclose (called by exit()) flushes). same 
           thing may happens with different log devices when buffers are used.
         */
        if(ctx->pipc == 0)
            klog_close(s->klog);
        s->klog = NULL;
    }

    while((be = LIST_FIRST(&s->bes)) != NULL)
    {
        LIST_REMOVE(be, np);
        server_backend_detach(s, be);
    }

    dbg_if(ppc_free(s->ppc));

    dbg_if(children_free(s->children));

#ifdef OS_WIN
    WSACleanup();
#endif

    U_FREE(s);
    return 0;
err:
    return ~0;
}

static int server_setup_backend(server_t *s, backend_t *be)
{
    const char *a;

    dbg_return_if (s == NULL, ~0);
    dbg_return_if (be == NULL, ~0);
    
    /* server count */
    s->nbackend++;

    /* Get 'addr' value from config.  Expect it to be given in libu::net
     * URI format, e.g. something in between 'tcp4://192.168.0.1:80' and 
     * 'tcp6://[*]:8080'. */
    warn_err_ifm ((a = u_config_get_subkey_value(be->config, "addr")) == NULL,
        "missing or bad '<servname>.addr' value");

    /* Parse and internalize it. */
    warn_err_ifm (u_net_uri2addr(a, U_NET_SSOCK, &be->na), 
            "bad syntax for 'addr' value");

    return 0;
err:
    u_warn("'addr' syntax has changed with klone 3: check libu URI format");
    return ~0;
}

static int server_log_hook(void *arg, int level, const char *str)
{
    server_t *s = (server_t*)arg;
    u_log_hook_t old = NULL;
    void *old_arg = NULL;

    dbg_err_if (s == NULL);
    dbg_err_if (str == NULL);
 
    /* if both the server and the calling backend have no log then exit */
    if(s->klog == NULL && (ctx->backend == NULL || ctx->backend->klog == NULL))
        return 0; /* log is disabled */

    /* disable log hooking in the hook itself otherwise an infinite loop 
       may happen if a log function is called from inside the hook */
    u_log_set_hook(NULL, NULL, &old, &old_arg);

    /* syslog klog doesn't go through ppc */
    if(s->klog->type == KLOG_TYPE_SYSLOG || ctx->pipc == 0)
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

    dbg_err_if (s == NULL);
    dbg_err_if (pkl == NULL);
 
    if(ctx->backend)
        kl = ctx->backend->klog; /* may be NULL */

    if(kl == NULL)
        kl = s->klog; /* may be NULL */

    *pkl = kl;

    return 0;
err:
    return ~0;
}

static int server_get_klog_line(server_t *s, klog_t *kl, size_t i, char *line)
{
    backend_t *be = ctx->backend;

    dbg_err_if(kl->type != KLOG_TYPE_MEM);
    dbg_err_if(be == NULL);

    /* we need ppc just in prefork mode */
    if(be->model != SERVER_MODEL_PREFORK)
    {
        dbg_err_if(klog_getln(kl, i, line));
        return 0;
    }

    /* send the ppc command and read back the response */
    nop_err_if(server_ppc_cmd_log_get(s, i, line));

    return 0;
err:
    return ~0;
}

int server_foreach_memlog_line(server_t *s, 
    int (*cb)(const char*, void*), void *arg)
{
    klog_t *kl = NULL;  
    size_t i;
    char line[KLOG_LN_SZ];

    /* get the configured klog object and check that's a in-memory klog */
    if(server_get_logger(s, &kl) || kl == NULL || kl->type != KLOG_TYPE_MEM)
    {
        cb("logging is not configured or is not a in-memory log", arg);
        return ~0;
    }

    /* for each log line call the user-given callback function */
    for(i = 1; server_get_klog_line(s, kl, i, line) == 0; ++i)
        cb(line, arg);

    return 0;
}


int server_get_backend_by_id(server_t *s, int id, backend_t **pbe)
{
    backend_t *be;

    dbg_err_if (s == NULL);
    dbg_err_if (pbe == NULL);
    
    LIST_FOREACH(be, &s->bes, np)
    {
        if(be->id == id)
        {
            *pbe = be;
            return 0;
        }
    }

err: /* fall through if search loop exhausted */
    return ~0;
}

int server_create(u_config_t *config, int foreground, server_t **ps)
{
    server_t *s = NULL;
    u_config_t *bekey = NULL, *log_c = NULL;
    backend_t *be = NULL;
    const char *list, *type;
    char *n = NULL, *name = NULL;
    int i, id, iv;

    dbg_return_if (ps == NULL, ~0);
    dbg_return_if (config == NULL, ~0);

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
    s->model = SERVER_MODEL_FORK; /* default */

    dbg_err_if(children_create(&s->children));

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
    dbg_err_if(ppc_register(s->ppc, PPC_CMD_LOG_GET, server_ppc_cb_log_get, s));
#ifdef OS_UNIX
    dbg_err_if(ppc_register(s->ppc, PPC_CMD_FORK_CHILD, 
        server_ppc_cb_fork_child, s));
#endif
    dbg_err_if(ppc_register(s->ppc, PPC_CMD_ACCESS_LOG, 
                server_ppc_cb_access_log, s));

    /* redirect logs to the server_log_hook function */
    dbg_err_if(u_log_set_hook(server_log_hook, s, NULL, NULL));

    /* parse server list and build s->bes */
    list = u_config_get_subkey_value(config, "server_list");
    warn_err_ifm(list == NULL, "bad or missing 'server_list' config param");

    /* chroot, uid and gid */
    s->chroot = u_config_get_subkey_value(config, "chroot");
    dbg_err_if(u_config_get_subkey_value_i(config, "uid", -1, &s->uid));
    dbg_err_if(u_config_get_subkey_value_i(config, "gid", -1, &s->gid));
    dbg_err_if(u_config_get_subkey_value_b(config, "allow_root", 0, 
        &s->allow_root));
    dbg_err_if(u_config_get_subkey_value_b(config, "blind_chroot", 0, 
        &s->blind_chroot));

    warn_err_ifm(!s->uid || !s->gid, 
        "you must set uid and gid config parameters");

    dbg_err_if(u_config_get_subkey_value_i(config, "max_child", 
        SERVER_MAX_CHILD, &iv));
    s->max_child = iv;

    name = n = u_zalloc(strlen(list) + 1);
    dbg_err_if(name == NULL);
    
    /* load config and init backend for each server in server.list */
    for(i = strlen(list), id = 0; 
        i > 0 && sscanf(list, "%[^ \t]", name); 
        i -= 1 + strlen(name), list += 1 + strlen(name), name[0] = 0)
    {
        u_dbg("configuring backend: %s", name);

        /* just SERVER_MAX_BACKENDS supported */
        warn_err_if(s->nbackend == SERVER_MAX_BACKENDS);

        /* get config tree of this backend */
        warn_err_ifm(u_config_get_subkey(config, name, &bekey),
            "missing [%s] backend configuration", name);

        type = u_config_get_subkey_value(bekey, "type");
        warn_err_ifm(type == NULL, "missing or bad '<servname>.type' value");

        /* create a new backend and push it into the 'be' list */
        warn_err_ifm(backend_create(type, bekey, &be),
            "backend \"%s\" startup error", type);

        be->server = s;
        be->config = bekey;
        be->id = id++;
        if(be->model == SERVER_MODEL_UNSET)
            be->model = s->model; /* inherit server model */

        if(foreground)
            be->model = SERVER_MODEL_ITERATIVE;

        /* create the log device (may fail if logging is not configured) */
        if(!u_config_get_subkey(bekey, "log", &log_c))
            dbg_if(klog_open_from_config(log_c, &be->klog));

#ifdef OS_WIN
        if(be->model != SERVER_MODEL_ITERATIVE)
            warn_err("child-based server model is not "
                     "yet supported on Windows");
#endif

        LIST_INSERT_HEAD(&s->bes, be, np);

        dbg_err_if(server_setup_backend(s, be));
    }

    U_FREE(n);

    /* init done, set signal handlers */
    dbg_err_if(u_signal(SIGINT, server_sigint));
    dbg_err_if(u_signal(SIGTERM, server_sigterm));
#ifdef OS_UNIX 
    dbg_err_if(u_signal(SIGPIPE, SIG_IGN));
    dbg_err_if(u_signal(SIGCHLD, server_sigchld));
#endif

    return 0;
err:
    u_warn("server init error (config error?)");
    U_FREE(n);
    if(s)
    {
        server_free(s);
        *ps = NULL;
    }
    return ~0;
}

