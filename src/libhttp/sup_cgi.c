/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: sup_cgi.c,v 1.12 2008/10/27 21:28:04 tat Exp $
 */

#include "klone_conf.h"
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <klone/http.h>
#include <klone/supplier.h>
#include <klone/io.h>
#include <klone/utils.h>
#include <klone/rsfilter.h>
#include <klone/vhost.h>

/* holds environment variables passed to the cgi */
typedef struct cgi_env_s
{
    char **env;
    int size, count;
} cgi_env_t;

static int cgi_get_config(http_t *h, request_t *rq, u_config_t **pc)
{
    vhost_t *vhost;

    dbg_err_if(h == NULL);
    dbg_err_if(rq == NULL);

    dbg_err_if((vhost = http_get_vhost(h, rq)) == NULL);

    *pc = vhost->config;

    return 0;
err:
    return ~0;
}


static int cgi_script(http_t *h, request_t *rq, const char *fqn)
{
    u_config_t *config, *sub, *base;
    const char *dir;
    int i, t;

    if(fqn == NULL)
        return 0;

    /* get cgi. config subtree */
    dbg_err_if(cgi_get_config(h, rq, &base));

    /* get cgi. config subtree */
    nop_err_if(u_config_get_subkey(base, "cgi", &config));

    /* for each script_di config item */
    for(i = 0; !u_config_get_subkey_nth(config, "script_alias", i, &sub); ++i)
    {
        if((dir = u_config_get_value(sub)) == NULL)
            continue; /* empty key */

        /* find the dir part of the "alias dir" value */
        for(t = strlen(dir) - 1; t > 0; --t)
            if(dir[t] == ' ' || dir[t] == '\t')
                break;

        if(t == 0)
            continue; /* malformed value */

        /* skip the blank */
        dir += ++t;

        /* the first part of fqn must be equal to p and the file must be +x */
        if(!strncmp(fqn, dir, strlen(dir)) && !access(fqn, X_OK))
            return 1; /* ok, fqn in in the dir_alias dir or in a subdir */
    }

err:
    return 0;
}

/* returns 1 if the file extension in one of those handled by cgi programs */
static int cgi_ext(http_t *h, request_t *rq, const char *fqn, 
        const char **phandler)
{
    u_config_t *config, *base;
    char buf[U_FILENAME_MAX];
    const char *handler, *ext = NULL;

    if(fqn == NULL)
        return 0;

    for(ext = NULL; *fqn; ++fqn) 
        if(*fqn == '.')
            ext = fqn;

    if(ext == NULL)
        return 0; /* file with no extension */

    ext++; /* skip '.' */

    /* get cgi. config subtree */
    dbg_err_if(cgi_get_config(h, rq, &base));

    /* get cgi. config subtree */
    nop_err_if(u_config_get_subkey(base, "cgi", &config));

    dbg_err_if(u_snprintf(buf, sizeof(buf), "%s.handler", ext));

    /* check for cgi extension handler */
    handler = u_config_get_subkey_value(config, buf);

    if(handler)
    {
        if(phandler)
            *phandler = handler;
        return 1;
    }

err:
    return 0;
}

static int cgi_setenv(cgi_env_t *env, const char *name, const char *value)
{
    enum { CHUNK = 32 };
    char *keyval = NULL, **nenv = NULL;
    int i, nl, vl;

    dbg_return_if(!env || !name || !value, ~0);

    if((nl = strlen(name)) == 0)
        return ~0;

    vl = strlen(value);

    /* alloc or realloc the array */
    if(env->size == 0 || env->size == env->count)
    {
        env->size += CHUNK;
        if(env->env == NULL)
            nenv = u_zalloc(env->size * sizeof(char*));
        else {
            nenv = u_realloc(env->env, env->size * sizeof(char*));
        }
        dbg_err_if(nenv == NULL);
        /* zero-out new elems */
        for(i = env->count; i < env->size; ++i)
            nenv[i] = NULL;
        env->env = nenv;
    }

    keyval = u_malloc(nl + vl + 2);
    dbg_err_if(keyval == NULL);

    sprintf(keyval, "%s=%s", name, value);

    env->env[env->count++] = keyval;

    return 0;
err:
    U_FREE(keyval);
    U_FREE(nenv);
    return ~0;
}

static int cgi_is_valid_uri(http_t *h, request_t *rq, const char *uri, 
        size_t len, void **handle, time_t *mtime)
{
    struct stat st; 
    char fqn[U_FILENAME_MAX];

    dbg_return_if (uri == NULL, 0);
    dbg_return_if (mtime == NULL, 0);
    dbg_return_if (len + 1 > U_FILENAME_MAX, 0);

    memcpy(fqn, uri, len);
    fqn[len] = '\0';

    /* fqn must be already normalized */
    if(strstr(fqn, ".."))
        return 0; 
    
    if(stat(fqn, &st) == 0 && S_ISREG(st.st_mode))
    {
        /* if it's not a cgi given its extension of uri then exit */
        if(!cgi_ext(h, rq, fqn, NULL) && !cgi_script(h, rq, fqn))
            return 0;

        *mtime = st.st_mtime;
        *handle = NULL;
        return 1;
    } else
        return 0;
}

