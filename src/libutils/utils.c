/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: utils.c,v 1.58 2009/05/29 12:14:59 tho Exp $
 */

#include "klone_conf.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <u/libu.h>
#include <klone/os.h>
#include <klone/io.h>
#include <klone/codecs.h>
#include <klone/emb.h>
#include <klone/mime_map.h>
#include <klone/version.h>
#include <klone/utils.h>
#ifdef HAVE_STRINGS
#include <strings.h>
#endif
#ifdef SSL_CYASSL
#include <config.h>
#include <types.h>
#include <ctc_aes.h>
#endif

enum { LF = 0xA, CR = 0xD };

static struct html_entities_s
{
    int s_char;
    const char *entity; 
} entities[] = {
    { '&',  "&amp;"  },
    { '"',  "&quot;" },
    { '\'', "&#39;"  }, 
    { '<',  "&lt;"   },
    { '>',  "&gt;"   },
    { 0, NULL     }
};

#ifdef OS_UNIX
inline int u_sig_block(int sig)
{
    sigset_t sset;

    sigemptyset(&sset);
    sigaddset(&sset, sig);
    dbg_err_if(sigprocmask(SIG_BLOCK, &sset, NULL));

    return 0;
err:
    return ~0;
}

inline int u_sig_unblock(int sig)
{
    sigset_t sset;

    sigemptyset(&sset);
    sigaddset(&sset, sig);
    dbg_err_if(sigprocmask(SIG_UNBLOCK, &sset, NULL));

    return 0;
err:
    return ~0;
}
#endif /* OS_UNIX */


/* Get address from addr+port combo string.  UNIX IPC paths are returned 
 * AS-IS. */
const char *u_addr_get_ip (const char *a, char *d, size_t dlen)
{
    char *e;
    const char *b;

    dbg_return_if (a == NULL, NULL);
    dbg_return_if (d == NULL, NULL);
    dbg_return_if (dlen == 0, NULL);

    switch (a[0])
    {
        case '/':   /* UNIX path is returned as-is. */
            dbg_if (u_strlcpy(d, a, dlen));
            return d;
        case '[':   /* Handle IPv6. */
            e = strchr(a, ']');
            b = a + 1;
            break;
        default:    /* Assume IPv4. */
            e = strchr(a, ':');
            b = a;
            break;
    }

    if (e != NULL)
    {
        dbg_if (u_strlcpy(d, b, U_MIN((size_t) (e - b + 1), dlen)) + 1);
        return d;
    }

    u_warn("unable to get an IP from '%s'", a);
    return NULL;
}

/* Get port from addr+port combo string.  UNIX IPC paths return the empty
 * string. */
const char *u_addr_get_port (const char *a, char *d, size_t dlen)
{
    char *e, *p;

    dbg_return_if (a == NULL, NULL);
    dbg_return_if (d == NULL, NULL);
    dbg_return_if (dlen == 0, NULL);

    /* Empty port returned on UNIX path. */
    if (a[0] == '/')
    {
        d[0] = '\0';
        return d;
    }

    /* Extra check. */
    dbg_return_if ((e = strchr(a, '\0')) == NULL, NULL);

    /* Go backwards until the addr/port separator is found. */
    for (p = e; *p != ':' && p > a; --p)
        ;

    dbg_if (u_strlcpy(d, (p > a) ? p + 1 : "", dlen));
        
    return d;
}

/* Format address string to be used in request->{peer,local}_addr fields.
 * Basically this function tries to emulate u_sa_ntop() + UNIX IPC paths 
 * handling. */
const char *u_addr_fmt (const char *ip, const char *port, char *d, size_t dlen)
{
    int isv6 = 1;

    dbg_return_if (d == NULL, NULL);
    dbg_return_if (dlen == 0, NULL);

    /* At least 'ip' must be there. */
    if (!ip || ip[0] == '\0')
    {
        d[0] = '\0';
        return d;
    }

    /* Handle UNIX IPC endpoints: make verbatim copy of the 'ip' parameter,
     * and ignore 'port' altogether. */
    if (ip[0] == '/')
    {
        dbg_if (u_strlcpy(d, ip, dlen));
        return d;
    }

    /* Tell v4 from v6 -- this test may be made less silly :-) */
    if (strchr(ip, '.'))
        isv6 = 0;

    /* In case 'port' is NULL, a fake '0' port is added anyway. */
    dbg_if (u_snprintf(d, dlen, "%s%s%s:%s", 
                isv6 ? "[" : "", ip, isv6 ? "]" : "", port ? port : "0"));

    return d;
}

