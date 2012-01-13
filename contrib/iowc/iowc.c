/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: iowc.c,v 1.4 2006/09/24 13:26:18 tat Exp $
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
} context_t;

context_t context, *ctx = &context;

static void usage()
{
    fprintf(stderr, 
        "Usage: iowc [infile]  \n"
        );
    exit(1);
}

static void parse_opt(int argc, char **argv)
{
    int ret;

    while((ret = getopt(argc, argv, "h")) != -1)
    {
        switch(ret)
        {
        default:
        case 'h': 
            usage();
        }
    }

    ctx->narg = argc - optind;
    ctx->arg = argv + optind;

    if(ctx->narg > 0)
        ctx->file_in = ctx->arg[0];
}

int main(int argc, char **argv)
{
    enum { LINE_BUFSZ = 4096 };
    ssize_t c;
    io_t *in;
    int n;
    char line[LINE_BUFSZ];
    
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

    for(n = 0; (c = io_gets(in, line, LINE_BUFSZ)) > 0; ++n)
        ;

    fprintf(stdout, "%d lines\n", n);

    dbg_if(io_free(in));

    return EXIT_SUCCESS;
err:
    return EXIT_FAILURE;
}

