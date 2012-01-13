/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: utils.h,v 1.35 2009/05/29 10:26:00 tho Exp $
 */

#ifndef _KLONE_UTILS_H_
#define _KLONE_UTILS_H_

#include "klone_conf.h"
#ifdef HAVE_STDINT
#include <stdint.h>
#endif /* HAVE_STDINT */
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <u/libu.h>
#include <klone/io.h>
#include <klone/md5.h>
#include <klone/os.h>
#include <klone/mime_map.h>
#ifdef SSL_ON
#include <openssl/evp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define KLONE_FREE(p) do {if (p) { free(p); p = NULL; }} while (0)

#define klone_die(...) do { u_con(__VA_ARGS__); exit(EXIT_FAILURE); } while(0)
#define klone_die_if(cond, ...) \
    do { dbg_ifb(cond) klone_die(__VA_ARGS__); } while(0)

enum { U_PATH_NOT_FOUND, U_PATH_IN_EMBFS, U_PATH_IN_FS };

const char *u_addr_get_ip (const char *a, char *d, size_t dlen);
const char *u_addr_get_port (const char *a, char *d, size_t dlen);
const char *u_addr_fmt (const char *ip, const char *port, char *d, size_t dlen);

int u_file_exists(const char*);
int u_write_debug_message(const char*, const char*, int, const char*, 
    const char*, ...);

struct dirent;
int u_foreach_dir_item(const char *, unsigned int,
    int (*)(struct dirent*, const char *, void*), 
    void*);

char* u_strnrchr(const char *s, char c, size_t len);
char *u_stristr(const char *string, const char *sub);
char *u_strnstr(const char *string, const char *sub, size_t stringlen);

enum { U_COPY_VERBATIM, U_COPY_ENCODE, U_COPY_DECODE };

enum { URLCPY_VERBATIM, URLCPY_ENCODE, URLCPY_DECODE };
ssize_t u_urlncpy(char *dst, const char *src, size_t slen, int flags);

enum { HEXCPY_VERBATIM, HEXCPY_ENCODE, HEXCPY_DECODE };
ssize_t u_hexncpy(char *dst, const char *src, size_t slen, int flags);

enum { HTMLCPY_VERBATIM, HTMLCPY_ENCODE, HTMLCPY_DECODE };
ssize_t u_htmlncpy(char *dst, const char *src, size_t slen, int flags);

enum { SQLCPY_VERBATIM, SQLCPY_ENCODE, SQLCPY_DECODE };
ssize_t u_sqlncpy(char *dst, const char *src, size_t slen, int flags);

int u_printf_ccstr(io_t *o, const char *buf, size_t sz);

int u_file_open(const char *file, int flags, io_t **pio);
int u_tmpfile_open(const char *tmpdir, io_t **pio);
int u_getline(io_t *io, u_string_t *ln);
int u_fgetline(FILE *in, u_string_t *ln);

int u_io_unzip_copy(io_t *out, const char *data, size_t size);

void u_tohex(char *hex, const char *src, size_t sz);
char u_tochex(int n);

int u_md5(const char *buf, size_t sz, char out[MD5_DIGEST_BUFSZ]);
int u_md5io(io_t *io, char out[MD5_DIGEST_BUFSZ]);

typedef void (*u_sig_t)(int);
int u_signal(int sig, u_sig_t handler);
int u_sig_block(int sig);
int u_sig_unblock(int sig);

const char* u_guess_mime_type(const char *file_name);
const mime_map_t* u_get_mime_map(const char *file_name);
int u_match_ext(const char *filename, const char *extension);

/* date time conversion funcs */
#define RFC822_DATE_BUFSZ   32

int u_tt_to_rfc822(char dst[RFC822_DATE_BUFSZ], time_t ts);
int u_httpdate_to_tt(const char *str, time_t *tp);
int u_rfc822_to_tt(const char *str, time_t *tp);
int u_rfc850_to_tt(const char *str, time_t *tp);
int u_asctime_to_tt(const char *str, time_t *tp);

void u_print_version_and_exit(void);

int u_uri_normalize(char *fqn);
int u_path_where_art_thou(const char *fqn, int *where);

int u_pwd_init_agnostic (const char *fqn, int hashed, int in_memory, 
        u_pwd_t **ppwd);

#ifdef SSL_ON
int u_cipher_encrypt(const EVP_CIPHER *cipher, unsigned char *key, 
    unsigned char *iv, char *dst, size_t *dcount, const char *src, size_t ssz);
int u_cipher_decrypt(const EVP_CIPHER *cipher, unsigned char *key, 
    unsigned char *iv, char *dst, size_t *dcount, const char *src, size_t ssz);
#endif

#ifdef __cplusplus
}
#endif 

#endif
