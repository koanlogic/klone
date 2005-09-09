#ifndef _KLONE_DO_H_
#define _KLONE_DO_H_

typedef struct do_s
{
    char *fqn;    /* obj file name */
    void *handle; /* obj handle    */
} do_t;

int  do_load(const char *, do_t **);
void do_free(do_t *);
int  do_sym(do_t *, const char *, void **);

#endif
