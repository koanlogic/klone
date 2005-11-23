/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: trans_c.c,v 1.19 2005/11/23 18:07:14 tho Exp $
 */

#include "klone_conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <u/libu.h>
#include <klone/klone.h>
#include <klone/translat.h>
#include <klone/parser.h>
#include <klone/utils.h>
#include <klone/codecs.h>

struct code_block_s;

TAILQ_HEAD(code_block_list_s, code_block_s);
struct code_block_s
{
    TAILQ_ENTRY(code_block_s) np; /* next & prev pointers                   */
    char *buf;
    size_t sz;
    size_t code_line;
    const char *file_in;
};
typedef struct code_block_s code_block_t;
typedef struct code_block_list_s code_block_list_t;

typedef struct
{
    code_block_list_t code_blocks;
    trans_info_t *ti;
    size_t html_block_cnt;
} lang_c_ctx_t;

static void free_code_block(code_block_t *node)
{
    if(node)
    {
        if(node->buf)
        {
            U_FREE(node->buf);
            node->buf = NULL;
        }
        U_FREE(node);
    }
}

static void free_code_blocks(lang_c_ctx_t *ctx)
{
    code_block_list_t *head = &ctx->code_blocks;
    code_block_t *node;

    while((node = head->tqh_first) != NULL)
    {
        TAILQ_REMOVE(head, node, np);
        free_code_block(node);
    }
}


static int push_code_block(lang_c_ctx_t *ctx, parser_t *p, 
    const char *buf, size_t sz)
{
    code_block_t *node;
    
    node = (code_block_t*)u_zalloc(sizeof(code_block_t));
    dbg_err_if(node == NULL);

    node->sz = sz;
    node->buf = (char*)u_malloc(sz);
    dbg_err_if(node->buf == NULL);

    node->code_line = p->code_line;
    node->file_in = ctx->ti->file_in;

    memcpy(node->buf, buf, sz);

    TAILQ_INSERT_TAIL(&ctx->code_blocks, node, np);

    return 0;
err:
    if(node)
        free_code_block(node);
    return ~0;
}

static void print_header(parser_t *p, lang_c_ctx_t *ctx)
{
    const char *file;
    (void)ctx;

    io_printf(p->out, "#include <klone/emb.h>\n");

    file = ctx->ti->uri + strlen(ctx->ti->uri) - 1;

    for(; *file != '/' && file >= ctx->ti->uri; --file)
            ;
    io_printf(p->out, "static const char *SCRIPT_NAME = \"%s\";\n", 
                ++file);
}

static int print_var_definition(parser_t *p, int comp, const char* varname, 
        const char* buf, size_t bufsz)
{
    codec_t *zip = NULL;
    io_t *ios = NULL;
    int rc, i;
    unsigned char c;

    dbg_err_if(p == 0 || p->out == 0);

    /* create an io_t around the HTML block */
    dbg_err_if(io_mem_create(buf, bufsz, 0, &ios));

    #ifdef HAVE_LIBZ
    /* if compression is enabled zip the data block */
    if(comp)
    {
        /* apply a gzip codec */
        dbg_err_if(codec_gzip_create(GZIP_COMPRESS, &zip));
        dbg_err_if(io_codec_add_tail(ios, zip));
        zip = NULL; /* io_free() will free the codec */
    }
    #endif

    io_printf(p->out, "static uint8_t %s[] = {\n", varname);

    for(i = 1; (rc = io_getc(ios, &c)) > 0; ++i)
    {
        io_printf(p->out, "0x%02X, ", c);
        if(i % 12 == 0)
            io_printf(p->out, "\n");
    }
    dbg_err_if(rc < 0); /* input stream error */

    io_printf(p->out, "};\n");

    io_free(ios);

    return 0;
err:
    if(zip)
        codec_free(zip);
    if(ios)
        io_free(ios);
    return ~0;
}

static void print_code_blocks(parser_t *p, lang_c_ctx_t *ctx)
{
    code_block_t *node;
    code_block_list_t *head;

    io_printf(p->out, 
    "\n\n"
    "static void exec_page(request_t *request, response_t *response,        \n"
    "   session_t *session) {                                               \n"
    "    io_t *out = response_io(response);                                 \n"
    );

    head = &ctx->code_blocks;
    for(node = head->tqh_first; node != NULL; node = node->np.tqe_next)
        io_write(p->out, node->buf, node->sz);

    io_printf(p->out, 
            "klone_script_exit:       \n"
            "   return;             \n"
            "}                      \n"
            );
}

