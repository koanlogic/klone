#ifndef _UTILS_H_
#define _UTILS_H_

#include "conf.h"
#ifdef HAVE_STDINT
#include <stdint.h>
#endif /* HAVE_STDINT */
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <klone/str.h>
#include <klone/debug.h>
#include <klone/io.h>
#include <klone/md5.h>
#include <klone/os.h>
#include <klone/mime_map.h>

#ifndef MIN
#define MIN(a,b)    (a < b ? a : b)
#endif

#ifndef MAX
#define MAX(a,b)    (a > b ? a : b)
#endif

#define FQN_BUFSZ   (PATH_MAX + NAME_MAX)

#define KLONE_FREE(p) do {if (p) { free(p); p = NULL; }} while (0)

#define die(...) do { cmsg(__VA_ARGS__); exit(EXIT_FAILURE); } while(0)
#define die_if(cond, ...) do { dbg_ifb(cond) die(__VA_ARGS__); } while(0)

int u_file_exists(const char*);
int u_write_debug_message(const char*, const char*, int, const char*, 
    const char*, ...);

struct dirent;
int u_foreach_dir_item(const char *, unsigned int,
    int (*)(struct dirent*, const char *, void*), 
    void*);

void* u_malloc(size_t);
void* u_calloc(size_t);
void* u_realloc(void*, size_t);
void u_free(void *ptr);
char* u_strdup(const char *s);
char* u_strndup(const char *s, size_t len);
char* u_strnrchr(const char *s, char c, size_t len);
char *u_stristr(const char *string, const char *sub);

enum { URLCPY_VERBATIM, URLCPY_ENCODE, URLCPY_DECODE };
ssize_t u_urlncpy(char *dst, const char *src, size_t slen, int flags);

enum { HEXCPY_VERBATIM, HEXCPY_ENCODE, HEXCPY_DECODE };
ssize_t u_hexncpy(char *dst, const char *src, size_t slen, int flags);

enum { HTMLCPY_VERBATIM, HTMLCPY_ENCODE, HTMLCPY_DECODE };
ssize_t u_htmlncpy(char *dst, const char *src, size_t slen, int flags);

enum { SQLCPY_VERBATIM, SQLCPY_ENCODE, SQLCPY_DECODE };
ssize_t u_sqlncpy(char *dst, const char *src, size_t slen, int flags);


int u_printf_ccstr(io_t *o, const char *buf, size_t sz);
int u_isnl(int c);
int u_isblank(int c);
int u_isblank_str(const char *str);

int u_file_open(const char *file, int flags, io_t **pio);
int u_emb_open(const char *file, io_t **pio);
int u_tmpfile_open(io_t **pio);
int u_getline(io_t *io, string_t *ln);
int u_fgetline(FILE *in, string_t *ln);

int u_io_unzip_copy(io_t *out, const uint8_t *data, size_t size);

int u_path_snprintf(char *buf, size_t sz, const char *fmt, ...);
void u_trim(char *src);
void u_tohex(char *hex, const char *src, size_t sz);
char u_tochex(int n);
int u_snprintf(char *str, size_t size, const char *format, ...);

int u_md5(char *buf, size_t sz, char out[MD5_DIGEST_BUFSZ]);
int u_md5io(io_t *io, char out[MD5_DIGEST_BUFSZ]);

int u_signal(int sig, sig_t handler);
int u_sig_block(int sig);
int u_sig_unblock(int sig);

const char* u_guess_mime_type(const char *file_name);
const mime_map_t* u_get_mime_map(const char *file_name);

/* date time conversion funcs */
int u_tt_to_rfc822(char *dst, time_t ts, size_t sz);
int u_httpdate_to_tt(const char *str, time_t *tp);
int u_rfc822_to_tt(const char *str, time_t *tp);
int u_rfc850_to_tt(const char *str, time_t *tp);
int u_asctime_to_tt(const char *str, time_t *tp);


#endif
