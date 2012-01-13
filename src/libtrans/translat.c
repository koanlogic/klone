/* 
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: translat.c,v 1.32 2009/05/29 10:26:01 tho Exp $
 */

#include "klone_conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
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
    do { if( (expr) ) { con_p_ctx(p); u_con("%s", #expr); goto err; } } while(0)
#define tr_err_ifm(expr, ...)    \
    do { if( (expr) ) { con_p_ctx(p); u_con(__VA_ARGS__); goto err; } } while(0)


typedef struct kblock_s
{
    char *name;                 /* block name (is NULL for unnamed blocks) */
    u_buf_t *ubuf;              /* block content */
    struct kblock_s *parent;     /* the parent block in the block tree */
    struct kblock_s *ancestor;   /* the ancestor block this block overrides */
    u_list_t *children;         /* list of children blocks */
} kblock_t;

typedef struct ppctx_s
{
    kblock_t *cur;       /* currently selected block */
    kblock_t *top;       /* top block in the tree */
    kblock_t *dead;      /* tree for dead blocks */
    int lev;            /* blocks nesting level */
    int extending;      /* nesting level from the extended block */
    u_list_t *depend;   /* list of depends (path+filename) */
} ppctx_t;

static int preprocess(io_t *in, io_t *out, ppctx_t*);

static int kblock_free(kblock_t *block)
{
    kblock_t *child;
    int i;

    dbg_err_if(block == NULL);

    /* free children first */
    for(i = 0; (child = u_list_get_n(block->children, i)) != NULL; ++i)
        kblock_free(child);

    if(block->ancestor)
        kblock_free(block->ancestor);

    if(block->name)
        u_free(block->name);

    if(block->ubuf)
        u_buf_free(block->ubuf);

    u_free(block);

    return 0;
err:
    return ~0;
}

static int kblock_push_child(kblock_t *parent, kblock_t *child)
{
    dbg_err_if(u_list_add(parent->children, child));

    child->parent = parent;

    return 0;
err:
    return ~0;
}

static int kblock_create(const char *name, const char *buf, size_t size, 
            kblock_t **pblock)
{
    kblock_t *b = NULL;

    b = u_zalloc(sizeof(kblock_t));
    dbg_err_if(b == NULL);

    dbg_err_if(u_buf_create(&b->ubuf));

    dbg_err_if(u_list_create(&b->children));

    if(name)
    {
        b->name = u_strdup(name);
        dbg_err_if(b->name == NULL);
    }

    if(buf && size)
        dbg_err_if(u_buf_append(b->ubuf, buf, size));

    *pblock = b;

    return 0;
err:
    if(b)
        kblock_free(b);
    return ~0;
}

static int ppctx_print_kblock(kblock_t *block, io_t *out)
{
    kblock_t *b;
    size_t i;

    if(block->name == NULL)
    {
        if(u_buf_len(block->ubuf))
            dbg_err_if(io_write(out, u_buf_ptr(block->ubuf), 
                        u_buf_len(block->ubuf)) < 0);
    } else {
        /* named block */
        for(i = 0; (b = u_list_get_n(block->children, i)) != NULL; ++i)
            dbg_err_if(ppctx_print_kblock(b, out));
    }

    return 0;
err:
    return ~0;
}


static int ppctx_depend_push(ppctx_t *ppc, const char *filepath)
{
    const char *s;

    dbg_err_if((s = u_strdup(filepath)) == NULL);

    dbg_err_if(u_list_add(ppc->depend, (void*)s));

    return 0;
err:
    return ~0;
}

static int ppctx_print_depend(ppctx_t *ppc, io_t *in, io_t *out, io_t *depend)
{
    char infile[U_FILENAME_MAX], outfile[U_FILENAME_MAX];
    char fullpath[U_FILENAME_MAX];
    const char *dfile;
    size_t i;

    dbg_err_if(ppc == NULL);
    dbg_err_if(in == NULL);
    dbg_err_if(out == NULL);
    dbg_err_if(depend == NULL);

    /* print make-style depends list:
       srcfile: depfile0 depfile1 ... depfileN */

    dbg_err_if(io_name_get(in, infile, U_FILENAME_MAX));
    dbg_err_if(io_name_get(out, outfile, U_FILENAME_MAX));

    /* input filename (makefile-style) */
    dbg_err_if(translate_makefile_filepath(infile, "$(srcdir)", fullpath, 
        sizeof(fullpath)));

    io_printf(depend, "%s %s.kld: %s ", outfile, outfile, fullpath);

    for(i = 0; (dfile = u_list_get_n(ppc->depend, i)) != NULL; ++i)
    {
        dbg_err_if(translate_makefile_filepath(dfile, "$(srcdir)", fullpath, 
            sizeof(fullpath)));
        io_printf(depend, "%s ", fullpath);
    }
    io_printf(depend, "\n");

    return 0;
err:
    return ~0;

}

static int ppctx_print(ppctx_t *ppc, io_t *out)
{
    kblock_t *b;
    size_t i;

    dbg_err_if(ppc == NULL);
    dbg_err_if(out == NULL);

    for(i = 0; (b = u_list_get_n(ppc->top->children, i)) != NULL; ++i)
        dbg_err_if(ppctx_print_kblock(b, out));

    return 0;
err:
    return ~0;

}

static int ppctx_free(ppctx_t *ppc)
{
    char *str;
    size_t i;

    if(ppc->top)
        kblock_free(ppc->top);

    if(ppc->dead)
        kblock_free(ppc->dead);

    if(ppc->depend)
    {
        for(i = 0; (str = u_list_get_n(ppc->depend, i)) != NULL; ++i)
            u_free(str);

        u_list_free(ppc->depend);
    }

    u_free(ppc);

    return 0;
}

static int ppctx_create(ppctx_t **pppc)
{
    ppctx_t *ppc = NULL;

    ppc = u_zalloc(sizeof(ppctx_t));
    dbg_err_if(ppc == NULL);

    dbg_err_if(kblock_create(NULL, NULL, 0, &ppc->top));
    dbg_err_if(kblock_create(NULL, NULL, 0, &ppc->dead));

    dbg_err_if(u_list_create(&ppc->depend));

    ppc->cur = ppc->top;
    ppc->dead->parent = ppc->top;

    *pppc = ppc;

    return 0;
err:
    if(ppc)
        ppctx_free(ppc);
    return ~0;
}

/* print parser context to the console */
static void con_p_ctx(parser_t *p)
{
    char fn[U_FILENAME_MAX];

    dbg_err_if(io_name_get(p->in, fn, U_FILENAME_MAX));

    /* con_ macro should be used here; we'd need a con_no_newline(...) */
    fprintf(stderr, "[%s:%d]: error: ", fn, p->code_line);

err:
    return;
}

int translate_makefile_filepath(const char *filepath, const char *prefix, 
    char *buf, size_t size)
{
    char file_in[U_FILENAME_MAX];

    /* input file */
    if(filepath[0] == '/' || filepath[0] == '\\')
    {   /* absolute path */
        dbg_err_if(u_snprintf(file_in, U_FILENAME_MAX, "%s", filepath));
    } else if(isalpha(filepath[0]) && filepath[1] == ':') {
        /* absolute path Windows (X:/....) */
        dbg_err_if(u_snprintf(file_in, U_FILENAME_MAX, "%s", filepath));
    } else {
        /* relative path, use $(srcdir) */
        dbg_err_if(u_snprintf(file_in, U_FILENAME_MAX, "%s/%s", prefix, 
            filepath ));
    }

    dbg_err_if(u_strlcpy(buf, file_in, size));

    return 0;
err:
    return ~0;
}

int translate_is_a_script(const char *filename)
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

static int include_file_full_path(parser_t *p, char *inc_file, char *obuf, 
    size_t size)
{
    char buf[U_FILENAME_MAX], *pc;

    /* get the name of the input file (that include filename path is relative
     * to the one of the input file) */
    dbg_err_if(io_name_get(p->in, buf, U_FILENAME_MAX));

    /* remove file name, just path is needed */
    dbg_err_if((pc = strrchr(buf, '/')) == NULL);
    ++pc; *pc = 0;

    dbg_err_if(u_strlcat(buf, inc_file, U_FILENAME_MAX));

    dbg_err_if(u_strlcpy(obuf, buf, size));

    return 0;
err:
    return ~0;
}

static int process_directive_include(parser_t *p, char *inc_file)
{
    ppctx_t *ppc;
    char fullpath[U_FILENAME_MAX], file[U_FILENAME_MAX];
    io_t *io = NULL;

    dbg_return_if (p == NULL, ~0);
    dbg_return_if (inc_file == NULL, ~0);

    dbg_err_if(io_name_get(p->in, file, U_FILENAME_MAX));

    dbg_err_if(include_file_full_path(p, inc_file, fullpath, U_FILENAME_MAX));

    /* copy include file to p->out */
    tr_err_ifm(u_file_open(fullpath, O_RDONLY, &io), 
        "unable to open included file %s", fullpath);

    dbg_err_if(io_printf(p->out, "<%% #line 1 \"%s\" \n %%>", fullpath));

    /* get the current preprocessor context */
    ppc = parser_get_cb_arg(p);
    dbg_err_if(preprocess(io, p->out, ppc));

    dbg_err_if(io_printf(p->out, "<%% #line %d \"%s\" \n %%>", 
        p->code_line, file));

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int kblock_get_by_name(kblock_t *parent, const char *name, 
    kblock_t **pfound, size_t *pidx)
{
    kblock_t *b;
    size_t i;

    dbg_err_if(parent == NULL);

    for(i = 0; (b = u_list_get_n(parent->children, i)) != NULL; ++i)
    {
        if(b->name && strcasecmp(b->name, name) == 0)
        {
            *pfound = b;
            *pidx = i;
            return 0;
        }
        if(kblock_get_by_name(b, name, pfound, pidx) == 0)
            return 0;
    }
        
err:
    return ~0;
}

static int process_directive_block(parser_t *p, const char *name)
{
    kblock_t *block = NULL, *old;
    ppctx_t *ppc;
    char file[U_FILENAME_MAX];
    size_t idx;

    dbg_err_if((ppc = parser_get_cb_arg(p)) == NULL);

    /* create a named block */
    dbg_err_if(kblock_create(name, NULL, 0, &block));

    dbg_err_if(io_name_get(p->in, file, U_FILENAME_MAX));

    ppc->lev++;

    if(!ppc->extending)
    {
        con_err_ifm(!kblock_get_by_name(ppc->top, name, &old, &idx),
            "[%s:%d] error: block named %s already exists", file, p->code_line,
            name);

        /* new block -> append to the block list */
        dbg_err_if(kblock_push_child(ppc->cur, block));

        ppc->cur = block;

        return 0;
    }

    ppc->extending++; /* increase the nesting level */

    /* we're extending a base template... */
     
    if(kblock_get_by_name(ppc->top, name, &old, &idx))
    {
        if(ppc->cur == ppc->top)
        {
            /* top-level block */
            u_con("[%s:%d] warning: extending a block (%s) that does not exist "
                "in the base templates, ignoring", file, p->code_line, name);

            /* attach it to a dead tree (i.e. a tree that doesn't get printed)*/
            dbg_err_if(kblock_push_child(ppc->dead, block));

        } else {

            if(ppc->lev)
            {
                /* nested block */
                dbg_err_if(kblock_push_child(ppc->cur, block));
            }
        }

    } else {
        /* overriding block */

        /* block with the same name as been found at position 'idx' of
           its parent's list; replace the old block with the new one
           and save the old in new->ancestor (we may need it for
           <% block inherit %>) */

        /* save the old block ptr */
        block->ancestor = old;
        block->parent = old->parent;

        /* insert the new block in the position of the old block */
        dbg_err_if(u_list_insert(old->parent->children, block, idx));

        /* remove the old block from the list */
        dbg_err_if(u_list_del(old->parent->children, old));
    }

    ppc->cur = block;

    return 0;
err:
    if(block)
        kblock_free(block);
    return ~0;
}

static int process_directive_endblock(parser_t *p, const char *name)
{
    ppctx_t *ppc;

    dbg_err_if((ppc = parser_get_cb_arg(p)) == NULL);

    dbg_err_if(ppc->cur == NULL);

    tr_err_ifm(--ppc->lev < 0, "unbalanced endblock directive");

    /* if name is provided check that it's correct */
    if(name)
        tr_err_ifm(ppc->cur->name == NULL || strcasecmp(name, ppc->cur->name),
            "endblock name is not correct (\"%s\" used when closing to "
            "\"%s\" block)", name, ppc->cur->name);

    ppc->cur = ppc->cur->parent; /* may be NULL */

    if(ppc->extending)
    {
        /* decrease the nesting level */
        if(--ppc->extending == 1)
            ppc->cur = ppc->top;
    }

    return 0;
err:
    return ~0;
}

static int process_directive_inherit(parser_t *p)
{
    ppctx_t *ppc;
    kblock_t *anc, *b;
    int i;

    dbg_err_if((ppc = parser_get_cb_arg(p)) == NULL);

    dbg_err_if(ppc->cur == NULL);

    anc = ppc->cur->ancestor;
    tr_err_ifm(anc == NULL, "inherit directive used in a block "
        "(\"%s\") that is not extending another block", ppc->cur->name);

    /* move all ancestor children to the current block (that's the block whom
     * is extending the ancestor */
    for(i = 0; (b = u_list_get_n(anc->children, i)) != NULL; ++i)
        dbg_err_if(kblock_push_child(ppc->cur, b));

    /* remove all elems */
    while(u_list_count(anc->children))
        dbg_err_if(u_list_del_n(anc->children, 0, NULL));

    return 0;
err:
    return ~0;
}

static int process_directive(parser_t *p, char *buf)
{
    ppctx_t *ppc;
    char *tok, *pp;
    char fullpath[U_FILENAME_MAX];

    dbg_return_if (p == NULL, ~0);
    dbg_return_if (buf == NULL, ~0);

    /* preprocessor context */
    dbg_err_if((ppc = parser_get_cb_arg(p)) == NULL);

    /* get preprocessor command */
    tr_err_ifm((tok = strtok_r(buf, " \t", &pp)) == NULL,
        "bad or missing preprocessor command");

    if(strcasecmp(tok, "include") == 0)
    {
        /* get include file name */
        tr_err_ifm((tok = strtok_r(NULL, " \t\"", &pp)) == NULL,
            "bad or missing include filename");

        /* calc the full path of the included file and add it to the deps list*/
        dbg_err_if(include_file_full_path(p, tok, fullpath, U_FILENAME_MAX));

        dbg_err_if(ppctx_depend_push(ppc, fullpath));

        dbg_err_if(process_directive_include(p, tok));

    } else if(strcasecmp(tok, "extends") == 0) { 

        /* get base file name */
        tr_err_ifm((tok = strtok_r(NULL, " \t\"", &pp)) == NULL,
            "bad or missing 'extends' filename");

        tr_err_ifm(p->code_line > 1, "child templates must start with the "
            "'<%@ extends \"FILE\" %>' directive (first line) %d");

        /* calc the full path of the included file and add it to the deps list*/
        dbg_err_if(include_file_full_path(p, tok, fullpath, U_FILENAME_MAX));

        dbg_err_if(ppctx_depend_push(ppc, fullpath));

        /* no difference with include? */
        dbg_err_if(process_directive_include(p, tok));

        con_err_ifm(ppc->lev, "[%s] error: unclosed block \"%s\"", tok, 
            ppc->cur->name);

        ppc->extending++;

    } else if(strcasecmp(tok, "block") == 0) { 

        /* get block name */
        tr_err_ifm((tok = strtok_r(NULL, " \t\"", &pp)) == NULL,
            "bad or missing 'block' name");

        dbg_err_if(process_directive_block(p, tok));

    } else if(strcasecmp(tok, "endblock") == 0) { 

        /* try get the block name (it's not mandatory) */
        tok = strtok_r(NULL, " \t\"", &pp);

        dbg_err_if(process_directive_endblock(p, tok));

    } else if(strcasecmp(tok, "inherit") == 0) { 

        dbg_err_if(process_directive_inherit(p));

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
    ppctx_t *ppc = (ppctx_t*)arg;
    kblock_t *block = NULL;
    char file[U_FILENAME_MAX];
    int ln;
    size_t i;

    u_unused_args(arg);

    dbg_err_if (p == NULL);
    dbg_err_if (ppc == NULL);

    /* create and append un unnamed */
    dbg_err_if(kblock_create(NULL, buf, sz, &block));

    if(ppc->cur == ppc->top && ppc->extending)
    {
        for(ln = p->code_line, i = 0; i < sz; ++i)
        {
            if(buf[i] == '\n')
                ln++; /* find out the line of the first not-blank char */
            if(!isspace(buf[i]))
            {
                dbg_err_if(io_name_get(p->in, file, U_FILENAME_MAX));
                u_con("[%s:%d] warning: text out of blocks is not allowed in "
                    "child templates, ignoring", file, ln);
                break;
            }
        }

        /* attach it to a dead tree (i.e. a tree that doesn't get printed)*/
        dbg_err_if(kblock_push_child(ppc->dead, block));
    } else {

        /* append the text block to the block list */
        dbg_err_if(kblock_push_child(ppc->cur, block));
    }

    return 0;
err:
    return ~0;
}

static int cb_pre_code_block(parser_t *p, int cmd, void *arg, const char *buf, 
        size_t sz)
{
    u_string_t *ustr = NULL;
    kblock_t *block = NULL;
    ppctx_t *ppc;
    char file[U_FILENAME_MAX];

    dbg_err_if (p == NULL);
    dbg_err_if((ppc = parser_get_cb_arg(p)) == NULL);

    if(cmd == '@')
    {   /* do preprocess */
        dbg_err_if(parse_directive(p, arg, buf, sz));
    } else {
        /* append the code to the current block */
        dbg_err_if(u_string_create(NULL, 0, &ustr));

        dbg_err_if(io_name_get(p->in, file, U_FILENAME_MAX));

        /* we must know the file:line where this code is coming from */
        if(cmd == 0 || cmd == '=')
        {
            dbg_err_if(u_string_aprintf(ustr, "<%% #line %d \"%s\" \n%%><%%", 
                 p->code_line, file)); 
            if(cmd == '=')
                dbg_err_if(u_string_aprintf(ustr, "="));
        } else {
            dbg_err_if(u_string_aprintf(ustr, 
                "<%%%c #line %d \"%s\" \n%%><%%%c", 
                 cmd, p->code_line, file, cmd)); 
        }
        dbg_err_if(u_string_aprintf(ustr, "%.*s", sz, buf));
        dbg_err_if(u_string_aprintf(ustr, "%%>"));

        /* placeholder to be subst'd in the last translation phase */
        dbg_err_if(u_string_aprintf(ustr, "<%%%c #line 0 __PG_FILE_C__ \n%%>", 
                    (cmd == '!' && cmd != 0 ? cmd : ' ')));

        dbg_err_if(kblock_create(NULL, u_string_c(ustr), u_string_len(ustr), 
                    &block));

        if(ppc->cur == ppc->top && ppc->extending)
        {
            dbg_err_if(io_name_get(p->in, file, U_FILENAME_MAX));
            u_con("[%s:%d] warning: code out of blocks is not allowed in child "
                "templates, ignoring", file, p->code_line);

            /* attach it to a dead tree (i.e. a tree that doesn't get printed)*/
            dbg_err_if(kblock_push_child(ppc->dead, block));
        } else {

            dbg_err_if(kblock_push_child(ppc->cur, block));
        }

        dbg_err_if(u_string_free(ustr));
        ustr = NULL;
    }

    return 0;
err:
    if(ustr)
        u_string_free(ustr);
    if(block)
        kblock_free(block);
    return ~0;
}

static int preprocess(io_t *in, io_t *out, ppctx_t *ppc)
{
    parser_t *p = NULL;
    char file[U_FILENAME_MAX];

    dbg_err_if(in == NULL);
    dbg_err_if(out == NULL);
    dbg_err_if(ppc == NULL);

    /* create a parse that reads from in and writes to out */
    dbg_err_if(parser_create(&p));

    parser_set_cb_arg(p, ppc);

    /* input filename */
    dbg_err_if(io_name_get(in, file, U_FILENAME_MAX));

    parser_set_io(p, in, out);

    parser_set_cb_code(p, cb_pre_code_block);
    parser_set_cb_html(p, cb_pre_html_block);

    dbg_err_if(parser_run(p));

    con_err_ifm(ppc->lev, "[%s] error: unclosed block \"%s\"", file, 
        ppc->cur->name);

    parser_free(p);

    return 0;
err:
    if(p)
        parser_free(p);
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
    con_err_if(u_tmpfile_open(NULL, &tmp));

    while(io_gets(in, buf, sizeof(buf)) > 0)
    {
        if(strstr(buf, "#line 0 __PG_FILE_C__") == NULL)
        {
            io_printf(tmp, "%s", buf);
            ln++; /* line number */
        } else
            io_printf(tmp, "#line %d \"%s\"\n", ln + 2, pti->file_out);
    }

    /* get the filename of the temporary io_t */
    dbg_err_if(io_name_get(tmp, tname, U_FILENAME_MAX));

    io_free(in), in = NULL;
    io_free(tmp), tmp = NULL;

    /* move tmp to file_out */
    u_remove(pti->file_out);

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
    io_t *in = NULL, *out = NULL, *tmp = NULL, *depend = NULL;
    ppctx_t *ppc = NULL;
    codec_t *gzip = NULL, *aes = NULL;
    char tname[U_FILENAME_MAX];

    dbg_return_if (pti == NULL, ~0);
    
    /* open the input file */
    con_err_ifm(u_file_open(pti->file_in, O_RDONLY, &in),
        "unable to open %s", pti->file_in);

    /* open the output file */
    con_err_ifm(u_file_open(pti->file_out, O_CREAT | O_TRUNC | O_WRONLY, &out),
        "unable to open %s", pti->file_out);

    /* open the depend output file */
    if(pti->depend_out[0])
        con_err_ifm(u_file_open(pti->depend_out, O_CREAT | O_TRUNC | O_WRONLY, 
            &depend), "unable to open %s", pti->depend_out);

    /* should choose the right translator based on file extensions or config */
    if(translate_is_a_script(pti->file_in))
    {
        /* get a temporary io_t */
        con_err_if(u_tmpfile_open(NULL, &tmp));

        /* create a preprocessor context */
        dbg_err_if(ppctx_create(&ppc));

        /* save the preprocessed in file to tmp */
        dbg_err_if(preprocess(in, tmp, ppc));

        /* print out the preprocessed file */
        dbg_err_if(ppctx_print(ppc, tmp));

        /* reset the tmp io */
        io_seek(tmp, 0);

        /* translate it */
        dbg_err_if(translate_script_to_c(tmp, out, pti));

        /* get the filename of the temporary io_t */
        dbg_err_if(io_name_get(tmp, tname, U_FILENAME_MAX));

        /* free the tmp io */
        io_free(tmp); tmp = NULL;

        /* remove the tmp file */
        u_remove(tname);

        /* print out the depend .kld file */
        if(depend)
            dbg_err_if(ppctx_print_depend(ppc, in, out, depend));

        dbg_err_if(ppctx_free(ppc)); ppc = NULL;
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
#ifdef SSL_ON
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

    if(pti->depend_out[0])
        io_free(depend), depend = NULL;

    io_free(out), out = NULL;
    io_free(in), in = NULL;

    /* replace '#line 0 __PG_FILE_C__' lines with '#line N "real_filename.c"' */
    if(translate_is_a_script(pti->file_in))
        dbg_err_if(fix_line_decl(pti));

    return 0;
err:
    if(pti && strlen(pti->emsg))
        u_con("%s", pti->emsg);
    if(gzip)
        codec_free(gzip);
    if(tmp)
        io_free(tmp);
    if(depend)
        io_free(depend);
    if(in)
        io_free(in);
    if(out)
        io_free(out);
    return ~0;
}