/**
 * \ingroup ut
 * \brief   Locate a substring in another string
 * 
 * The function locates the first occurrence of \p sub in the string 
 * buf of size buflen 
 *
 * \param   buf 
 * \param   sub 
 * \param   buflen 
 *
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
char *u_strnstr(const char *buf, const char *sub, size_t buflen)
{
    ssize_t len = strlen(sub);
    ssize_t plen;
    char *p;

    if (*sub == 0)
        return (char *)buf;

    plen = buflen;
    for (p = (char *)buf; p != NULL; p = memchr(p + 1, *sub, plen - 1))
    {
        plen = buflen - (p - buf);

        if (plen < len)
            return NULL;

        if (strncmp(p, sub, len) == 0)
            return (p);
    }

    return NULL;
}

/**
 * \ingroup ut
 * \brief   Apply the supplied callback to each file in a given directory
 * 
 * Apply the supplied callback \p cb with additional arguments \p arg to each
 * file in directory \p path which match the given \p mask.
 *
 * \param   path    directory path
 * \param   mask    matching file mask 
 * \param   cb      function to call
 * \param   arg     optional additional arguments
 *
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
int u_foreach_dir_item(const char *path, unsigned int mask,
    int (*cb)(struct dirent*, const char *, void*), void* arg)
{
    struct dirent *de;
    struct stat st;
    DIR *dir = NULL;
    char buf[U_FILENAME_MAX];
    int rc;

    dbg_return_if (path == NULL, ~0);
    dbg_return_if (cb == NULL, ~0);
    
    /* open the given directory */
    dir = opendir(path);
    dbg_err_if(dir == NULL);

    while((de = readdir(dir)) != NULL)
    {
        /* skip . and .. dirs */
        if(!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;

        /* build fully qualified name of this item */
        dbg_err_if(u_snprintf(buf, U_FILENAME_MAX, "%s/%s", path, de->d_name));

        dbg_err_if( (rc = stat(buf, &st)) == -1);

        /* skip if its type is not in the requested mask */
        if(((st.st_mode & S_IFMT) & mask) != 0 && cb(de, path, arg))
            break;
    }

    closedir(dir);

    return 0;
err:
    return ~0;
}

/**
 * \ingroup ut
 * \brief   Match filename extension
 * 
 * Return 1 if the filename externsion is equal to \p extension
 * (case-insensitive comparison).
 *
 * \param   filename    file name
 * \param   extension  file extension to match 
 *
 * \return
 * - \c 1  if \p filename extension is \p extension
 * - \c 0  if \p filename extension is not equal to \p extension
 */
int u_match_ext(const char *filename, const char *extension)
{
    const char *fn, *ext;
    size_t flen, elen;

    if(filename == NULL || extension == NULL)
        return 0;

    flen = strlen(filename);
    elen = strlen(extension);
    if(elen > flen)
        return 0;

    fn = filename + flen - 1;
    ext = extension + elen - 1;
    for( ; elen; --fn, --ext, --elen)
    {
        if(tolower(*fn) != tolower(*ext))
            return 0;
    }
    return 1;
}

