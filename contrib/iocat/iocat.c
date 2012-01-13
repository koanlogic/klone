/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: iocat.c,v 1.8 2006/11/02 09:08:05 tho Exp $
 */

#include "klone_conf.h"
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
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
    int comp;
    int cipher;
} context_t;

context_t context, *ctx = &context;

static void print_error(const char *msg)
{
    fprintf(stderr, "err: %s\n", msg);
    exit(1);
}

static void usage()
{
    fprintf(stderr, 
        "Usage: iocat [-"
        #ifdef SSL_ON
        "c"
        #endif
        "d"
        "e"
        #ifdef HAVE_LIBZ
        "z"
        #endif
        "] [infile [outfile]]  \n"
        #ifdef SSL_ON
        "           -c    use AES256 codec  \n"
        #endif
        "           -d    decode                    \n"
        "           -h    print this help and exit  \n"
        "           -e    encode                    \n"
        #ifdef HAVE_LIBZ
        "           -z    use libz codec            \n"
        #endif
        );
    exit(1);
}

static void parse_opt(int argc, char **argv)
{
    int ret;

    while((ret = getopt(argc, argv, "hcdez")) != -1)
    {
        switch(ret)
        {
        case 'd':
            ctx->decode++;
            break;
        case 'e': 
            ctx->encode++;
            break;
        case 'z': 
            ctx->comp++;
            break;
        case 'c': 
            ctx->cipher++;
            break;
        default:
        case 'h': 
            usage();
        }
    }
    /* sanity checks */
    if(ctx->encode && ctx->decode)
        print_error("just one of -e or -d may be used");
    
    if(ctx->encode || ctx->decode)
    {
        if(!ctx->comp && !ctx->cipher)
            print_error("-z and/or -c must be used with -e and -d");
    }

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
    codec_t *null0 = NULL;
    codec_t *null1 = NULL;
    codec_t *null2 = NULL;
    codec_t *null3 = NULL;
    codec_t *null4 = NULL;
    codec_t *zip = NULL;
    codec_t *unzip = NULL;
    codec_t *encrypt = NULL;
    codec_t *decrypt = NULL;
    unsigned char key[CODEC_CIPHER_KEY_BUFSZ];
    unsigned char iv[CODEC_CIPHER_IV_LEN];
    
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

    /* create some null codec to stress-test the io_t */
    dbg_err_if(codec_null_create(&null0));
    dbg_err_if(codec_null_create(&null1));
    dbg_err_if(codec_null_create(&null2));
    dbg_err_if(codec_null_create(&null3));
    dbg_err_if(codec_null_create(&null4));

    /* zip */
    #ifdef HAVE_LIBZ
    dbg_err_if(codec_gzip_create(GZIP_COMPRESS, &zip));
    dbg_err_if(codec_gzip_create(GZIP_UNCOMPRESS, &unzip));
    #endif

    /* aes256 */
    #ifdef SSL_ON
    memset(key, 0, sizeof(key));
    memset(iv, 0, sizeof(iv));
    strcpy(key, "pwd");
    strcpy(iv, "iv");

    dbg_err_if(codec_cipher_create(CIPHER_ENCRYPT, EVP_aes_256_cbc(),
            key, iv, &encrypt));
    dbg_err_if(codec_cipher_create(CIPHER_DECRYPT, EVP_aes_256_cbc(),
            key, iv, &decrypt));
    #endif

    if(ctx->encode || ctx->decode)
    {
        /* for testing purpose attach the encode codec on input stream and 
         * the decode one on the output; also add a few null codecs  */
        if(ctx->encode)
        {
            dbg_err_if(io_codec_add_tail(in, null0));
            dbg_err_if(io_codec_add_tail(in, null1));
            if(ctx->comp)
                dbg_err_if(io_codec_add_tail(in, zip));
            dbg_err_if(io_codec_add_tail(in, null2));
            if(ctx->cipher)
                dbg_err_if(io_codec_add_tail(in, encrypt));
            dbg_err_if(io_codec_add_tail(in, null3));
            dbg_err_if(io_codec_add_tail(in, null4));
        } else {
            dbg_err_if(io_codec_add_tail(out, null0));
            dbg_err_if(io_codec_add_tail(out, null1));
            if(ctx->cipher)
                dbg_err_if(io_codec_add_tail(out, decrypt));
            dbg_err_if(io_codec_add_tail(out, null2));
            if(ctx->comp)
                dbg_err_if(io_codec_add_tail(out, unzip));
            dbg_err_if(io_codec_add_tail(out, null3));
            dbg_err_if(io_codec_add_tail(out, null4));
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

