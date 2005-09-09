#ifndef _KLONE_CONFIG_H_
#define _KLONE_CONFIG_H_
#include <klone/io.h>

struct config_s;
typedef struct config_s config_t;

int config_create(config_t **pc);
int config_free(config_t *c);
int config_load(config_t *c, io_t *io, int overwrite);

const char* config_get_key(config_t *c);
const char* config_get_value(config_t *c);

int config_get_subkey(config_t *c, const char *subkey, config_t **pc);
int config_get_subkey_nth(config_t *c,const char *subkey, int n, config_t **pc);

const char* config_get_subkey_value(config_t *c, const char *subkey);

int config_add_key(config_t *c, const char *key, const char *val);
int config_set_key(config_t *c, const char *key, const char *val);

void config_print(config_t *c, int lev);

#endif