/* hex char to int */
static short htoi(unsigned char c)
{
    c = tolower(c);

    if(c >= '0' && c <= '9')
        return c - '0';
    else if(c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    else
        return 0;
}


static ssize_t u_sqlncpy_encode(char *d, const char *s, size_t slen)
{
    ssize_t wr = 0;
    unsigned char c;

    dbg_return_if (d == NULL, -1);
    dbg_return_if (s == NULL, -1);

    for(; slen; --slen)
    {
        c = *d++ = *s++;
        wr++;
        if(c == '\'') 
        {
            dbg_err_if(slen < 2);
            *d++ = '\'';
            wr++;
            --slen;
        } 
    }
    *d = 0;

    return wr;
err:
    return -1;
}

static ssize_t u_sqlncpy_decode(char *d, const char *s, size_t slen)
{
    unsigned char c, last = 0;
    ssize_t wr = 0;
    
    dbg_return_if (d == NULL, -1);
    dbg_return_if (s == NULL, -1);

    for(; slen; --slen)
    {
        c = *s++;
        if(c == '\'' && last == c) 
        {
            last = 0; 
            ; /* skip */
        } else {
            *d++ = c;
            last = c;
            wr++;
        }
    }
    *d = 0;

    return wr;
}

/**
 * \ingroup ut
 * \brief   Copy and SQL escape/unescape a given string 
 *
 * Copy and SQL escape/unescape, depending on \p flags value, the string \p s 
 * into \p d.  The destination string, which must be at least \p slen + 1 bytes
 * long, is NULL terminated.
 *
 * \param   d       the encoded/decoded string
 * \param   s       string to process
 * \param   slen    length of \p s
 * \param   flags   one of \c SQLCPY_ENCODE or \c SQLCPY_DECODE
 *
 * \return  The number of characters written to \p d not including the 
 *          trailing '\\0' or \c -1 on error.
 */
ssize_t u_sqlncpy(char *d, const char *s, size_t slen, int flags)
{
    dbg_return_if (d == NULL, -1);
    dbg_return_if (s == NULL, -1);

    switch(flags)
    {
    case SQLCPY_ENCODE:
        return u_sqlncpy_encode(d, s, slen);
    case SQLCPY_DECODE:
        return u_sqlncpy_decode(d, s, slen);
    default:
        u_strlcpy(d, s, slen + 1);
        return slen;
    }

    return -1;
}

static ssize_t u_urlncpy_encode(char *d, const char *s, size_t slen)
{
    const char hexc[] = "0123456789ABCDEF";
    ssize_t wr = 0;
    unsigned char c;

    dbg_return_if (d == NULL, -1);
    dbg_return_if (s == NULL, -1);

    for(; slen; --slen)
    {
        c = *s++;
        if(c == ' ') {
            *d++ = '+';
            wr++;
        } else if(isalnum(c) || c == '_' || c == '-' || c == '.') {
            *d++ = c;
            wr++;
        } else {
            *d++ = '%';                                        
            *d++ = hexc[(c >> 4) & 0xF];             
            *d++ = hexc[c & 0xF];  
            wr += 3;
        }
    }
    *d = 0;

    return wr;
}

static ssize_t u_urlncpy_decode(char *d, const char *s, size_t slen)
{
    unsigned char c;
    ssize_t wr = 0;

    dbg_return_if (d == NULL, -1);
    dbg_return_if (s == NULL, -1);

    for(; slen; --slen, ++wr)
    {
        c = *s++;
        if(c == '%')
        {
            dbg_err_if(slen < 2 || !isxdigit(s[0]) || !isxdigit(s[1]));
            c = htoi(s[0]) << 4 | htoi(s[1]);
            //dbg_err_if(c == 0);
            *d++ = (char)c;
            s += 2;
            slen -= 2;
        } else if(c == '+') {
            *d++ = ' ';
        } else {
            *d++ = c;
        }
    }
    *d = 0;

    return wr;
err:
    return -1;

}

/**
 * \ingroup ut
 * \brief   Copy and URL escape/unescape a given string 
 *
 * Copy an URL escaped/unescaped version of string \p s, depending on 
 * \p flags value, into \p d.  The destination string is NULL terminated.
 * The destination string \p d must be at least \p slen + 1 bytes long.
 *
 * \param   d       the encoded/decoded string
 * \param   s       string to process
 * \param   slen    length of \p s
 * \param   flags   one of \c URLCPY_ENCODE or \c URLCPY_DECODE
 *
 * \return  The number of characters written to \p d not including the 
 *          trailing '\\0' or \c -1 on error.
 */
ssize_t u_urlncpy(char *d, const char *s, size_t slen, int flags)
{
    dbg_return_if (d == NULL, -1);
    dbg_return_if (s == NULL, -1);

    switch(flags)
    {
    case URLCPY_ENCODE:
        return u_urlncpy_encode(d, s, slen);
    case URLCPY_DECODE:
        return u_urlncpy_decode(d, s, slen);
    default:
        u_strlcpy(d, s, slen + 1);
        return slen;
    }

    return -1;
}

inline char u_tochex(int n)
{
	if(n > 15)
		return '?';
	return ( n < 10 ? n + '0' : n-10 + 'a');
}

static int u_hex2ch(char c)
{
    if(c >= 'a' && c <= 'z') 
        return c - 'a' + 10;
    else if(c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    else if(c >= '0' && c <= '9') 
        return c - '0';
    else
        return -1; /* error */
}

void u_print_version_and_exit(void)
{
    static const char *vv = 
    "KLone %s - Copyright (c) 2005-2012 KoanLogic s.r.l. - "
    "All rights reserved. \n\n";

    fprintf(stderr, vv, klone_version());

    exit(EXIT_FAILURE);
}

static ssize_t u_hexncpy_decode(char *d, const char *s, size_t slen)
{
	size_t i, t;

    dbg_return_if (d == NULL, -1);
    dbg_return_if (s == NULL, -1);

    /* slen must be multiple of 2 */
    dbg_err_if((slen % 2) != 0);

	for(i = 0, t = 0; i < slen; ++t, i += 2)
        d[t] = (u_hex2ch(s[i]) << 4) | u_hex2ch(s[i+1]); 

    d[t] = 0; /* zero-term */

    return t;
err:
    return -1;
}

static ssize_t u_hexncpy_encode(char *d, const char *s, size_t slen)
{
	size_t c, i, t;

    dbg_return_if (d == NULL, -1);
    dbg_return_if (s == NULL, -1);

	for(i = 0, t = 0; i < slen; ++i, t += 2)
	{
		c = s[i];
        d[t]   = u_tochex((c >> 4) & 0x0F);
        d[t+1] = u_tochex(c & 0x0F);
	}
    d[t] = 0; /* zero-term */

    return t;
}

/**
 * \ingroup ut
 * \brief   Copy and HEX encode/decode a given string 
 *
 * Copy an HEX encoded/decoded version of string \p s, depending on 
 * \p flags value, into \p d.  The destination string \p d, which must be
 * at least \p slen + 1 bytes long, is NULL terminated.
 *
 * \param   d       the encoded/decoded string
 * \param   s       string to process
 * \param   slen    length of \p s
 * \param   flags   one of \c HEXCPY_ENCODE or \c HEXCPY_DECODE
 *
 * \return  The number of characters written to \p d not including the 
 *          trailing '\\0' or \c -1 on error.
 */
ssize_t u_hexncpy(char *d, const char *s, size_t slen, int flags)
{
    dbg_err_if (d == NULL);
    dbg_err_if (s == NULL);

    switch(flags)
    {
    case HEXCPY_ENCODE:
        return u_hexncpy_encode(d, s, slen);
    case HEXCPY_DECODE:
        return u_hexncpy_decode(d, s, slen);
    default:
        u_strlcpy(d, s, slen + 1);
        return slen;
    }

err:
    return -1;
}

static ssize_t u_htmlncpy_encode(char *d, const char *s, size_t slen)
{
    struct html_entities_s *p;
    const char *map[256];
    size_t elen;
    unsigned char c;
    ssize_t wr = 0;

    dbg_return_if (d == NULL, -1);
    dbg_return_if (s == NULL, -1);

    /* build the map table (could be static but it wouldn't be thread-safe) */
    memset(map, 0, sizeof(map));
    for(p = entities; p->s_char; ++p)
        map[p->s_char] = p->entity;

    while(slen)
    {
        c = *s++;
        if(map[c] == NULL)
        {
            *d++ = c;   /* this char doesn't need encoding */
            wr++;
            --slen;
        } else {
            elen = strlen(map[c]);
            strcpy(d, map[c]); /* append the entity */
            --slen;
            d += elen;
            wr += elen;
        }
    }
    *d = 0;

    return wr;
}

static ssize_t u_htmlncpy_decode(char *d, const char *s, size_t slen)
{
    struct html_entities_s *p;
    char *found, *after;

    dbg_return_if (d == NULL, -1);
    dbg_return_if (s == NULL, -1);

    u_strlcpy(d, s, slen + 1);

    for(p = entities; p->s_char; ++p)
    {
        while((found = u_stristr(d, p->entity)) != NULL)
        {
            *found = p->s_char;
            after = found + strlen(p->entity);
            memmove(++found, after, 1 + strlen(after));
        }
    }

    return strlen(d);
}

/**
 * \ingroup ut
 * \brief   Copy and HTML escape/unescape a given string 
 *
 * Copy an HTML escaped/unescaped version of string \p s, depending on 
 * \p flags value, into \p d.  The destination string is NULL terminated.
 * The destination string \p d must be at least \p slen + 1 bytes long.
 *
 * \param   d       the encoded/decoded string
 * \param   s       string to process
 * \param   slen    length of \p s
 * \param   flags   one of \c HTMLCPY_ENCODE or \c HTMLCPY_DECODE
 *
 * \return  The number of characters written to \p d not including the 
 *          trailing '\\0' or \c -1 on error.
 */
ssize_t u_htmlncpy(char *d, const char *s, size_t slen, int flags)
{
    dbg_err_if (d == NULL);
    dbg_err_if (s == NULL);

    switch(flags)
    {
    case HTMLCPY_ENCODE:
        return u_htmlncpy_encode(d, s, slen);
    case HTMLCPY_DECODE:
        return u_htmlncpy_decode(d, s, slen);
    default:
        u_strlcpy(d, s, slen + 1);
        return slen;
    }
err:
    return -1;
}

/**
 * \ingroup ut
 * \brief   Locate a given substring ignoring case
 *
 * Locate the first occurrence of the null-terminated string \p sub in the 
 * null-terminated string \p string, ignoring the case of both string.
 *
 * \param   string  string to be searched
 * \param   sub     substring to search
 *
 * \return  the pointer to the found substring or NULL if \p sub occurs nowhere
 *          in \p string
 */
char *u_stristr(const char *string, const char *sub)
{
    const char *p;
    size_t len;

    dbg_err_if (sub == NULL);
    dbg_err_if (string == NULL);

    len = strlen(sub);
    for(p = string; *p; ++p)
    {
        if(strncasecmp(p, sub, len) == 0)
            return (char*)p;
    }

err: /* fall through */
    return NULL;
}

/**
 * \ingroup ut
 * \brief   Locate a character in a string
 *
 * Locate the last occurrence of \p c in the substring of length \p len
 * starting at \p s.
 *
 * \param   s       pointer to the starting of the string
 * \param   c       the character to search
 * \param   len     length of the string to be searched 
 *
 * \return  the pointer to the character, or NULL if \p c doesn't occur in \p s
 */
char* u_strnrchr(const char *s, char c, size_t len)
{
    register int i = len - 1;

    dbg_err_if (s == NULL);
    
    for(; i >= 0; --i)
        if(s[i] == c)
            return (char*)s + i; /* found */
err:
    return NULL;
}

/**
 * \ingroup ut
 * \brief   Create a temporary \c io_t object 
 *
 * Create a temporary \c io_t object at \p *pio.
 *
 * \param   tmpdir  optional base temp directory, supply \c NULL to use the
 *                  default value
 * \param   pio     pointer to the temporary \c io_t object
 *
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
int u_tmpfile_open (const char *tmpdir, io_t **pio)
{
    int fd = -1;
    unsigned int i;
    char tmpf[U_FILENAME_MAX];
    const char *pfx = "kloned_tmp_";
    /* Mimic tempnam(3) directory search for backwards compatibility. */
    const char *dirs[] = {
        tmpdir,
        getenv("TMPDIR"),
        P_tmpdir,
        "/tmp",
        "."
    };

    dbg_return_if (pio == NULL, ~0);

    for (i = 0; i < (sizeof(dirs) / sizeof(char *)); i++)
    {
        if (dirs[i])
        {
            /* Do the template for the temporary file name. */
            if (u_path_snprintf(tmpf, sizeof tmpf, U_PATH_SEPARATOR, 
                        "%s/%sXXXXXXXXXX", dirs[i], pfx))
                continue;

            /* Try to create it in the file system. */
            if ((fd = mkstemps(tmpf, 0)) == -1)
                continue;

            /* Abort on any error subsequent to file creation. */
            dbg_err_if (io_fd_create(fd, IO_FD_CLOSE, pio));
            dbg_err_if (io_name_set(*pio, tmpf));

            return 0;
        }
    }

    /* Loop exhausted, bail out. */

err:
    if (fd != -1)
        (void) close(fd);

    return ~0;
}

/**
 * \ingroup ut
 * \brief   Create an \c io_t object from the file system object \p file
 *
 * Create an \c io_t object at \p *pio from the file system object \p file.
 * The file is opened with the permission bits given in \p mode.
 *
 * \param   file    pathname of the file to open
 * \param   flags   permission bits passed to the open syscall
 * \param   pio     the \c io_t object associated to \p file
 *
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
int u_file_open(const char *file, int flags, io_t **pio)
{
    int fmod = 0; /* flags modifier */
    int fd;

#ifdef OS_WIN
    fmod = _O_BINARY;
#endif
    
    dbg_return_if (file == NULL, ~0);
    dbg_return_if (pio == NULL, ~0);
    
    fd = open(file, fmod | flags, 0600);
    dbg_err_if(fd < 0);

    dbg_err_if(io_fd_create(fd, IO_FD_CLOSE, pio));

    /* name the stream */
    dbg_err_if(io_name_set(*pio, file));

    return 0;
err:
    if(fd < 0)
        dbg_strerror(errno);
    else
        close(fd);
    return ~0;
}

