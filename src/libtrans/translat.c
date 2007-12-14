/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: translat.c,v 1.26 2007/12/14 13:18:58 tat Exp $
 */

#include "klone_conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#ifdef HAVE_LIBOPENSSL
#include <openssl/evp.h>
#endif /* HAVE_LIBOPENSSL */
#include <u/libu.h>
#include <klone/os.h>
#include <klone/translat.h>
#include <klone/parser.h>
#include <klone/utils.h>
#include <klone/os.h>
#include <klone/io.h>
#include <klone/codec.h>
#include <klone/codecs.h>

#define tr_err(...)             \
    do  { con_p_ctx(p); con_err(__VA_ARGS__); } while(0)
#define tr_err_if(expr)          \
    do { if( (expr) ) { con_p_ctx(p); con("%s", #expr); goto err; } } while(0)
#define tr_err_ifm(expr, ...)    \
    do { if( (expr) ) { con_p_ctx(p); con(__VA_ARGS__); goto err; } } while(0)

static int preprocess(io_t *in, io_t *out);

/* print parser context to the console */
static void con_p_ctx(parser_t *p)
{
    char fn[U_FILENAME_MAX];

    dbg_err_if(io_name_get(p->in, fn, U_FILENAME_MAX));

    /* con_ macro should be used here; we'd need a con_no_newline(...) */
    fprintf(stderr, "[%s:%d]: error:  ", fn, p->code_line);
err:
    return;
}

static int is_a_script(const char *filename)
{
    static const char *script_ext[] = { 
        ".klone", ".kl1", ".klc", 
        ".klx",  /* C++ page */
        NULL 
    };
    const char **ext;

    dbg_return_if(filename == NULL, 0);

    /* try to find an index page between default index uris */
    for(ext = script_ext; *ext; ++ext)
    {
        /* case insensitive matching */
        if(u_match_ext(filename, *ext))
            return 1;
    }
    return 0;
}

static int process_directive_include(parser_t *p, char *inc_file)
{
    enum { BUFSZ = 4096 };
    char buf[U_FILENAME_MAX], *pc;
    char file[U_FILENAME_MAX];
    io_t *io = NULL;

    dbg_return_if (p == NULL, ~0);
    dbg_return_if (inc_file == NULL, ~0);

    dbg_err_if(io_name_get(p->in, file, U_FILENAME_MAX));
    dbg_err_if(io_name_get(p->in, buf, U_FILENAME_MAX));

    /* remove file name, just path is needed */
    dbg_err_if((pc = strrchr(buf, '/')) == NULL);
    ++pc; *pc = 0;

    dbg_err_if(strlen(buf) + strlen(inc_file) >= BUFSZ);

    strcat(buf, inc_file);

    /* copy include file to p->out */
    tr_err_ifm(u_file_open(buf, O_RDONLY, &io), 
        "unable to open included file %s", buf);

    dbg_err_if(io_printf(p->out, "<%% #line 1 \"%s\" \n %%>", buf));

    dbg_err_if(preprocess(io, p->out));

    dbg_err_if(io_printf(p->out, "<%% #line %d \"%s\" \n %%>", 
        p->code_line, file));

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int process_directive(parser_t *p, char *buf)
{
    char *tok, *pp;

    dbg_return_if (p == NULL, ~0);
    dbg_return_if (buf == NULL, ~0);

    /* get preprocessor command */
    tr_err_ifm((tok = strtok_r(buf, " \t", &pp)) == NULL,
        "bad or missing preprocessor command");

    if(strcasecmp(tok, "include") == 0)
    {
        /* get include file name */
        tr_err_ifm((tok = strtok_r(NULL, " \t\"", &pp)) == NULL,
            "bad or missing include filename");

        dbg_err_if(process_directive_include(p, tok));
    } else {
        tr_err("unknown preprocessor directive: %s", tok);
    }

    return 0;
err:
    return ~0;
}

static int parse_directive(parser_t *p, void *arg, const char *buf, size_t sz)
{
    enum { LINE_BUFSZ = 1024 };
    char line[LINE_BUFSZ];
    io_t *io = NULL;

    u_unused_args(arg);

    dbg_return_if (p == NULL, ~0);
    dbg_return_if (buf == NULL, ~0);
    
    dbg_err_if(io_mem_create((char*)buf, sz, 0, &io));

    while(io_gets(io, line, LINE_BUFSZ) > 0)
        dbg_err_if(process_directive(p, line));

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int cb_pre_html_block(parser_t *p, void *arg, const char *buf, size_t sz)
{
    u_unused_args(arg);

    dbg_err_if (p == NULL);

    dbg_err_if(io_write(p->out, buf, sz) < 0);

    return 0;
err:
    return ~0;
}

static int cb_pre_code_block(parser_t *p, int cmd, void *arg, const char *buf, 
        size_t sz)
{
    char file[U_FILENAME_MAX];

    dbg_err_if (p == NULL);

    if(cmd == '@')
    {   /* do preprocess */
        dbg_err_if(parse_directive(p, arg, buf, sz));
    } else {
        dbg_err_if(io_name_get(p->in, file, U_FILENAME_MAX));

        if(cmd != '=')
            dbg_err_if(io_printf(p->out, "<%%%c #line %d \"%s\" \n%%>", 
                (cmd == 0 ? ' ' : cmd), p->code_line, file)); 
        else
            dbg_err_if(io_printf(p->out, "<%% #line %d \"%s\" \n%%>", 
                p->code_line, file)); 

        dbg_err_if(io_printf(p->out, "<%%%c ", (cmd == 0 ? ' ' : cmd)) < 0);
        dbg_err_if(io_write(p->out, buf, sz) < 0);
        dbg_err_if(io_printf(p->out, "%%>") < 0);

        /* placeholder to be subst'd in the last translation phase */
        dbg_err_if(io_printf(p->out, "<%%%c #line 0 __PG_FILE_C__ \n%%>", 
                    (cmd == '!' ? cmd : ' ')));
    }

    return 0;
err:
    return ~0;
}

static int preprocess(io_t *in, io_t *out)
{
    parser_t *p = NULL;

    /* create a parse that reads from in and writes to out */
    dbg_err_if(parser_create(&p));

    parser_set_io(p, in, out);

    parser_set_cb_code(p, cb_pre_code_block);
    parser_set_cb_html(p, cb_pre_html_block);

    dbg_err_if(parser_run(p));

    parser_free(p);

    return 0;
err:
    if(p)
        parser_free(p);
    return ~0;
}

static int u_copy(const char *src, const char *dst)
{
    FILE *sfp = NULL, *dfp = NULL;
    size_t c;
    char buf[4096];

    sfp = fopen(src, "rb");
    crit_err_sifm(sfp == NULL, "unable to open %s for reading", src);

    dfp = fopen(dst, "wb+");
    crit_err_sifm(dfp == NULL, "unable to open %s for writing", dst);

    while((c = fread(buf, 1, sizeof(buf), sfp)) > 0)
    {
        crit_err_sifm(fwrite(buf, 1, c, dfp) == 0, "error writing to %s", dst);
    }

    crit_err_sif(fclose(sfp));

    crit_err_sifm(fclose(dfp), "error flushing %s", dst);

    return 0;
err:
    if(sfp)
        fclose(sfp);
    if(dfp)
        fclose(dfp);
    return ~0;
}

static int u_move(const char *src, const char *dst)
{
#ifdef HAVE_LINK
    int rc;

    crit_err_sif((rc = link(src, dst)) < 0 && errno != EXDEV);

    if(rc && errno == EXDEV)
        dbg_err_if(u_copy(src, dst)); /* deep copy */
#else
    dbg_err_if(u_copy(src, dst));
#endif

    unlink(src);

    return 0;
err:
    return ~0;
}

static int fix_line_decl(trans_info_t *pti)
{
    io_t *in = NULL, *tmp = NULL;
    char tname[U_FILENAME_MAX], buf[1024];
    int ln = 0;

    /* open the input file */
    con_err_ifm(u_file_open(pti->file_out, O_RDONLY, &in),
        "unable to open %s", pti->file_out);

    /* get a temporary io_t */
    con_err_if(u_tmpfile_open(&tmp));

    while(io_gets(in, buf, sizeof(buf)) > 0)
    {
        if(strstr(buf, "#line 0 __PG_FILE_C__") == NULL)
        {
            io_printf(tmp, "%s", buf);
            ln++; /* line number */
        } else
            io_printf(tmp, "#line %d \"%s\"", ln + 2, pti->file_out);
    }

    /* get the filename of the temporary io_t */
    dbg_err_if(io_name_get(tmp, tname, U_FILENAME_MAX));

    io_free(in), in = NULL;
    io_free(tmp), tmp = NULL;

    /* move tmp to file_out */
    unlink(pti->file_out);

    u_move(tname, pti->file_out);

    return 0;
err:
    if(in)
        io_free(in);
    if(tmp)
        io_free(tmp);
    return ~0;
}

int translate(trans_info_t *pti)
{
    io_t *in = NULL, *out = NULL, *tmp = NULL;
    codec_t *gzip = NULL, *aes = NULL;
    char tname[U_FILENAME_MAX];

    dbg_return_if (pti == NULL, ~0);
    
    /* open the input file */
    con_err_ifm(u_file_open(pti->file_in, O_RDONLY, &in),
        "unable to open %s", pti->file_in);

    /* open the output file */
    con_err_ifm(u_file_open(pti->file_out, O_CREAT | O_TRUNC | O_WRONLY, &out),
        "unable to open %s", pti->file_out);

    /* should choose the right translator based on file extensions or config */
    if(is_a_script(pti->file_in))
    {
        /* get a temporary io_t */
        con_err_if(u_tmpfile_open(&tmp));

        /* save the preprocessed in file to tmp */
        dbg_err_if(preprocess(in, tmp));

        /* reset the tmp io */
        io_seek(tmp, 0);

        /* translate it */
        dbg_err_if(translate_script_to_c(tmp, out, pti));

        /* get the filename of the temporary io_t */
        dbg_err_if(io_name_get(tmp, tname, U_FILENAME_MAX));

        /* free the tmp io */
        io_free(tmp);

        /* remove the tmp file */
        unlink(tname);
    } else {
        /* check if compression is requested */
#ifdef HAVE_LIBZ
        if(pti->comp)
        {
            /* set a compression filter to the input stream */
            dbg_err_if(codec_gzip_create(GZIP_COMPRESS, &gzip));
            dbg_err_if(io_codec_add_tail(in, gzip));
            gzip = NULL;
        }
#endif
#ifdef HAVE_LIBOPENSSL
        /* check if encryption is requested */
        if(pti->encrypt)
        {
            /* set a cipher filter */
            dbg_err_if(codec_cipher_create(CIPHER_ENCRYPT, EVP_aes_256_cbc(),
                pti->key, NULL, &aes));
            dbg_err_if(io_codec_add_tail(in, aes));
            aes = NULL;
        }
#endif
        dbg_err_if(translate_opaque_to_c(in, out, pti));
    }

    io_free(in), in = NULL;
    io_free(out), out = NULL;

    /* replace '#line 0 __PG_FILE_C__' lines with '#line N "real_filename.c"' */
    if(is_a_script(pti->file_in))
        dbg_err_if(fix_line_decl(pti));

    return 0;
err:
    if(pti && strlen(pti->emsg))
        con("%s", pti->emsg);
    if(gzip)
        codec_free(gzip);
    if(tmp)
        io_free(tmp);
    if(in)
        io_free(in);
    if(out)
        io_free(out);
    return ~0;
}
