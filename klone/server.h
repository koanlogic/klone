#ifndef _KLONE_SERVER_H_
#define _KLONE_SERVER_H_

struct config_s;
struct server_s;
typedef struct server_s server_t;

enum { 
    SERVER_MODEL_UNSET,     /* uninitialized                        */
    SERVER_MODEL_FORK,      /* fork for each incoming connection    */
    SERVER_MODEL_ITERATIVE  /* serialize responses                  */
};

int server_create(struct config_s *config, int model, server_t **ps);
int server_free(server_t *s);
int server_loop(server_t *s);
int server_cgi(server_t *s);
int server_stop(server_t *s);

#endif
