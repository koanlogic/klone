#include "klone_conf.h"
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <u/libu.h>
#include <klone/klone.h>
#include <klone/server.h>
#include <klone/emb.h>
#include <klone/context.h>
#include <klone/utils.h>
#include "main.h"
#include "server_s.h"

extern context_t* ctx;
extern int modules_init(context_t *);
extern int modules_term(context_t *);

static char *io_gets_cb(void *arg, char *buf, size_t size)
{
    io_t *io = (io_t*)arg;

    dbg_err_if(io_gets(io, buf, size) <= 0);

    return buf;
err: 
    return NULL;
}

int app_init()
{
    io_t *io = NULL;
    int cfg_found = 0;

    /* init embedded resources */
    emb_init();
    
    /* create a config obj */
    dbg_err_if(u_config_create(&ctx->config));

    /* get the io associated to the embedded configuration file (if any) */
    dbg_if(emb_open("/etc/kloned.conf", &io));

    /* load the embedded config */
    if(io)
    {
        dbg_err_if(u_config_load_from(ctx->config, io_gets_cb, io, 0));
        cfg_found = 1;
        io_free(io);
        io = NULL;
    }

    /* load the external (-f command line switch) config file */
    if(ctx->ext_config)
    {
        dbg("loading external config file: %s", ctx->ext_config);
        dbg_err_if(u_file_open(ctx->ext_config, O_RDONLY, &io));

        dbg_err_if(u_config_load_from(ctx->config, io_gets_cb, io, 1));
        cfg_found = 1;

        io_free(io);
        io = NULL;
    }

    con_err_ifm(cfg_found == 0, 
        "missing config file (use -f file or embed /etc/kloned.conf");

    if(ctx->debug)
        u_config_print(ctx->config, 0);

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
        u_config_free(ctx->config);
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
    /* create a server object and start its main loop */
    dbg_err_if(server_create(ctx->config, !ctx->daemon, &ctx->server));

    if(getenv("GATEWAY_INTERFACE"))
        dbg_err_if(server_cgi(ctx->server));
    else
        dbg_err_if(server_loop(ctx->server));

    return EXIT_SUCCESS;
err:
    return EXIT_FAILURE;
}

