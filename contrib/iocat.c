#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <getopt.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <klone/klone.h>
#include <klone/debug.h>
#include <klone/io.h>
#include <klone/utils.h>
#include <klone/codgzip.h>

int facility = LOG_LOCAL0;

typedef struct ctx_s
{
    char *file_in, *file_out;
    char **arg;
    size_t narg;
    int encode;
    int decode;
} context_t;

context_t context, *ctx = &context;

static void error(char *msg)
{
    fprintf(stderr, "err: %s", msg);
    exit(1);
}

static void usage()
{
    fprintf(stderr, 
        "Usage: iocat [-ed] [infile [outfile]]  \n"
        "           -e    encode                \n"
        "           -d    decode                \n"
        );
    exit(1);
}

static void parse_opt(int argc, char **argv)
{
    int ret;

    while((ret = getopt(argc, argv, "de")) != -1)
    {
        switch(ret)
        {
        case 'd':
            ctx->decode++;
            break;
        case 'e': 
            ctx->encode++;
            break;
        default:
        case 'h': 
            usage();
        }
    }
    /* sanity checks */
    if(ctx->encode && ctx->decode)
        error("just one of -e or -d may be used");

    ctx->narg = argc - optind;
    ctx->arg = argv + optind;

    if(ctx->narg > 0)
        ctx->file_in = ctx->arg[0];

    if(ctx->narg > 1)
        ctx->file_out = ctx->arg[1];
}

int main(int argc, char **argv)
{
    ssize_t c;
    io_t *in, *out;
    codec_gzip_t *fi = NULL;
    
    memset(ctx, 0, sizeof(context_t));

    parse_opt(argc, argv);

    /* open the input stream */
    if(ctx->file_in)
        dbg_err_if(u_file_open(ctx->file_in, O_RDONLY, &in));
    else
        dbg_err_if(io_fd_create(0, 0, &in));

    /* open the output stream */
    if(ctx->file_out)
        dbg_err_if(u_file_open(ctx->file_out, O_WRONLY | O_CREAT | O_TRUNC, 
            &out));
    else
        dbg_err_if(io_fd_create(1, 0, &out));

    if(ctx->decode)
        dbg_err_if(codec_gzip_create(GZIP_UNCOMPRESS, &fi));
    else if(ctx->encode)
        dbg_err_if(codec_gzip_create(GZIP_COMPRESS, &fi));
     
    if(fi)
        dbg_err_if(io_set_codec(out, fi));

    while((c = io_pipe(out, in)) > 0)
         ;

    io_free(in);
    io_free(out);

    return EXIT_SUCCESS;
err:
    return EXIT_FAILURE;
}