/**
 * \ingroup ut
 * \brief   Read a line from the \c io_t object \p io
 *
 * Read a line and place it into \p ln from the \c io_t object \p io
 *
 * \param   io  an initialised \c io_t object
 * \param   ln  the line read 
 *
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
int u_getline(io_t *io, u_string_t *ln)
{
    enum { BUFSZ = 1024 };
    char buf[BUFSZ];
    ssize_t len, rc;

    dbg_return_if (io == NULL, ~0);
    dbg_return_if (ln == NULL, ~0);
    
    u_string_clear(ln);

    while((rc = len = io_gets(io, buf, BUFSZ)) > 0)
    {
        dbg_err_if(u_string_append(ln, buf, --len));
        if(!u_isnl(buf[len]))
            continue; /* line's longer the bufsz (or eof);get next line chunk */
        else
            break;
    }

    dbg_if(rc < 0); /* io_gets error */

err:
    return (rc <= 0 ? ~0 : 0);
}

/**
 * \ingroup ut
 * \brief   get a line from a \c FILE object
 *
 * Try to get a line from the \c FILE object \p in and store it at \p ln.
 *
 * \param   in  the \c FILE object from which read is performed
 * \param   ln  the \c u_string_t object where the line read is stored
 *
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
int u_fgetline(FILE *in, u_string_t *ln)
{
    enum { BUFSZ = 256 };
    char buf[BUFSZ];
    size_t len;

    dbg_return_if (in == NULL, ~0);
    dbg_return_if (ln == NULL, ~0);
    
    u_string_clear(ln);

    while(!ferror(in) && !feof(in) && fgets(buf, BUFSZ, in))
    {
        len = strlen(buf);
        dbg_err_if(u_string_append(ln, buf, len));
        if(!u_isnl(buf[len-1]))
            continue; /* line's longer the bufsz, get next line chunk */
        else
            break;
    }

    if(ferror(in))
        dbg_strerror(errno);
