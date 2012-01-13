/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: trans_c.c,v 1.43 2010/05/31 13:41:12 tho Exp $
 */

#include "klone_conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <u/libu.h>
#include <klone/os.h>
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

struct lang_c_ctx_s
{
    code_block_list_t code_blocks;
    trans_info_t *ti;
    size_t html_block_cnt;
};

typedef struct lang_c_ctx_s lang_c_ctx_t;

static const char copyright_hdr[] =
    "/*                                                                     \n"
    " * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://koanlogic.com>  \n"
    " * All rights reserved.                                                \n"
    " *                                                                     \n"
    " * This file is part of KLone, and as such it is subject to the license\n"
    " * stated in the LICENSE file which you have received as part of this  \n"
    " * distribution                                                        \n"
    " */                                                                    \n";

static void free_code_block(code_block_t *node)
{
    if(node)
    {
        U_FREE(node->buf);
        U_FREE(node);
    }
}

static void free_code_blocks(lang_c_ctx_t *ctx)
{
    code_block_t *node;
    code_block_list_t *head;

    dbg_ifb (ctx == NULL) return;

    head = &ctx->code_blocks;
    
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
    
    dbg_return_if (p == NULL, ~0);
    dbg_return_if (ctx == NULL, ~0);
 
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
    static const char dfun_prefix[] = "page_";
    const char *file;
    char *dfun;
    int i;

    dbg_ifb (p == NULL) return;
    dbg_ifb (ctx == NULL) return;

    (void)ctx;

    io_printf(p->out, "%s", copyright_hdr);
    io_printf(p->out, "#include <klone/emb.h>\n");
    io_printf(p->out, "#include <klone/dypage.h>\n");

    file = ctx->ti->uri + strlen(ctx->ti->uri) - 1;

    for(; *file != '/' && file >= ctx->ti->uri; --file)
            ;

    io_printf(p->out, "static const char *SCRIPT_NAME = \"%s\";\n", 
                ++file);

    dfun = ctx->ti->dfun; /* shortcut */

    /* we need a prefix to avoid errors with pages whose filename starts with
       a number (404-handler.kl1) or any other char not allowed by C as the
       first char of function names */
    (void) u_strlcpy(dfun, dfun_prefix, URI_BUFSZ);
    dbg_if (u_strlcat(dfun, file, URI_BUFSZ));

    for(i = 0; i < (int) strlen(dfun); ++i)
        if(!isalnum(dfun[i]))
            dfun[i] = '_'; /* just a-zA-Z0-9 allowed */

    /* add a dummy function that the user can use to set a breakpoint when
       entering the page code. the "static volatile" variable is used to avoid
       the compiler to optimize-out (inlining) the function call */
    io_printf(p->out, 
            "static int %s (void) { "
            "static volatile int dummy; return dummy; }\n", dfun);

    io_printf(p->out, 
        "static request_t *request = NULL;\n"
        "static response_t *response = NULL;\n"
        "static session_t *session = NULL;\n"
        "static io_t *in = NULL;\n"
        "static io_t *out = NULL;\n");
 
    return;
}

static int print_var_definition(parser_t *p, int comp, const char *varname, 
        const char *buf, size_t bufsz)
{
    codec_t *zip = NULL;
    io_t *ios = NULL;
    int rc, i;
    unsigned char c;

    dbg_err_if(p == NULL);
    dbg_err_if(varname == NULL);
    dbg_err_if(buf == NULL);

    /* create an io_t around the HTML block */
    dbg_err_if(io_mem_create((char*)buf, bufsz, 0, &ios));

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

    io_printf(p->out, "static const char %s[] = {\n", varname);

    for(i = 1; (rc = io_getc(ios, (char*)&c)) > 0; ++i)
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

    dbg_ifb (p == NULL) return;
    dbg_ifb (ctx == NULL) return;

    io_printf(p->out, 
        "\n\n"
        "static void exec_page(dypage_args_t *_dyp_args)                    \n"
        "{                                                                  \n"
        "   request = _dyp_args->rq;                                        \n"
        "   response = _dyp_args->rs;                                       \n"
        "   session = _dyp_args->ss;                                        \n"
        "   in = request_io(request);                                       \n"
        "   out = response_io(response);                                    \n"
        "   u_unused_args(SCRIPT_NAME, request, response, session, in, out);\n"
        "   %s () ; \n ", ctx->ti->dfun
        );

    head = &ctx->code_blocks;
    for(node = head->tqh_first; node != NULL; node = node->np.tqe_next)
    {
        io_printf(p->out, "\n");
        io_write(p->out, node->buf, node->sz);
    }

    io_printf(p->out, 
            "goto klone_script_exit;\n" /* just to avoid a warning */
            "klone_script_exit:     \n"
            "   return;             \n"
            "}                      \n"
            );
}