static void print_static_page_block(io_t *out, lang_c_ctx_t *ctx)
{
    io_printf(out, 
        "static embfile_t e;                \n"
        "static void res_ctor()             \n"
        "{                                  \n"
        "   e.res.type = ET_FILE;           \n"
        "   e.res.filename = \"%s\";        \n"
        "   e.data = data;                  \n"
        "   e.size = sizeof(data);          \n"
        "   e.file_size = %u;               \n"
        "   e.mime_type = \"%s\";           \n"
        "   e.mtime = %lu;                  \n"
        "   e.comp = %d;                    \n"
        "   e.encrypted = %d;               \n"
        "}                                  \n",
        ctx->ti->uri, 
         /* file_size will be == to size if the file is not compressed */
        ctx->ti->file_size, 
        u_guess_mime_type(ctx->ti->uri), 
        ctx->ti->mtime, 
        ctx->ti->comp,
        ctx->ti->encrypt);
}

static void print_dynamic_page_block(io_t *out, lang_c_ctx_t *ctx)
{
    io_printf(out, 
        "static embpage_t e;                \n"
        "static void res_ctor()             \n"
        "{                                  \n"
        "   e.res.type = ET_PAGE;           \n"
        "   e.res.filename = \"%s\";        \n"
        "   e.run = exec_page;              \n"
        "}                                  \n",
        ctx->ti->uri);
}

static void print_register_block(io_t *out, lang_c_ctx_t *ctx)
{
    char md5[MD5_DIGEST_BUFSZ];

    u_md5(ctx->ti->uri, strlen(ctx->ti->uri), md5);

    io_printf(out, 
        "void module_init_%s()              \n"
        "{                                  \n"
        "    res_ctor();                    \n"
        "    emb_register((embres_t*)&e);   \n"
        "}                                  \n"
        "void module_term_%s()              \n"
        "{                                  \n"
        "    emb_unregister((embres_t*)&e); \n"
        "}                                  \n",
        md5, md5);
}

static int print_c_line(parser_t *p, lang_c_ctx_t *ctx)
{
    dbg_err_if(io_printf(p->out, "#line %d \"%s\"\n", p->code_line, 
        ctx->ti->file_in) < 0);

    return 0;
err:
    return ~0;
}

static int process_declaration(parser_t* p, void *arg, const char* buf, 
    size_t sz)
{
    /* lang_c_ctx_t *ctx = (lang_c_ctx_t*)arg;

    dbg_err_if(print_c_line(p, ctx)); */
    u_unused_args(arg);

    dbg_err_if(io_write(p->out, buf, sz) < 0);

    return 0;
err:
    return ~0;
}

static int process_expression(parser_t* p, void *arg, const char*buf, size_t sz)
{
    lang_c_ctx_t *ctx = (lang_c_ctx_t*)arg;
    const char before[] = "io_printf(out, \"%s\",";
    const char after[] = ");";

    dbg_err_if(push_code_block(ctx, p, before, strlen(before)));
    dbg_err_if(push_code_block(ctx, p, buf, sz));
    dbg_err_if(push_code_block(ctx, p, after, strlen(after)));

    return 0;
err:
    return ~0;
}

static int process_code(parser_t* p, void *arg, const char* buf, size_t sz)
{
    lang_c_ctx_t *ctx = (lang_c_ctx_t*)arg;

    dbg_err_if(push_code_block(ctx, p, buf, sz));

    return 0;
err:
    return ~0;
}

static int translate_set_error(trans_info_t *ti, parser_t *p, const char *msg)
{
    char file[U_FILENAME_MAX];

    dbg_err_if(io_name_get(p->in, file, U_FILENAME_MAX));

    dbg_err_if(u_snprintf(ti->emsg, EMSG_BUFSZ, "[%s:%d] %s", 
        file, p->line, msg));

    return 0;
err:
    return ~0;
}

