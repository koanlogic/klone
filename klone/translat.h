/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: translat.h,v 1.8 2006/01/09 12:38:38 tat Exp $
 */

#ifndef _KLONE_TRANSLAT_H_
#define _KLONE_TRANSLAT_H_

#include <u/libu.h>
#include <klone/io.h>
#include <klone/codecs.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { URI_BUFSZ = 1024, MIME_BUFSZ = 256, EMSG_BUFSZ = 512 };

typedef struct trans_info_s
{
    char file_in[U_FILENAME_MAX], file_out[U_FILENAME_MAX];
    char uri[URI_BUFSZ], mime_type[MIME_BUFSZ];
    char key[CODEC_CIPHER_KEY_SIZE];
    char emsg[EMSG_BUFSZ];
    int comp;
    int encrypt;
    size_t file_size;
    time_t mtime;
} trans_info_t;

int translate(trans_info_t*);

int translate_script_to_c(io_t *in, io_t *out, trans_info_t* ti);
int translate_opaque_to_c(io_t *in, io_t *out, trans_info_t* ti);

#ifdef __cplusplus
}
#endif 

#endif
