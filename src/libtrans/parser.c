/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: parser.c,v 1.14 2008/10/18 00:03:00 tat Exp $
 */

#include "klone_conf.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <u/libu.h>
#include <klone/os.h>
#include <klone/translat.h>
#include <klone/parser.h>

/* parser state */
enum { 
    S_START, 
    S_IN_DOUBLE_QUOTE,
    S_IN_SINGLE_QUOTE, 
    S_HTML, 
    S_WAIT_PERC,
    S_START_CODE, 
    S_CODE, 
    S_WAIT_GT,
    S_EAT_NEWLINE
};

enum { LF = 0xA, CR = 0xD };

static int parser_on_block(parser_t *p, const char *buf, size_t sz)
{
    dbg_err_if (p == NULL);
    dbg_err_if (buf == NULL);

    for(;;)
    {
        switch(p->state)
        {
        case S_START:
            /* empty file */
            return 0;
        case S_IN_DOUBLE_QUOTE:
        case S_IN_SINGLE_QUOTE: 
            if(p->state != p->prev_state)
            {
                p->state = p->prev_state;
                continue;
            } else
                return 0;
        case S_HTML: 
        case S_WAIT_PERC:
            if(sz && p->cb_html)
                dbg_err_if(p->cb_html(p, p->cb_arg, buf, sz));
            return 0;
        case S_START_CODE:
        case S_CODE:
        case S_WAIT_GT:
            if(sz && p->cb_code)
                dbg_err_if(p->cb_code(p, p->cmd_code, p->cb_arg, buf, sz));
            return 0;
        }
    }

    return 0;
err:
    return ~0;
}

int parser_run(parser_t *p)
{
    enum { BUFSZ = 262144 }; /* a big buffer is good to better zip *.kl1 */
    #define set_state( s ) \
        do { tmp = p->state; p->state = s; p->prev_state = tmp; } while(0)
    #define fetch_next_char()                                           \
        do { prev = c;                                                  \
            dbg_err_if((rc = io_getc(p->in, &c)) < 0);                  \
            if(rc == 0) break;                                          \
            if( (c == CR || c == LF) && prev != (c == CR ? LF : CR))    \
                p->line++;                                              \
        } while(0)
    int tmp;
    char c = 0, prev;
    char buf[BUFSZ];
    size_t idx = 0;
    ssize_t rc;

    dbg_err_if (p == NULL);
    
    buf[0] = 0;
    prev = 0;

    dbg_err_if(p->line > 1);

    fetch_next_char();

    while(rc > 0)
    {
        prev = c;
        switch(p->state)
        {
        case S_START:
            set_state(S_HTML);
            continue;
        case S_IN_DOUBLE_QUOTE:
            if(c == '"' && prev != '\\')
                set_state(p->prev_state);
            break;
        case S_IN_SINGLE_QUOTE:
            if(c == '\'' && prev != '\\')
                set_state(p->prev_state);
            break;
        case S_HTML:
            if(c == '<')
                set_state(S_WAIT_PERC);
            break;
        case S_WAIT_PERC:
            if(c == '%')
            {
                if(idx && --idx) /* erase < */
                {
                    buf[idx] = 0;
                    dbg_err_if(parser_on_block(p, buf, idx));
                    buf[0] = 0; idx = 0;
                }
                set_state(S_START_CODE);
                p->code_line = p->line; /* save start code line number  */
                fetch_next_char();      /* get cmd char (!,@,etc.)      */
                continue;
            } else {
                set_state(S_HTML);
                continue;
            }
            break;
        case S_START_CODE:
            if(isspace(c))
                p->cmd_code = 0;
            else {
                p->cmd_code = c;
                fetch_next_char();
            }
            set_state(S_CODE);
            continue;
        case S_CODE:
            if(c == '%') 
                set_state(S_WAIT_GT);
            break;
        case S_WAIT_GT:
            if(c == '>')
            {
                if(idx && --idx) /* erase % */
                {
                    buf[idx] = 0;
                    dbg_err_if(parser_on_block(p, buf, idx));
                    buf[0] = 0; idx = 0;
                }
                fetch_next_char();
                p->cmd_code = 0;
                set_state(S_HTML);
                continue;
            } else {
                set_state(S_CODE);
                continue;
            }
            break;
        case S_EAT_NEWLINE:
            if(c == CR || c == LF)
            {
                fetch_next_char();
                continue; /* eat it */
            }
            set_state(S_HTML);
            continue;
        default:
            dbg_err_if("unknown parser state");
        }
        buf[idx++] = c;
        if(idx == BUFSZ - 1)
        {
            buf[idx] = 0;
            dbg_err_if(parser_on_block(p, buf, idx));
            buf[0] = 0; idx = 0;
        }

        fetch_next_char();
    }

    if(idx)
    {
        buf[idx] = 0;
        dbg_err_if(parser_on_block(p, buf, idx));
        buf[0] = 0; idx = 0;
    }

    return 0;
err:
    return ~0;
}

void parser_set_cb_code(parser_t *p, parser_cb_code_t cb)
{
    dbg_ifb (p == NULL) return;
    p->cb_code = cb;
}

void parser_set_cb_html(parser_t *p, parser_cb_html_t cb)
{
    dbg_ifb (p == NULL) return;
    p->cb_html = cb;
}

void* parser_get_cb_arg(parser_t *p)
{
    dbg_ifb (p == NULL) return NULL;
    return p->cb_arg;
}

void parser_set_cb_arg(parser_t *p, void *opaque)
{
    dbg_ifb (p == NULL) return;
    p->cb_arg = opaque;
}

void parser_set_io(parser_t *p, io_t *in, io_t *out)
{
    dbg_ifb (p == NULL) return;
    p->in = in;
    p->out = out;
}

int parser_free(parser_t *t)
{
    U_FREE(t);
    return 0;
}

int parser_reset(parser_t *p)
{
    dbg_return_if (p == NULL, ~0);

    p->line = 1;
    p->state = p->prev_state = S_START;
    p->cmd_code = 0;

    return 0;
}

int parser_create(parser_t **pt)
{
    parser_t *p = NULL;

    dbg_return_if (pt == NULL, ~0);
    
    p = (parser_t*)u_zalloc(sizeof(parser_t));
    dbg_err_if(p == NULL);

    (void) parser_reset(p);

    *pt = p;

    return 0;
err:
    if(p)
        parser_free(p);
    return ~0;
}
