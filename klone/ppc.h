#ifndef _KLONE_PPC_H_
#define _KLONE_PPC_H_

#include <stdlib.h>
#include <unistd.h>

enum { PPC_MAX_DATA_SIZE = 8192 }; 

struct ppc_s;
typedef struct ppc_s ppc_t;

typedef int (*ppc_cb_t)(ppc_t*, int fd, unsigned char cmd, char *data, 
    size_t size, void*arg);

int ppc_create(ppc_t **pppc);
int ppc_free(ppc_t *ppc);
int ppc_register(ppc_t *ppc, unsigned char cmd, ppc_cb_t func, void *arg);
int ppc_dispatch(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size);
ssize_t ppc_write(ppc_t *ppc, int fd, unsigned char cmd, char *data, 
    size_t size);
ssize_t ppc_read(ppc_t *ppc, int fd, unsigned char *cmd, char *data, 
    size_t size);

#endif
