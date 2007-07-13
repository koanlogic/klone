/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: sup_cgi.c,v 1.2 2007/07/13 23:08:24 tat Exp $
 */

#include "klone_conf.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <klone/supplier.h>
#include <klone/io.h>
#include <klone/utils.h>

static int cgi_is_valid_uri(const char *uri, size_t len, time_t *mtime)
{
    struct stat st; 
    char fqn[1+U_FILENAME_MAX];

    dbg_return_if (uri == NULL, 0);
    dbg_return_if (mtime == NULL, 0);
    dbg_return_if (len > U_FILENAME_MAX, 0);

    memcpy(fqn, uri, len);
    fqn[len] = 0;
    
    if( stat(fqn, &st) == 0 && S_ISREG(st.st_mode))
    {
        *mtime = st.st_mtime;
        return 1;
    } else
        return 0;
}

static int cgi_setenv_addr(addr_t *addr, 
        const char *label_addr, const char *label_port)
{
    const char *cstr;
    char buf[128];

    dbg_return_if(addr->type == ADDR_UNIX, 0);

#ifndef NO_IPV6
    cstr = inet_ntop( addr->type == ADDR_IPV4 ? AF_INET : AF_INET6,
                (addr->type == ADDR_IPV4 ?  
                    (const void*)&(addr->sa.sin.sin_addr) : 
                    (const void*)&(addr->sa.sin6.sin6_addr)),
                 buf, sizeof(buf));
#else
    cstr = inet_ntoa(addr->sa.sin.sin_addr);
#endif

    if(cstr)
        dbg_err_if(setenv(label_addr, cstr, 1));

    u_snprintf(buf, sizeof(buf), "%u", ntohs(addr->sa.sin.sin_port));
    dbg_err_if(setenv(label_port, buf, 1));

    return 0;
err:
    return ~0;
}

static int cgi_setenv_clen(request_t *rq)
{
    char buf[32];
    ssize_t len;

    if((len = request_get_content_length(rq)) > 0)
    {
        dbg_err_if(u_snprintf(buf, sizeof(buf), "%ld", len));
        dbg_err_if(setenv("CONTENT_LENGTH", buf, 1));
    }

    return 0;
err:
    return ~0;
}

static int cgi_set_blocking(int fd)
{
    int flags;

    warn_err_sif((flags = fcntl(fd, F_GETFL)) < 0);

    warn_err_sif(fcntl(fd, F_SETFL, flags & (~O_NONBLOCK)) < 0);;

    return 0;
err:
    return ~0;
}

static int cgi_setenv(request_t *rq, response_t *rs)
{
    addr_t *addr;
    header_t *h;
    field_t *field;
    const char *cstr;
    char *p, buf[1024];
    int i;

    dbg_if(clearenv());

    dbg_err_if(setenv("SERVER_SOFTWARE", "klone/" KLONE_VERSION, 1));
    dbg_err_if(setenv("SERVER_PROTOCOL", "HTTP/1.0", 1));
    dbg_err_if(setenv("GATEWAY_INTERFACE", "CGI/1.1", 1));
    dbg_err_if(setenv("REDIRECT_STATUS", "200", 1));

    /* klone server address */
    if((addr = request_get_addr(rq)) != NULL) 
    {
        dbg_err_if(cgi_setenv_addr(addr, "SERVER_ADDR", "SERVER_PORT"));
        dbg_err_if(setenv("SERVER_NAME", getenv("SERVER_ADDR"), 1));
    }

    /* client address */
    if((addr = request_get_peer_addr(rq)) != NULL) 
        dbg_err_if(cgi_setenv_addr(addr, "REMOTE_ADDR", "REMOTE_PORT"));

    /* method */
    switch(request_get_method(rq))
    {
    case HM_GET:    cstr = "GET"; break;
    case HM_HEAD:   cstr = "HEAD"; break;
    case HM_POST:   cstr = "POST"; break;
    default:
        cstr = "UNKNOWN";
    }
    dbg_err_if(setenv("REQUEST_METHOD", cstr, 1));

    if(io_is_secure(request_io(rq)))
        dbg_err_if(setenv("HTTPS", "on", 1));

    if((cstr = request_get_path_info(rq)) != NULL)
        dbg_err_if(setenv("PATH_INFO", cstr, 1));

    if((cstr = request_get_resolved_path_info(rq)) != NULL)
        dbg_err_if(setenv("PATH_TRANSLATED", cstr, 1));

    if((cstr = request_get_query_string(rq)) != NULL)
        dbg_err_if(setenv("QUERY_STRING", cstr, 1));

    /* CONTENT_LENGTH */
    dbg_err_if(cgi_setenv_clen(rq));

    if((cstr = request_get_filename(rq)) != NULL)
        dbg_err_if(setenv("SCRIPT_NAME", cstr, 1));

    if((cstr = request_get_resolved_filename(rq)) != NULL)
        dbg_err_if(setenv("SCRIPT_FILENAME", cstr, 1));

    if((cstr = getenv("SYSTEMROOT")) != NULL)
        dbg_err_if(setenv("SYSTEMROOT", cstr, 1));

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
            dbg_err_if(setenv(buf, field_get_value(field), 1));
        else
            dbg_err_if(setenv(buf, "", 1));
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

int cgi_exec(request_t *rq, response_t *rs, pid_t *pchild, 
        int *pcgi_stdin, int *pcgi_stdout)
{
    enum { RD_END /* read end point */, WR_END /* write end point */};
    int cgi_stdin[2] = { -1, -1 };
    int cgi_stdout[2] = { -1, -1 };
    int fd;
    const char *cgi_file;
    char *p, *cgi_path = NULL;
    pid_t child;

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
        dbg_err_if(cgi_set_blocking(STDOUT_FILENO));
        dbg_err_if(cgi_set_blocking(STDIN_FILENO));
        dbg_err_if(cgi_set_blocking(STDERR_FILENO));

        /* close any other open fd */
        for(fd = 3; fd < 255; ++fd) 
            close(fd);

        /* extract path name from cgi_file */
        dbg_err_if((cgi_file = request_get_resolved_filename(rq)) == NULL);
        cgi_path = u_strdup(cgi_file);
        dbg_err_if(cgi_path == NULL);

        /* cut out filename part */
        dbg_err_if((p = strrchr(cgi_path, U_PATH_SEPARATOR)) == NULL);
        ++p; *p = 0;

        crit_err_sifm(chdir(cgi_path) < 0, "unable to chdir to %s", cgi_path);

        /* set the CGI environment variables */
        crit_err_sif(cgi_setenv(rq, rs));

        /* setup cgi argv (fixme: ISINDEX command line handling is missing) */
        const char *argv[] = { cgi_file, NULL };;

        /* run the cgi (never returns) */
        crit_err_sif(execv(cgi_file, argv));

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
    FILE *fp = NULL;
    io_t *out = NULL, *cgi_in = NULL, *cgi_out = NULL;;
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

    warn_ifm(access(fqn, X_OK), 
            "[%s] doesn't seem to be executable (bad perm?)", fqn);

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
    crit_err_if((filename = strrchr(fqn, U_PATH_SEPARATOR)) == NULL);
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

    /* if nothing has been printed by the sciprt then write a dummy byte so 
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

