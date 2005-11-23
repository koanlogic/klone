#ifndef _KLONE_PARSER_H_
#define _KLONE_PARSER_H_

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <klone/io.h>

struct parser_s;
typedef struct parser_s parser_t;

struct parser_s
{
    io_t *in, *out;
    int state, prev_state, cmd_code, line, code_line;
    void *cb_arg; /* opaque callback funcs argument */
    /* <%[.]   %>  where . is a not blank char */
    int (*cb_code)       (parser_t*, int, void*, const char*, size_t); 
    int (*cb_html)       (parser_t*, void*, const char*, size_t); /* HTML     */
}; 

typedef int (*parser_cb_html_t)(parser_t*, void*, const char*, size_t);
typedef int (*parser_cb_code_t)(parser_t*, int, void*, const char*, size_t);

int parser_create(parser_t **);
int parser_free(parser_t *);
int parser_run(parser_t *);
int parser_reset(parser_t *);

void parser_set_io(parser_t *, io_t *in, io_t *out);
void parser_set_cb_code(parser_t *, parser_cb_code_t);
void parser_set_cb_html(parser_t *, parser_cb_html_t);
void parser_set_cb_arg(parser_t *, void *);

#endif