static void print_static_page_block(io_t *out, lang_c_ctx_t *ctx)
{
    dbg_ifb (out == NULL) return;
    dbg_ifb (ctx == NULL) return;
    dbg_ifb (ctx->ti == NULL) return;
 
    io_printf(out, 
        "static embfile_t e;                \n"
        "static void res_ctor(void)         \n"
        "{                                  \n"
        "   e.res.type = ET_FILE;           \n"
        "   e.res.filename = \"%s\";        \n"
        "   e.data = (unsigned char*)data;  \n"
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
        (unsigned long) ctx->ti->mtime,
        ctx->ti->comp,
        ctx->ti->encrypt);
}

static void print_dynamic_page_block(io_t *out, lang_c_ctx_t *ctx)
{
    dbg_ifb (out == NULL) return;
    dbg_ifb (ctx == NULL) return;
    dbg_ifb (ctx->ti == NULL) return;

    io_printf(out, 
        "static embpage_t e;                \n"
        "static void res_ctor(void)         \n"
        "{                                  \n"
        "   e.res.type = ET_PAGE;           \n"
        "   e.res.filename = \"%s\";        \n"
        "   e.fun = exec_page;              \n"
        "}                                  \n",
        ctx->ti->uri);
}

static void print_register_block(io_t *out, lang_c_ctx_t *ctx)
{
    char md5[MD5_DIGEST_BUFSZ];

    dbg_ifb (out == NULL) return;
    dbg_ifb (ctx == NULL) return;
    dbg_ifb (ctx->ti == NULL) return;

    u_md5(ctx->ti->uri, strlen(ctx->ti->uri), md5);

    io_printf(out, 
        "#ifdef __cplusplus                 \n"
        "extern \"C\" {                     \n"
        "#endif                             \n"
        "void module_init_%s(void);         \n" /* avoids a warning */
        "void module_init_%s(void)          \n"
        "{                                  \n"
        "    res_ctor();                    \n"
        "    emb_register((embres_t*)&e);   \n"
        "}                                  \n"
        "void module_term_%s(void);         \n" /* avoids a warning */
        "void module_term_%s(void)          \n"
        "{                                  \n"
        "    emb_unregister((embres_t*)&e); \n"
        "}                                  \n"
        "#ifdef __cplusplus                 \n"
        "}                                  \n"
        "#endif                             \n",
        md5, md5, md5, md5);
}

static int process_declaration(parser_t *p, void *arg, const char *buf, 
        size_t sz)
{
    u_unused_args(arg);

    dbg_err_if (p == NULL);

    dbg_err_if(io_write(p->out, buf, sz) < 0);

    /* a newline is required after #includes or #defines */
    dbg_err_if(io_printf(p->out, "\n") < 0);

    return 0;
err:
    return ~0;
}

static int process_expression(parser_t *p, void *arg, const char *buf, 
        size_t sz)
{
    lang_c_ctx_t *ctx;
    const char before[] = "io_printf(out, \"%s\",";
    const char after[] = ");\n";

    dbg_err_if (p == NULL);
    dbg_err_if (arg == NULL);

    ctx = (lang_c_ctx_t*)arg;
    
    dbg_err_if(push_code_block(ctx, p, before, strlen(before)));
    dbg_err_if(push_code_block(ctx, p, buf, sz));
    dbg_err_if(push_code_block(ctx, p, after, strlen(after)));

    return 0;
err:
    return ~0;
}

