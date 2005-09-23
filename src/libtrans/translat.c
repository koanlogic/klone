#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <klone/klone.h>
#include <klone/translat.h>
#include <klone/parser.h>
#include <klone/utils.h>
#include <klone/os.h>
#include <klone/io.h>
#include <klone/codec.h>
#include <klone/codgzip.h>

int preprocess(io_t *in, io_t *out);

static int is_a_script(const char *filename)
{
    const char *script_ext = ".klone";
    const char *fn, *ext;

    if(!strlen(filename))
        return 0;

    fn = filename + strlen(filename) - 1;
    ext = script_ext + strlen(script_ext) - 1;
    for( ; ext >= script_ext; --fn, --ext)
    {
        if(tolower(*fn) != *ext)
            return 0;
    }
    return 1;
}

static int process_directive_include(parser_t* p, char *inc_file)
{
    enum { BUFSZ = 4096 };
    char buf[BUFSZ], *pc;
    io_t *io = NULL;
    size_t c;

    dbg_err_if(io_get_name(p->in, buf, BUFSZ));

    /* remove file name, just path is needed */
    dbg_err_if((pc = strrchr(buf, '/')) == NULL);
    ++pc; *pc = 0;

    dbg_err_if(strlen(buf) + strlen(inc_file) >= BUFSZ);

    strcat(buf, inc_file);

    /* copy include file to p->out */
    dbg_err_if(u_file_open(buf, O_RDONLY, &io));

    preprocess(io, p->out);
    /*
    while( (c = io_read(io, buf, BUFSZ)) > 0)
        dbg_err_if(io_write(p->out, buf, c) < 0);
    */

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int process_directive(parser_t* p, char* buf)
{
    char *tok, *pp;

    /* get preprocessor command */
    dbg_err_if((tok = strtok_r(buf, " \t", &pp)) == NULL);

    if(strcasecmp(tok, "include") == 0)
    {
        /* get include file name */
        dbg_err_if((tok = strtok_r(NULL, " \t\"", &pp)) == NULL);

        dbg_err_if(process_directive_include(p, tok));
    } else {
        dbg("unknown preprocessor directive: %s", tok);
        goto err; 
    }

    return 0;
err:
    return ~0;
}

static int parse_directive(parser_t* p, void *arg, const char* buf, size_t sz)
{
    enum { LINE_BUFSZ = 1024 };
    char line[LINE_BUFSZ];
    io_t *io = NULL;

    dbg_err_if(io_mem_create(buf, sz, 0, &io));

    while(io_gets(io, line, LINE_BUFSZ) > 0)
        dbg_err_if(process_directive(p, line));

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int cb_pre_html_block(parser_t* p, void *arg, const char* buf, size_t sz)
{
    io_write(p->out, buf, sz);
    
    return 0;
}

static int cb_pre_code_block(parser_t* p, int cmd, void *arg, const char* buf, 
    size_t sz)
{
    if(cmd == '@')
    { /* do preprocess */
        dbg_err_if(parse_directive(p, arg, buf, sz));
    } else {
        io_printf(p->out, "<%%%c ", (cmd == 0 ? ' ' : cmd));
        io_write(p->out, buf, sz);
        io_printf(p->out, "%%>");
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

    //parser_set_cb_arg(p, NULL);
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

int translate(trans_info_t *pti)
{
    io_t *in = NULL, *out = NULL, *tmp = NULL;
    codec_gzip_t *gzip = NULL;

    /* open the input file */
    dbg_err_if(u_file_open(pti->file_in, O_RDONLY, &in));

    /* open the output file */
    dbg_err_if(u_file_open(pti->file_out, O_CREAT | O_TRUNC | O_WRONLY, &out));

    /* should choose the right translator based on file extensions or config */
    if(is_a_script(pti->file_in))
    {
        /* get a temporary io_t */
        dbg_err_if(u_tmpfile_open(&tmp));

        /* save the preprocessed in file to tmp */
        dbg_err_if(preprocess(in, tmp));

        /* reset the tmp io */
        io_seek(tmp, 0);

        /* translate it */
        dbg_err_if(translate_script_to_c(tmp, out, pti));

        /* free the tmp io and remove the tmp file */
        char tname[PATH_MAX];

        /* get the filename of the temporary io_t */
        dbg_err_if(io_get_name(tmp, tname, PATH_MAX));

        io_free(tmp);

        /* remove the tmp file */
        unlink(tname);
    } else  {
        /* check if compression is requested */
        if(pti->comp)
        {
            /* set a compression filter to the input stream */
            dbg_err_if(codec_gzip_create(GZIP_COMPRESS, &gzip));
            dbg_err_if(io_set_codec(in, (codec_t*)gzip));
            gzip = NULL;
        }
        dbg_err_if(translate_opaque_to_c(in, out, pti));
    }

    io_free(in);
    io_free(out);

    return 0;
err:
    if(gzip)
        codec_free((codec_t*)gzip);
    if(tmp)
        io_free(tmp);
    if(in)
        io_free(in);
    if(out)
        io_free(out);
    return ~0;
}

