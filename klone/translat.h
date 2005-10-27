#ifndef _TRANSLAT_H_
#define _TRANSLAT_H_
#include <limits.h>
#include <klone/io.h>
#include <klone/codecs.h>

enum { 
    URI_BUFSZ = 1024, MIME_BUFSZ = 256, 
    NAME_BUFSZ = PATH_MAX + 
#ifdef OS_UNIX
        NAME_MAX
#else
        FILENAME_MAX
#endif
};

typedef struct trans_info_s
{
    char file_in[NAME_BUFSZ], file_out[NAME_BUFSZ];
    char uri[URI_BUFSZ], mime_type[MIME_BUFSZ];
    char key[CODEC_CIPHER_KEY_SIZE];
    int comp;
    int encrypt;
    size_t file_size;
    time_t mtime;
} trans_info_t;

int translate(trans_info_t*);

int translate_script_to_c(io_t *in, io_t *out, trans_info_t* ti);
int translate_opaque_to_c(io_t *in, io_t *out, trans_info_t* ti);

#endif