static int process_code(parser_t *p, void *arg, const char *buf, size_t sz)
{
    lang_c_ctx_t *ctx;

    dbg_err_if (p == NULL);
    dbg_err_if (arg == NULL);

    ctx = (lang_c_ctx_t*)arg;
 
    dbg_err_if(push_code_block(ctx, p, buf, sz));

    return 0;
err:
    return ~0;
}

static int translate_set_error(trans_info_t *ti, parser_t *p, const char *msg)
{
    char file[U_FILENAME_MAX];

    dbg_err_if (ti == NULL);
    dbg_err_if (p == NULL || p->in == NULL);
 
    dbg_err_if(io_name_get(p->in, file, U_FILENAME_MAX));

    dbg_err_if(u_snprintf(ti->emsg, EMSG_BUFSZ, "[%s:%d] %s", 
        file, p->line, msg));

    return 0;
err:
    return ~0;
}

static int cb_html_block(parser_t *p, void *arg, const char *buf, size_t sz)
{
    enum { CODESZ = 128, VARNSZ = 32 };
    lang_c_ctx_t *ctx;
    char code[CODESZ];
    char varname[VARNSZ];

    dbg_err_if (p == NULL);
    dbg_err_if (arg == NULL);

    ctx = (lang_c_ctx_t*)arg;

    if(ctx->ti->comp)
    {   /* zip embedded HTML blocks */
        dbg_err_if(u_snprintf(varname, VARNSZ, "klone_html_zblock_%lu", 
            (unsigned long) ctx->html_block_cnt));

        dbg_err_if(print_var_definition(p, 1 /* zip it */, varname, buf, sz));

        dbg_err_if(u_snprintf(code, CODESZ, 
            "\ndbg_if(u_io_unzip_copy(out, klone_html_zblock_%lu, "
            "   sizeof(klone_html_zblock_%lu)));\n", 
            (unsigned long) ctx->html_block_cnt, 
            (unsigned long) ctx->html_block_cnt));

    } else {
        /* embedded HTML blocks will not be zipped */
        dbg_err_if(u_snprintf(varname, VARNSZ, "klone_html_%lu", 
            (unsigned long) ctx->html_block_cnt));

        dbg_err_if(print_var_definition(p, 0, varname, buf, sz));

        dbg_err_if(u_snprintf(code, CODESZ, 
            "\ndbg_if(io_write(out, klone_html_%lu, "
            "   sizeof(klone_html_%lu)) < 0);\n", 
            (unsigned long) ctx->html_block_cnt, 
            (unsigned long) ctx->html_block_cnt));
    }

    dbg_err_if(push_code_block(ctx, p, code, strlen(code)));

    ctx->html_block_cnt++;

    return 0;
err:
    return ~0;
}

static int cb_code_block(parser_t *p, int cmd, void *arg, const char *buf, 
        size_t sz)
{
    lang_c_ctx_t *ctx;

    dbg_err_if (p == NULL);
    dbg_err_if (arg == NULL);

    ctx = (lang_c_ctx_t *)arg;

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

/* translate a opaque file to a const char array */
int translate_opaque_to_c(io_t *in, io_t *out, trans_info_t *ti)
{
    lang_c_ctx_t ctx;
    int i = 0;
    ssize_t rc;
    unsigned char c;

    dbg_err_if (in == NULL);
    dbg_err_if (out == NULL);
    dbg_err_if (ti == NULL);
    
    memset(&ctx, 0, sizeof(lang_c_ctx_t));
    TAILQ_INIT(&ctx.code_blocks);
    ctx.ti = ti;

    io_printf(out, "%s", copyright_hdr);
    io_printf(out, "#include <klone/emb.h>\n");

    io_printf(out, "static const char data[] = {\n");

    for(i = 1; (rc = io_getc(in, (char*)&c)) > 0; ++i)
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

    dbg_return_if (in == NULL, ~0);
    dbg_return_if (out == NULL, ~0);
    dbg_return_if (ti == NULL, ~0);
    
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