static int cgi_setenv_addr(cgi_env_t *env, const char *addr, 
        const char *label_addr, const char *label_port)
{
    char buf[128];
    
    if (u_addr_get_ip(addr, buf, sizeof buf) != NULL)
        dbg_err_if (cgi_setenv(env, label_addr, buf));

    if (u_addr_get_port(addr, buf, sizeof buf) != NULL)
        dbg_err_if (cgi_setenv(env, label_port, buf));

    return 0;
err:
    return ~0;
}

static int cgi_setenv_ctype(cgi_env_t *env, request_t *rq)
{
    const char *ct;

    if((ct = request_get_field_value(rq, "Content-type")) != NULL)
        dbg_err_if(cgi_setenv(env, "CONTENT_TYPE", ct));

    return 0;
err:
    return ~0;
}

static int cgi_setenv_clen(cgi_env_t *env, request_t *rq)
{
    char buf[32];
    ssize_t len;

    if((len = request_get_content_length(rq)) > 0)
    {
        dbg_err_if(u_snprintf(buf, sizeof(buf), "%ld", (long int) len));
        dbg_err_if(cgi_setenv(env, "CONTENT_LENGTH", buf));
    }

    return 0;
err:
    return ~0;
}

static int cgi_set_blocking(int fd)
{
    int flags;

    warn_err_sif((flags = fcntl(fd, F_GETFL)) < 0);

    nop_err_if(fcntl(fd, F_SETFL, flags & (~O_NONBLOCK)) < 0);

    return 0;
err:
    return ~0;
}

static int cgi_makeenv(request_t *rq, response_t *rs, cgi_env_t *env)
{
    const char *addr, *cstr;
    header_t *h;
    field_t *field;
    char *p, buf[1024];
    unsigned int i;

    u_unused_args(rs);

    dbg_err_if(cgi_setenv(env, "SERVER_SOFTWARE", "klone/" KLONE_VERSION));
    dbg_err_if(cgi_setenv(env, "SERVER_PROTOCOL", "HTTP/1.0"));
    dbg_err_if(cgi_setenv(env, "GATEWAY_INTERFACE", "CGI/1.1"));
    dbg_err_if(cgi_setenv(env, "REDIRECT_STATUS", "200"));

    /* klone server address */
    if ((addr = request_get_addr(rq)) != NULL)
    {
        dbg_err_if(cgi_setenv_addr(env, addr, "SERVER_ADDR", "SERVER_PORT"));

        if ((u_addr_get_ip(addr, buf, sizeof(buf))) != NULL)
            dbg_err_if(cgi_setenv(env, "SERVER_NAME", buf));
    }

    /* client address */
    if ((addr = request_get_peer_addr(rq)) != NULL)
        dbg_err_if(cgi_setenv_addr(env, addr, "REMOTE_ADDR", "REMOTE_PORT"));

    /* method */
    switch(request_get_method(rq))
    {
    case HM_GET:    cstr = "GET"; break;
    case HM_HEAD:   cstr = "HEAD"; break;
    case HM_POST:   cstr = "POST"; break;
    default:        cstr = "UNKNOWN"; break;
    }
    dbg_err_if(cgi_setenv(env, "REQUEST_METHOD", cstr));

    if(io_is_secure(request_io(rq)))
        dbg_err_if(cgi_setenv(env, "HTTPS", "on"));

    if((cstr = request_get_path_info(rq)) != NULL)
        dbg_err_if(cgi_setenv(env, "PATH_INFO", cstr));

    if((cstr = request_get_resolved_path_info(rq)) != NULL)
        dbg_err_if(cgi_setenv(env, "PATH_TRANSLATED", cstr));

    if((cstr = request_get_query_string(rq)) != NULL)
        dbg_err_if(cgi_setenv(env, "QUERY_STRING", cstr));

    /* content length */
    dbg_err_if(cgi_setenv_clen(env, rq));

    /* content type*/
    dbg_err_if(cgi_setenv_ctype(env, rq));

    if((cstr = request_get_filename(rq)) != NULL)
        dbg_err_if(cgi_setenv(env, "SCRIPT_NAME", cstr));

    if((cstr = request_get_uri(rq)) != NULL)
        dbg_err_if(cgi_setenv(env, "REQUEST_URI", cstr));

    if((cstr = request_get_resolved_filename(rq)) != NULL)
        dbg_err_if(cgi_setenv(env, "SCRIPT_FILENAME", cstr));

    if((cstr = getenv("SYSTEMROOT")) != NULL)
        dbg_err_if(cgi_setenv(env, "SYSTEMROOT", cstr));

    dbg_err_if((h = request_get_header(rq)) == NULL);

    /* export all client request headers prefixing them with HTTP_ */
    for(i = 0; i < header_field_count(h); ++i)
    {
        field = header_get_fieldn(h, i);
        dbg_err_if(field == NULL);

        dbg_err_if(u_snprintf(buf, sizeof(buf), "HTTP_%s", 
                    field_get_name(field)));

        /* convert the field name to uppercase and '-' to '_' */
        for(p = buf; *p && *p != ':'; ++p)
        {
            if(*p == '-')
                *p = '_';
            else
                *p = toupper(*p);
        }

        if(field_get_value(field))
            dbg_err_if(cgi_setenv(env, buf, field_get_value(field)));
        else
            dbg_err_if(cgi_setenv(env, buf, ""));
    }

    return 0;
err:
    return ~0;
}