static int cb_html_block(parser_t* p, void *arg, const char* buf, size_t sz)
{
    enum { CODESZ = 128, VARNSZ = 32 };
    lang_c_ctx_t *ctx = (lang_c_ctx_t*)arg;
    char code[CODESZ];
    char varname[VARNSZ];

    if(ctx->ti->comp)
    {   /* zip embedded HTML blocks */
        dbg_err_if(u_snprintf(varname, VARNSZ, "klone_html_zblock_%lu", 
            ctx->html_block_cnt));

        dbg_err_if(print_var_definition(p, 1 /* zip it */, varname, buf, sz));

        dbg_err_if(u_snprintf(code, CODESZ, 
            "\ndbg_ifb(u_io_unzip_copy(out, klone_html_zblock_%lu, "
            "   sizeof(klone_html_zblock_%lu))) goto klone_script_exit;\n", 
            ctx->html_block_cnt, ctx->html_block_cnt));

    } else {
        /* embedded HTML blocks will not be zipped */
        dbg_err_if(u_snprintf(varname, VARNSZ, "klone_html_%lu", 
            ctx->html_block_cnt));

        dbg_err_if(print_var_definition(p, 0, varname, buf, sz));

        dbg_err_if(u_snprintf(code, CODESZ, 
            "\ndbg_ifb(io_write(out, klone_html_%lu, "
            "   sizeof(klone_html_%lu)) < 0) goto klone_script_exit;\n", 
            ctx->html_block_cnt, ctx->html_block_cnt));
    }

    dbg_err_if(push_code_block(ctx, p, code, strlen(code)));

    ctx->html_block_cnt++;

    return 0;
err:
    return ~0;
}


static int cb_code_block(parser_t* p, int cmd, void *arg, const char* buf, 
    size_t sz)
{
    lang_c_ctx_t *ctx = (lang_c_ctx_t *)arg;
    //trans_info_t *ti = ctx->ti;

	switch(cmd)
	{
	case 0: /* plain code block <% ... %> */
        process_code(p, arg, buf, sz);
		break;
	case '@': /* <%@ ... %> */
		dbg_err_if("the file should have already been preprocessed");
		break;
	case '!': /* <%! ... %> */
        process_declaration(p, arg, buf, sz);
		break;
	case '=': /* <%= ... %> */
        process_expression(p, arg, buf, sz);
		break;
	default:
        translate_set_error(ctx->ti, p, "bad command char after <%");
		warn_err("unknown code type");
	}
    return 0;
err:
    return ~0;
}

/* translate a opaque file to a C unsigned char array */
int translate_opaque_to_c(io_t *in, io_t *out, trans_info_t *ti)
{
    lang_c_ctx_t ctx;
    int i = 0;
    ssize_t rc;
    unsigned char c;

    memset(&ctx, 0, sizeof(lang_c_ctx_t));
    TAILQ_INIT(&ctx.code_blocks);
    ctx.ti = ti;

    io_printf(out, "#include <klone/emb.h>\n");

    io_printf(out, "static uint8_t data[] = {\n");

    for(i = 1; (rc = io_getc(in, &c)) > 0; ++i)
    {
        io_printf(out, "0x%02X, ", c);
        if(i % 12 == 0)
            io_printf(out, "\n");
    }
    dbg_err_if(rc < 0); /* input stream error */

    io_printf(out, "};\n");

    print_static_page_block(out, &ctx);
    print_register_block(out, &ctx);

    return 0;
err:
    return ~0;
}

int translate_script_to_c(io_t *in, io_t *out, trans_info_t *ti)
{
    parser_t *p = NULL;
    lang_c_ctx_t ctx;

    /* init the context obj */
    memset(&ctx, 0, sizeof(lang_c_ctx_t));
    TAILQ_INIT(&ctx.code_blocks);
    ctx.ti = ti;

    /* create a parse that reads from in and writes to out */
    dbg_err_if(parser_create(&p));

    parser_set_io(p, in, out);

    parser_set_cb_arg(p, &ctx);
    parser_set_cb_code(p, cb_code_block);
    parser_set_cb_html(p, cb_html_block);

    print_header(p, &ctx);

    dbg_err_if(parser_run(p));

    print_code_blocks(p, &ctx);

    print_dynamic_page_block(p->out, &ctx);

    print_register_block(p->out, &ctx);

    free_code_blocks(&ctx);

    parser_free(p);

    return 0;
err:
    free_code_blocks(&ctx);
    if(p)
        parser_free(p);
    return ~0;
}
