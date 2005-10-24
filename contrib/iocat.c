#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <getopt.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <klone/klone.h>
#include <klone/io.h>
#include <klone/utils.h>
#include <klone/codecs.h>
#include <u/libu.h>

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

static void error(const char *msg)
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
    codec_null_t *null0 = NULL;
    codec_null_t *null1 = NULL;
    codec_null_t *null2 = NULL;
    codec_null_t *null3 = NULL;
    
    memset(ctx, 0, sizeof(context_t));

    parse_opt(argc, argv);

    /* open the input stream */
    if(ctx->file_in)
    {
        dbg_err_if(u_file_open(ctx->file_in, O_RDONLY, &in));
        dbg_err_if(io_name_set(in, ctx->file_in));
    } else {
        dbg_err_if(io_fd_create(0, 0, &in));
        dbg_err_if(io_name_set(in, "stdin"));
    }

    /* open the output stream */
    if(ctx->file_out)
    {
        dbg_err_if(u_file_open(ctx->file_out, O_WRONLY | O_CREAT | O_TRUNC, 
            &out));
        dbg_err_if(io_name_set(out, ctx->file_out));
    } else {
        dbg_err_if(io_fd_create(1, 0, &out));
        dbg_err_if(io_name_set(out, "stdout"));
    }

    dbg_err_if(codec_null_create(&null0));
    dbg_err_if(codec_null_create(&null1));
    dbg_err_if(codec_null_create(&null2));
    dbg_err_if(codec_null_create(&null3));

    if(ctx->decode)
        dbg_err_if(codec_gzip_create(GZIP_UNCOMPRESS, &fi));
    else if(ctx->encode)
        dbg_err_if(codec_gzip_create(GZIP_COMPRESS, &fi));

    if(fi)
    {
        /* for testing purpose attach the encode codec on input stream and 
         * the decode one on the output */
        if(ctx->encode)
        {
            dbg_err_if(io_codec_add_tail(in, (codec_t*)null0));
            dbg_err_if(io_codec_add_tail(in, (codec_t*)null1));
            dbg_err_if(io_codec_add_tail(in, (codec_t*)fi));
            dbg_err_if(io_codec_add_tail(in, (codec_t*)null2));
            dbg_err_if(io_codec_add_tail(in, (codec_t*)null3));
        } else {
            dbg_err_if(io_codec_add_tail(out, (codec_t*)null0));
            dbg_err_if(io_codec_add_tail(out, (codec_t*)null1));
            dbg_err_if(io_codec_add_tail(out, (codec_t*)fi));
            dbg_err_if(io_codec_add_tail(out, (codec_t*)null2));
            dbg_err_if(io_codec_add_tail(out, (codec_t*)null3));
        }
    }


    while((c = io_pipe(out, in)) > 0)
         ;

    dbg_if(io_free(in));
    dbg_if(io_free(out));

    return EXIT_SUCCESS;
err:
    return EXIT_FAILURE;
}