#define close_pipe(fd)                      \
    do {                                    \
        if(fd[0] != -1) close(fd[0]);       \
        if(fd[1] != -1) close(fd[1]);       \
    } while(0); 

static int cgi_exec(request_t *rq, response_t *rs, pid_t *pchild, 
        int *pcgi_stdin, int *pcgi_stdout)
{
    enum { RD_END /* read end point */, WR_END /* write end point */};
    int cgi_stdin[2] = { -1, -1 };
    int cgi_stdout[2] = { -1, -1 };
    cgi_env_t cgi_env = { NULL, 0, 0 };
    http_t *h;
    const char *argv[] = { NULL, NULL, NULL };
    const char *cgi_file, *handler;
    char *p, *cgi_path = NULL;
    pid_t child;
    int fd;

    dbg_err_if((h = request_get_http(rq)) == NULL);

    /* create a pair of parent<->child IPC channels */
    dbg_err_if(pipe(cgi_stdin) < 0);
    dbg_err_if(pipe(cgi_stdout) < 0);

    crit_err_if((child = fork()) < 0);

    if(child == 0)
    {   /* child */

        /* close one end of both channels */
        close(cgi_stdin[WR_END]);
        close(cgi_stdout[RD_END]);

        /* setup cgi stdout to point to the write end of the cgi_stdout pipe */
        close(STDOUT_FILENO);
        crit_err_if(dup2(cgi_stdout[WR_END], STDOUT_FILENO) < 0);
        close(cgi_stdout[WR_END]);

        /* setup cgi stdin to point to the read end of the cgi_stdin pipe */
        close(STDIN_FILENO);
        crit_err_if(dup2(cgi_stdin[RD_END], STDIN_FILENO) < 0);
        close(cgi_stdin[RD_END]);

        /* ignore cgi stderr */
        fd = open("/dev/null", O_WRONLY);
        dbg_err_if(fd < 0);
        crit_err_if(dup2(fd, STDERR_FILENO) < 0);
        close(fd);

        /* all standard descriptor must be blocking */
        cgi_set_blocking(STDOUT_FILENO);
        cgi_set_blocking(STDIN_FILENO);
        cgi_set_blocking(STDERR_FILENO);

        /* close any other open fd */
        for(fd = 3; fd < 255; ++fd) 
            close(fd);

        /* extract path name from cgi_file */
        dbg_err_if((cgi_file = request_get_resolved_filename(rq)) == NULL);

        cgi_path = u_strdup(cgi_file);
        dbg_err_if(cgi_path == NULL);

        /* cut out filename part */
        dbg_err_if((p = strrchr(cgi_path, '/')) == NULL);
        ++p; *p = 0;

        crit_err_sifm(chdir(cgi_path) < 0, "unable to chdir to %s", cgi_path);

        U_FREE(cgi_path);

        /* make the CGI environment vars array */
        crit_err_sif(cgi_makeenv(rq, rs, &cgi_env));

        /* the handler may be the path of the handler or "exec" that means that
         * the script must be run as is */
        if(!cgi_ext(h, rq, cgi_file, &handler) || !strcasecmp(handler, "exec"))
        {
            /* setup cgi argv (ISINDEX command line handling is not impl) */
            argv[0] = cgi_file;

        } else {
            /* run the handler of this file extension */
            argv[0] = handler;
            argv[1] = cgi_file;
        }

        /* run the cgi (never returns) */
        crit_err_sif(execve(argv[0], argv, cgi_env.env));

        /* never reached */

    } else if(child > 0) {
        /* parent */

        /* close one end of both channels */
        close(cgi_stdin[RD_END]);
        close(cgi_stdout[WR_END]);

        /* return cgi read/write descriptors to the parent */
        *pcgi_stdin = cgi_stdin[WR_END];
        *pcgi_stdout = cgi_stdout[RD_END];
        *pchild = child;

        return 0;

    } else {
        warn_err("fork error");
    }

err:
    if(child == 0)
        _exit(1); /* children exit here on error */
    close_pipe(cgi_stdin);
    close_pipe(cgi_stdout);
    return ~0;
}

