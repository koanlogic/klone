#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <klone/klone.h>
#include <klone/debug.h>
#include <klone/server.h>
#include <klone/config.h>
#include <klone/emb.h>
#include "conf.h"
#include "context.h"
#include "main.h"

extern context_t* ctx;

static void sigint(int sig)
{
    dbg("SIGINT");
    server_stop(ctx->server);
}

static void sigterm(int sig)
{
    dbg("SIGTERM");
    server_stop(ctx->server);
}

static void sigchld(int sig)
{
    pid_t           pid = -1;
    int             status;

    /* detach from child processes */
    while((pid = waitpid(-1, &status, WNOHANG)) > 0) 
    {
        if(WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
            warn("pid [%u], exit code [%d]", pid, WEXITSTATUS(status));

        if(WIFSIGNALED(status))
            warn("pid [%u], signal [%d]", pid, WTERMSIG(status));
    }
}

int app_init()
{
    io_t *io = NULL;

    /* init embedded resources */
    emb_init();
    
    /* create a config obj */
    dbg_err_if(config_create(&ctx->config));

    /* get the io associated to the embedded configuration file (if any) */
    dbg_if(u_emb_open("/etc/kloned.conf", &io));

    /* load the embedded config */
    if(io)
        dbg_err_if(config_load(ctx->config, io, 0));

    io_free(io);
    io = NULL;

    /* load the external (-f command line switch) config file */
    if(ctx->ext_config)
    {
        dbg("loading external config file: %s", ctx->ext_config);
        dbg_err_if(u_file_open(ctx->ext_config, O_RDONLY, &io));

        dbg_err_if(config_load(ctx->config, io, 1 /* overwrite */));

        io_free(io);
        io = NULL;
    }

    if(ctx->debug)
        config_print(ctx->config, 0);

    dbg_err_if(modules_init(ctx));

    return 0;
err:
    if(io)
        io_free(io);
    app_term();
    return ~0;
}

int app_term()
{
    modules_term(ctx);

    if(ctx && ctx->config)
    {
        config_free(ctx->config);
        ctx->config = NULL;
    }

    if(ctx && ctx->server)
    {
        server_free(ctx->server);
        ctx->server = NULL;
    }

    emb_term();

    return 0;
}

int app_run()
{
    int model;

    /* set signal handlers */
    dbg_err_if(u_signal(SIGINT, sigint));
    dbg_err_if(u_signal(SIGTERM, sigterm));
    #ifdef OS_UNIX 
    dbg_err_if(u_signal(SIGPIPE, SIG_IGN));
    dbg_err_if(u_signal(SIGCHLD, sigchld));
    #endif

    /* fork() if the server has been launched w/ -F */
    model = ctx->daemon ? SERVER_MODEL_FORK : SERVER_MODEL_SERIAL;

    /* create a server object and start its main loop */
    dbg_err_if(server_create(ctx->config, model, &ctx->server));

    if(getenv("GATEWAY_INTERFACE"))
        dbg_err_if(server_cgi(ctx->server));
    else
        dbg_err_if(server_loop(ctx->server));

    return EXIT_SUCCESS;
err:
    return EXIT_FAILURE;
}

