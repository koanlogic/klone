#ifndef _KLONE_URI_H_
#define _KLONE_URI_H_

typedef struct 
{
    char *scheme;   
    char *user;
    char *pwd;
    char *host;
    short port;
    char *path;
} uri_t;

int uri_parse(const char *, uri_t **);
void uri_free(uri_t *);

#endif 