err:
    return (u_string_len(ln) ? 0 : ~0);
}

int u_printf_ccstr(io_t *o, const char *buf, size_t sz)
{
    char prev, c = 0;
    int pos = 0;
    size_t i;

    dbg_return_if (o == NULL, ~0);
    dbg_return_if (buf == NULL, ~0);
    
    for(i = 0; i < sz; ++i)
    {
        prev = c;
        c = buf[i];
        if(pos++ == 0) // first line char
            io_putc(o, '"');
        switch(c)
        {
        case CR:
            if(prev != LF) 
            io_printf(o, "\\n\"\n");
            pos = 0;
            break;
        case LF:
            if(prev != CR) 
            io_printf(o, "\\n\"\n");
            pos = 0;
            break;
        case '"':
            io_printf(o, "\\\"");
            break;
        case '\\':
            io_printf(o, "\\\\");
            break;
        default:
            if(isprint(c))
                io_putc(o, c);
            else {
                io_printf(o, "\\x%c%c", u_tochex((c >> 4) & 0x0F),
                u_tochex(c & 0x0F));
            }
        }
    }
    if(pos)
        io_putc(o, '"');

    return 0;
}

/**
 * \ingroup ut
 * \brief   Tell if the given file exists
 *
 * Tell if the given file \p fqn exists
 *
 * \param   fqn     the path of the (regular) file to check
 *
 * \return  \c 1 if the file exists and is a regular file, \c 0 otherwise 
 */
