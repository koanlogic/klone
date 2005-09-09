#ifndef _KLONE_STRING_H_
#define _KLONE_STRING_H_
#include <sys/types.h>

enum { BLOCK_SIZE = 64 };

struct string_s;
typedef struct string_s string_t;

#define STRING_NULL { NULL, 0, 0, 0 };

int string_create(const char *buf, size_t len, string_t **ps);
int string_append(string_t *s, const char *buf, size_t len);
int string_set(string_t *s, const char *buf, size_t len);
int string_clear(string_t *s);
int string_free(string_t *s);
const char *string_c(string_t *s);
size_t string_len(string_t *s);
int string_copy(string_t *dst, string_t *src);
int string_set_length(string_t *s, size_t len); 
int string_trim(string_t *s);

int string_url_encode(string_t *s);
int string_url_decode(string_t *s);

int string_html_encode(string_t *s);
int string_html_decode(string_t *s);

int string_sql_encode(string_t *s);
int string_sql_decode(string_t *s);

#endif