static int cgi_serve(request_t *rq, response_t *rs)
{
    codec_t *filter = NULL;
    header_t *head = NULL;
    field_t *field = NULL;
    const char *fqn, *filename;
    char buf[4096];
    io_t *out = NULL, *cgi_in = NULL, *cgi_out = NULL;
    ssize_t n, tot = 0, clen;
    int cgi_stdin = -1, cgi_stdout = -1, status;
    pid_t child;

    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);

    /* shortcuts */
    dbg_err_if((out = response_io(rs)) == NULL);
    dbg_err_if((head = response_get_header(rs)) == NULL);

    /* if something goes wrong return a "bad request" */
    response_set_status(rs, HTTP_STATUS_BAD_REQUEST); 

    /* script file name */
    fqn = request_get_resolved_filename(rq);

    /* run the CGI and return its stdin and stdout descriptor */
    crit_err_if(cgi_exec(rq, rs, &child, &cgi_stdin, &cgi_stdout));

    /* by default disable caching */
    response_disable_caching(rs);

    /* copy any POST input data to the CGI */
    if(request_get_method(rq) == HM_POST && 
            (clen = request_get_content_length(rq)) > 0)
    {
        /* build an io_t object to read cgi output */
        crit_err_sif(io_fd_create(cgi_stdin, O_WRONLY, &cgi_out));

        /* FIXME 
           if the cgi does not read from stdin (and POSTed data is big) 
           we could be block here waiting for the buffer to drain 
         */
        
        /* send POSTed data to the cgi (the script may not read it so we don't 
         * complain on broken pipe error) */
        crit_if(io_copy(cgi_out, request_io(rq), clen) < 0);

        io_free(cgi_out); cgi_out = NULL;
        close(cgi_stdin); cgi_stdin = -1;
    }

    /* build an io_t object to read cgi output */
    crit_err_sif(io_fd_create(cgi_stdout, O_RDONLY, &cgi_in));

    /* extract filename part of the fqn */
    crit_err_if((filename = strrchr(fqn, '/')) == NULL);
    filename++;

    /* header of cgis whose name start with nhp- must not be parsed */
    if(strncmp(filename, "nph-", 4))
    {
        /* create a response filter (used to automatically print the header) */
        dbg_err_if(response_filter_create(rq, rs, NULL, &filter));
        io_codec_add_tail(out, filter);
        filter = NULL; /* io_t owns it */

        /* merge cgi header with response headers */
        crit_err_if(header_load_ex(head, cgi_in, HLM_OVERRIDE));

        /* set the response code */
        if((field = header_get_field(head, "Status")) != NULL && 
                field_get_value(field))
        {
            response_set_status(rs, atoi(field_get_value(field)));
        } else {
            if(header_get_field(head, "Location"))
                response_set_status(rs, HTTP_STATUS_MOVED_TEMPORARILY);
            else
                response_set_status(rs, HTTP_STATUS_OK); 
        }
    } else
        response_set_status(rs, HTTP_STATUS_OK); 

    /* write cgi output to the client */
    while((n = io_read(cgi_in, buf, sizeof(buf))) > 0)
    {
        if(io_write(out, buf, n) < 0)
            break;
        tot += n;
    }

    /* if nothing has been printed by the script; write a dummy byte so 
     * the io_t calls the filter function that, in turn, will print out the 
     * HTTP header (rsfilter will handle it) */
    if(tot == 0)
        io_write(out, "\n", 1);

    if(cgi_in)
        io_free(cgi_in); 
    if(cgi_out)
        io_free(cgi_out); 

    close(cgi_stdin);
    close(cgi_stdout);

    /* wait for the child to finish (FIXME add a max timeout) */
    waitpid(child, &status, 0);
    if(WIFEXITED(status) && WEXITSTATUS(status))
        warn("cgi exited with [%d]", WEXITSTATUS(status));

    return 0;
err:
    if(cgi_out)
        io_free(cgi_out);
    if(cgi_in)
        io_free(cgi_in);
    if(cgi_stdin != -1)
        close(cgi_stdin);
    if(cgi_stdout != -1)
        close(cgi_stdout);
    return ~0;
}

static int cgi_init(void)
{
    return 0;
}

static void cgi_term(void)
{
    return;
}

supplier_t sup_cgi = {
    "cgi supplier",
    cgi_init,
    cgi_term,
    cgi_is_valid_uri,
    cgi_serve
};