int u_file_exists(const char *fqn)
{
    struct stat st;

    dbg_return_if (fqn == NULL, 0);
    
    return stat(fqn, &st) == 0 && S_ISREG(st.st_mode);
}

/**
 * \ingroup ut
 * \brief   Convert a given string in hexadecimal representation
 *
 * Convert the string \p src of lenght \p sz into its hexadecimal 
 * representation \p hex.  The string \p hex must be at least \c 2 \c * \p sz
 * long.
 *
 * \param   hex     the string holding the hexadecimal conversion of \p src
 * \param   src     the string that has to be converted
 * \param   sz      the length of \p src
 *
 * \return  nothing
 */
void u_tohex(char *hex, const char *src, size_t sz)
{
    size_t c, i, t;

    dbg_ifb (hex == NULL) return;
    dbg_ifb (src == NULL) return;
    
    for(i = 0, t = 0; i < sz; ++i, t += 2)
    {
        c = src[i];
        hex[t]   = u_tochex((c >> 4) & 0x0F);
        hex[t+1] = u_tochex(c & 0x0F);
    }

    hex[t] = 0; /* zero-term */
}

/**
 * \ingroup ut
 * \brief   Calculate the MD5 digest over a given buffer
 *
 * Calculate the MD5 digest over the supplied buffer \p buf of size \p sz
 * and place it at \p out.
 *
 * \param   buf     the buffer to be hashed
 * \param   sz      length in bytes of \p buf
 * \param   out     hexadecimal string containing the MD5 hash calculated over
 *                  \p buf.  It must be at least \c MD5_DIGEST_BUFSZ bytes long.
 * \return
 * - \c 0   always successful
 */
int u_md5(const char *buf, size_t sz, char out[MD5_DIGEST_BUFSZ])
{
    md5_state_t md5ctx;
    md5_byte_t md5_digest[16]; /* binary digest */

    dbg_return_if (buf == NULL, ~0);
    dbg_return_if (out == NULL, ~0);
    
    md5_init(&md5ctx);
    md5_append(&md5ctx, (md5_byte_t*)buf, sz);
    md5_finish(&md5ctx, md5_digest);

    u_tohex(out, (const char*)md5_digest, 16);

    out[MD5_DIGEST_LEN] = 0;

    return 0;
}

/**
 * \ingroup ut
 * \brief   Calculate the MD5 hash over an \c io_t stream
 *
 * Calculate the MD5 hash over an \c io_t stream \p io and place the result
 * as an hexadecimal string into \p out. 
 *
 * \param   io      the \c io_t stream to be hashed
 * \param   out     hexadecimal string containing the MD5 hash calculated over
 *                  \p buf.  It must be at least \c MD5_DIGEST_BUFSZ bytes long.
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
int u_md5io(io_t *io, char out[MD5_DIGEST_BUFSZ])
{
    enum { page_sz = 4096 };
    md5_state_t md5ctx;
    md5_byte_t md5_digest[16]; /* binary digest */
    char buf[page_sz];
    size_t cnt;

    dbg_err_if (io == NULL);
    dbg_err_if (out == NULL);

    md5_init(&md5ctx);

    while((cnt = io_read(io, buf, page_sz)) > 0)
        md5_append(&md5ctx, (md5_byte_t*)buf, cnt);

    md5_finish(&md5ctx, md5_digest);

    u_tohex(out, (const char*)md5_digest, 16);

    out[MD5_DIGEST_LEN] = 0;

    return 0;
err:
    return ~0;
}

