#ifndef _KLONE_HEADER_H_
#define _KLONE_HEADER_H_
#include <klone/field.h>
#include <klone/io.h>

typedef struct
{
     fields_t fields;         
     size_t nfields;
} header_t;

int header_create(header_t**);
int header_load(header_t*, io_t *);
int header_free(header_t*);
int header_add_field(header_t *h, field_t *f);
int header_del_field(header_t *h, field_t *f);
field_t* header_get_field(header_t *h, const char *name);

const char* header_get_field_value(header_t *h, const char *name);

int header_set_field(header_t *h, const char *name, const char *value);

field_t* header_get_fieldn(header_t *h, size_t idx);
size_t header_field_count(header_t *h);

#endif
