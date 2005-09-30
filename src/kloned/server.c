#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <klone/debug.h>
#include <klone/queue.h>
#include <klone/addr.h>
#include <klone/server.h>
#include <klone/backend.h>
#include <klone/os.h>
#include <klone/timer.h>

#define SERVER_MAX_BACKENDS 8

enum watch_fd_e
{
    WATCH_FD_READ   = 1 << 1,
    WATCH_FD_WRITE  = 1 << 2,
    WATCH_FD_EXCP   = 1 << 3
};

void server_watch_fd(server_t *s, int fd, unsigned int mode);
void server_clear_fd(server_t *s, int fd, unsigned int mode);
void server_close_fd(server_t *s, int fd);

struct server_s 
{
    config_t *config;       /* server config                    */
    fd_set rdfds, wrfds, exfds;
    int hfd;                /* highest set fd in fd_sets        */
    size_t nserver;         /* # of servers                     */
    backends_t bes;         /* backend list                     */
    int stop;               /* >0 will stop the loop            */
    int model;              /* server model                     */
};

static int server_be_listen(backend_t *be)
{
    enum { DEFAULT_BACKLOG = 10 };
    int d = 0, backlog = 0, val = 1;
    config_t *subkey;

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

    if(!config_get_subkey(be->config, "backlog", &subkey))
        backlog = atoi(config_get_value(subkey));

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

    /* set s->hfd to highest value */
    for(i = s->hfd, s->hfd = 0; i > 0; --i)
    {
        if(FD_ISSET(i, &s->rdfds) || FD_ISSET(i, &s->wrfds) || 
                FD_ISSET(i, &s->exfds))
        {
            s->hfd = i;
            break;
        }
    }
}

void server_clear_fd(server_t *s, int fd, unsigned int mode)
{
    if(mode & WATCH_FD_READ)
        FD_CLR(fd, &s->rdfds);

    if(mode & WATCH_FD_WRITE)
        FD_CLR(fd, &s->wrfds);

    if(mode & WATCH_FD_EXCP)
        FD_CLR(fd, &s->exfds);

    server_recalc_hfd(s);
}

void server_watch_fd(server_t *s, int fd, unsigned int mode)
{
    if(mode & WATCH_FD_READ)
        FD_SET(fd, &s->rdfds);

    if(mode & WATCH_FD_WRITE)
        FD_SET(fd, &s->wrfds);

    if(mode & WATCH_FD_EXCP)
        FD_SET(fd, &s->exfds);

    s->hfd = MAX(s->hfd, fd);
}

void server_close_fd(server_t *s, int fd)
{
    server_clear_fd(s, fd, WATCH_FD_READ | WATCH_FD_WRITE | WATCH_FD_EXCP);
    close(fd);
}


static int server_be_accept(server_t *s, backend_t *be, int* pfd)
{
    struct sockaddr sa;
    int sa_len = sizeof(struct sockaddr);
    int ad;

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

    dbg("sending SIGTERM to child [%d]", child);

    dbg_err_if(kill(child, SIGTERM) == -1);

    return 0;
err:
    dbg_strerror(errno);
    return ~0;
}

static int server_be_serve(server_t *s, backend_t *be, int ad)
{
    pid_t child;
    alarm_t *al = NULL;

    switch(be->model)
    {
    case SERVER_MODEL_FORK:
        if((child = fork()) == 0)
        {   /* child */

            /* close this be listening descriptor */
            close(be->ld);

            /* serve the page */
            dbg_if(backend_serve(be, ad));

            /* close client socket and die */
            close(ad);
            server_stop(be->server); 

        } else if(child > 0) {
           /* parent */
            close(ad);

        } else {
            dbg_err_sif("fork error");
        }
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
            backend_serve(be, 0);
            break;
        }
    }

    return ~0;
}

int server_dispatch(server_t *s, int ld)
{
    backend_t *be;
    int ad; 

    /* find the backend that listen on fd */
    LIST_FOREACH(be, &s->bes, np)
    {
        if(be->ld == ld)
        {
            /* accept the pending connection */
            dbg_if(server_be_accept(s, be, &ad));

            dbg_if(server_be_serve(s, be, ad));

            break;
        }
    }

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

        /* a child is calling, use the internal service backend */
        // server_be_serve(s, service_be, fd);

    } /* infinite loop */

    return 0;
err:
    return ~0;
}


int server_free(server_t *s)
{
    backend_t *be;

    dbg_err_if(s == NULL);

    while((be = LIST_FIRST(&s->bes)) != NULL)
    {
        LIST_REMOVE(be, np);
        server_backend_detach(s, be);
    }

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
    config_t *subkey;

    /* server count */
    s->nserver++;

    /* parse and create the bind addr_t */
    dbg_err_if(config_get_subkey(be->config, "addr", &subkey));

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

int server_create(config_t *config, int model, server_t **ps)
{
    server_t *s = NULL;
    config_t *bekey = NULL;
    backend_t *be = NULL;
    const char *list, *type;
    char *n = NULL, *name = NULL;
    int i;

#ifdef OS_WIN
    WORD ver;
    WSADATA wsadata;

    ver = MAKEWORD(1,1);
    dbg_err_if(WSAStartup(ver, &wsadata));
#endif

    s = u_calloc(sizeof(server_t));
    dbg_err_if(s == NULL);

    s->config = config;
    s->model = model;

    /* init fd_set */
    memset(&s->rdfds, 0, sizeof(fd_set));
    memset(&s->wrfds, 0, sizeof(fd_set));
    memset(&s->exfds, 0, sizeof(fd_set));

    /* init backend list */
    LIST_INIT(&s->bes);
    
    /* parse server list and build s->bes */
    list = config_get_subkey_value(config, "server_list");
    dbg_err_if(list == NULL);

    name = n = u_calloc(strlen(list) + 1);
    dbg_err_if(name == NULL);
    
    /* load config and init backend for each server in server.list */
    for(i = strlen(list); 
        i > 0 && sscanf(list, "%[^ \t]", name); 
        i -= 1 + strlen(name), list += 1 + strlen(name), name[0] = 0)
    {
        dbg("configuring backend: %s", name);

        /* just SERVER_MAX_BACKENDS supported */
        dbg_err_if(s->nserver == SERVER_MAX_BACKENDS);

        /* get config tree of this backend */
        warn_err_ifm(config_get_subkey(config, name, &bekey),
            "missing [%s] backend configuration", name);

        type = config_get_subkey_value(bekey, "type");
        dbg_err_if(type == NULL);

        /* create a new backend and push into the be list */
        dbg_err_if(backend_create(type, bekey, &be));

        be->server = s;
        be->config = bekey;
        if(be->model == SERVER_MODEL_UNSET)
            be->model = s->model; /* inherit server model */

        LIST_INSERT_HEAD(&s->bes, be, np);

        dbg_err_if(server_setup_backend(s, be));
    }

    u_free(n);

    *ps = s;

    return 0;
err:
    if(n)
        u_free(n);
    if(s)
        server_free(s);
    return ~0;
}