int u_signal(int sig, u_sig_t handler)
{
#ifdef OS_WIN
    dbg_err_if(signal(sig, handler) == SIG_ERR);
#else
    struct sigaction action;
    sigset_t all;

    sigfillset(&all); 
    action.sa_mask = all;
    action.sa_handler = handler;

    /* disable child shell jobs notification */
    action.sa_flags = (sig == SIGCHLD ? SA_NOCLDSTOP : 0);
#ifdef HAVE_SA_RESTART
    action.sa_flags |= SA_RESTART;
#endif
    dbg_err_if(sigaction(sig, &action, (struct sigaction *) 0));
#endif

    return 0;
err:
    return ~0;
}                                                             

/**
 * \ingroup ut
 * \brief   Get the MIME type of a file
 *
 * Get the MIME type of the given file \p file_name by its extension
 *
 * \param   file_name   the path of the file
 *
 * \return the found MIME map, or the first map if no match could be found
 */
const mime_map_t *u_get_mime_map(const char *file_name)
{
    char *ext;
    mime_map_t *mm;

    dbg_goto_if (file_name == NULL, notfound);

    if((ext = strrchr(file_name, '.')) != NULL)
    {
        ++ext; /* skip '.' */
        /* FIXME binary search here */
        for(mm = mime_map; mm->ext && mm->mime_type; ++mm)
        {
            if(strcasecmp(mm->ext, ext) == 0)
                return mm;
        }
    }

notfound:
    return mime_map; /* the first item is the default */
}

/**
 * \ingroup ut
 * \brief   Guess the MIME type of a file
 *
 * Guess the MIME type of the given file \p file_name by its extension
 *
 * \param   file_name   the path of the file
 *
 * \return the string corresponding to the guessed MIME type, or
 *         "application/octet-stream" in case no map could be found
 */
const char *u_guess_mime_type(const char *file_name)
{
    char *ext;
    mime_map_t *mm;

    dbg_goto_if (file_name == NULL, notfound);
    
    if((ext = strrchr(file_name, '.')) != NULL)
    {
        ++ext; /* skip '.' */
        for(mm = mime_map; mm->ext && mm->mime_type; ++mm)
            if(strcasecmp(mm->ext, ext) == 0)
                return mm->mime_type;
    }

notfound:
    return "application/octet-stream";
}

#ifdef HAVE_LIBZ
/**
 * \ingroup ut
 * \brief   uncompress an HTML block and place it into an \c io_t object
 *
 * Uncompress the HTML block \p data of size \p sz and store the result into
 * the \c io_t object \p out.
 *
 * \param   out     the \c io_t object where the uncompressed HTML block resides
 * \param   data    the compressed HTML block
 * \param   sz      the size of \p data in bytes
 *
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
int u_io_unzip_copy(io_t *out, const char *data, size_t sz)
{
    codec_t *zip = NULL;
    io_t *ios = NULL;

    dbg_return_if (out == NULL, ~0);
    dbg_return_if (data == NULL, ~0);
    
    /* create an io_t around the HTML block */
    dbg_err_if(io_mem_create((char*)data, sz, 0, &ios));

    /* apply a gzip codec */
    dbg_err_if(codec_gzip_create(GZIP_UNCOMPRESS, &zip));
    dbg_err_if(io_codec_add_tail(ios, zip));
    zip = NULL; /* io_free() will free the codec */

    /* pipe ios to out */
    dbg_err_if(io_pipe(out, ios) < 0);

    io_free(ios);

    return 0;
err:
    if(zip)
        codec_free(zip);
    if(ios)
        io_free(ios);
    return ~0;
}
#endif

#ifdef SSL_OPENSSL
/**
 * \ingroup ut
 * \brief   Encrypt a given data block
 *
 * Encrypt the data block \p src of size \p ssz using the encryption algorithm
 * \p cipher, with key \p key and initialisation vector \p iv.  The result is
 * stored at \p dst, a preallocated buffer with a size of at least 
 * \c cipher_block_size + the length in bytes of \p src.
 * The length of the encrypted buffer is stored at \p *dcount.
 *
 * \param   cipher  an OpenSSL \c EVP_CIPHER
 * \param   key     encryption key string
 * \param   iv      initialisation vector
 * \param   dst     buffer holding the result 
 * \param   dcount  size in bytes of the result buffer
 * \param   src     the buffer to be encrypted
 * \param   ssz     size of \p src in bytes
 *
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
int u_cipher_encrypt(const EVP_CIPHER *cipher, unsigned char *key, 
    unsigned char *iv, char *dst, size_t *dcount, const char *src, size_t ssz)
{
    EVP_CIPHER_CTX ctx;
    ssize_t dlen = 0;  /* dst buffer length */
    int wr;

    dbg_return_if (cipher == NULL, ~0);
    dbg_return_if (key == NULL, ~0);
    dbg_return_if (iv == NULL, ~0);
    dbg_return_if (dcount == NULL, ~0);
    dbg_return_if (src == NULL, ~0);
    dbg_return_if (dst == NULL, ~0);
    
    /* init the context */
    EVP_CIPHER_CTX_init(&ctx);

    /* be sure that the cipher has been loaded */
    EVP_add_cipher(cipher);
    
    dbg_err_if(!EVP_EncryptInit_ex(&ctx, cipher, NULL, key, iv));

    dbg_err_if(!EVP_EncryptUpdate(&ctx, dst, &wr, src, ssz));
    dlen += wr;
    dst += wr;

    dbg_err_if(!EVP_EncryptFinal_ex(&ctx, dst, &wr));
    dlen += wr;

    *dcount = dlen; /* # of bytes written to dst */

    EVP_CIPHER_CTX_cleanup(&ctx);

    return 0;
