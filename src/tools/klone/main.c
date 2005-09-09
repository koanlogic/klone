#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <klone/klone.h>
#include <klone/debug.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/translat.h>
#include <klone/utils.h>
#include <klone/str.h>
#include <klone/run.h>
#include <klone/mime_map.h>

#define err(args...) \
    do { dbg(args); fprintf(stderr, args); fprintf(stderr, "\n"); } while(0)

#define err_if(cond, args...) \
    do { if(cond) err(args);  } while(0)

#define die(args...)  \
    do { err(args); exit(EXIT_FAILURE); } while(0)

#define die_if(cond, args...)  \
    do { dbg_ifb(cond) die(args); } while(0)

typedef enum action_e
{
    ACTION_NONE,
    ACTION_TRANSLATE,
    ACTION_MAKE,
    ACTION_RUN
} action_t;

enum flags_e
{
    FLAG_VERBOSE,
    FLAG_DEBUG
};

typedef struct 
{
    action_t action;
    char *file_in, *file_out;
    char *uri;
    unsigned int flags;
    char **arg;
    size_t narg;
} context_t;

/* global context */
static context_t g_ctx, *ctx = &g_ctx;

void usage(void)
{
    static const char * us = 
    "usage: klone [-ctr][-i in_file][-o out_file]                   \n"
    "             -h       display this help                      \n"
    "           actions                                           \n"
    "             -c       compile a klone to a cxo file            \n"
    "             -r       run cxo file                           \n"
    "             -t       translate a web page file to C         \n"
    "           params                                            \n"
    "             -d       debug messages on                      \n"
    "             -i file  input file                             \n"
    "             -o file  output file                            \n"
    "             -v       verbose mode                           \n"
    ;

    fprintf(stderr, us);

    exit(EXIT_FAILURE);
}

void parse_opt(int argc, char **argv)
{
    int ret;

    while((ret = getopt(argc, argv, "cdhi:o:ru:tv")) != -1)
    {
        switch(ret)
        {
        case 'c': 
            die_if(ctx->action, "just one action switch is allowed");
            ctx->action = ACTION_MAKE;
            break;
        case 'd': /* debug on */
            ctx->flags |= FLAG_DEBUG;
            break;
        case 'i': /* input file */
            ctx->file_in = u_strdup(optarg);
            break;
        case 'o': /* output file */
            ctx->file_out = u_strdup(optarg);
            break;
        case 'r': /* run action */
            die_if(ctx->action, "just one action switch is allowed");
            ctx->action = ACTION_RUN;
            break;
        case 't': /* translate action */
            die_if(ctx->action, "just one action switch is allowed");
            ctx->action = ACTION_TRANSLATE;
            break;
        case 'u': /* translated page uri */
            /* skip the first char to avoid MSYS path translation bug
             * (see klone-site.c) */
            ctx->uri = u_strdup(1+optarg);
            break;
        case 'v': /* verbose on */
            ctx->flags |= FLAG_VERBOSE;
            break;
        default:
        case 'h': 
            usage();
        }
    }
    /* sanity checks */
    die_if(!ctx->action, "one action switch must be specified");

    ctx->narg = argc - optind;
    ctx->arg = argv + optind;
}

int action_run(const char *cxo)
{
    request_t *rq;
    response_t *rs;
    io_t *in, *out;

    dbg_err_if(request_create(NULL, &rq));
    dbg_err_if(response_create(NULL, &rs));

    dbg_err_if(io_fd_create(0 /* stdin  */, 0, &in));
    dbg_err_if(io_fd_create(1 /* stdout */, 0, &out));

    request_bind(rq, in);
    response_bind(rs, out);

    dbg_if(run_page(cxo, rq, rs));

    request_free(rq);
    response_free(rs);

    return 0;
err:
    return ~0;
}

int action_trans()
{
    trans_info_t ti;
    mime_map_t *mm;
    struct stat st;

    /* input file */
    strncpy(ti.file_in, ctx->file_in, NAME_BUFSZ);

    /* output file */
    strncpy(ti.file_out, ctx->file_out, NAME_BUFSZ);

    /* uri */
    strncpy(ti.uri, ctx->uri, MIME_BUFSZ);

    /* mime_type */
    if((mm = u_get_mime_map(ctx->file_in)) != NULL)
    {
        strncpy(ti.mime_type, mm->mime_type, MIME_BUFSZ);
        ti.comp = mm->comp;
    } else {
        strncpy(ti.mime_type, "application/octect-stream", MIME_BUFSZ);
        ti.comp = 1; /* try to compress, libz never enlarge uncompressables */
    }

    dbg_err_if(stat(ctx->file_in, &st));

    ti.file_size = st.st_size;
    ti.mtime = st.st_mtime; 

    dbg_err_if(translate(&ti));

    return 0;
err:
    return ~0;
}


int main(int argc, char **argv)
{
    size_t i;

    /* parse command line switches */
    parse_opt(argc, argv);

    switch(ctx->action)
    {
    case ACTION_TRANSLATE:
        die_if(!ctx->file_in, "input file name required (-i file)");
        die_if(!ctx->file_out, "output file name required (-o file)");
        die_if(!ctx->uri, "translated page URI required (-u uri)");
        dbg_err_if(action_trans());
        break;
    case ACTION_MAKE:
        break;
    case ACTION_RUN:
        die_if(!ctx->narg,  "file name required");
        for(i = 0; i < ctx->narg; ++i)
            action_run(ctx->arg[i]);
        break;
    default:
        err("unknown action switch");
    }

    return EXIT_SUCCESS;
err:
    return EXIT_FAILURE;
}