err:
    EVP_CIPHER_CTX_cleanup(&ctx);
    return ~0;
}

/**
 * \ingroup ut
 * \brief   Decrypt a given data block
 *
 * Decrypt the data block \p src of size \p ssz using the encryption algorithm
 * \p cipher, with key \p key and initialisation vector \p iv.  The result is
 * stored at \p dst, with length \p dcount.
 *
 * \param   cipher  an OpenSSL \c EVP_CIPHER
 * \param   key     decryption key string
 * \param   iv      initialisation vector
 * \param   dst     buffer holding the result
 * \param   dcount  size in bytes of the result buffer
 * \param   src     the buffer to be decrypted
 * \param   ssz     size of \p src in bytes
 *
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
int u_cipher_decrypt(const EVP_CIPHER *cipher, unsigned char *key, 
    unsigned char *iv, char *dst, size_t *dcount, const char *src, size_t ssz)
{
    EVP_CIPHER_CTX ctx;
    ssize_t dlen = 0;  /* dst buffer length */
    int wr;

    dbg_return_if (cipher == NULL, ~0);
    dbg_return_if (key == NULL, ~0);
    dbg_return_if (iv == NULL, ~0);
    dbg_return_if (dcount == NULL, ~0);
    dbg_return_if (src == NULL, ~0);
    dbg_return_if (dst == NULL, ~0);

    /* init the context */
    EVP_CIPHER_CTX_init(&ctx);

    /* be sure that the cipher has been loaded */
    EVP_add_cipher(cipher);
    
    dbg_err_if(!EVP_DecryptInit_ex(&ctx, cipher, NULL, key, iv));

    dbg_err_if(!EVP_DecryptUpdate(&ctx, dst, &wr, src, ssz));
    dlen += wr;
    dst += wr;

    dbg_err_if(!EVP_DecryptFinal_ex(&ctx, dst, &wr));
    dlen += wr;

    *dcount = dlen; /* # of bytes written to dst */

    EVP_CIPHER_CTX_cleanup(&ctx);

    return 0;
err:
    EVP_CIPHER_CTX_cleanup(&ctx);
    return ~0;
}

#endif

#ifdef SSL_CYASSL
static int u_cipher_op(int op, const EVP_CIPHER *cipher, unsigned char *key, 
    unsigned char *iv, char *dst, size_t *dcount, const char *src, size_t ssz)
{
    io_t *io = NULL;
    codec_t *codec = NULL;
    size_t avail;
    ssize_t rd;

    dbg_return_if (cipher == NULL, ~0);
    dbg_return_if (key == NULL, ~0);
    dbg_return_if (iv == NULL, ~0);
    dbg_return_if (dcount == NULL, ~0);
    dbg_return_if (src == NULL, ~0);
    dbg_return_if (dst == NULL, ~0);
    dbg_return_if (*dcount < ssz, ~0);

    dbg_err_if(io_mem_create(src, ssz, 0, &io));

    dbg_err_if(codec_cipher_create(op, cipher, key, iv, &codec));
    dbg_err_if(io_codec_add_tail(io, codec));
    codec = NULL;

    avail = *dcount;
    *dcount = 0;

    while(avail > 0)
    {
        dbg_err_if((rd = io_read(io, dst, avail)) < 0);

        if(rd == 0)
            break; /* eof */

        dbg_err_if(avail < rd); /* dst too small */
        
        avail -= rd; 
        dst += rd; 
        *dcount += rd;
    }

    io_free(io);

    return 0;
err:
    if(codec)
        codec_free(codec);
    if(io)
        io_free(io);
    return ~0;
}

int u_cipher_encrypt(const EVP_CIPHER *cipher, unsigned char *key, 
    unsigned char *iv, char *dst, size_t *dcount, const char *src, size_t ssz)
{
    return u_cipher_op(CIPHER_ENCRYPT, cipher, key, iv, dst, dcount, src, ssz);
}

int u_cipher_decrypt(const EVP_CIPHER *cipher, unsigned char *key, 
    unsigned char *iv, char *dst, size_t *dcount, const char *src, size_t ssz)
{
    return u_cipher_op(CIPHER_DECRYPT, cipher, key, iv, dst, dcount, src, ssz);
}

#endif
