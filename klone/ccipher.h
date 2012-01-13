/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ccipher.h,v 1.8 2006/01/09 12:38:37 tat Exp $
 */

#ifndef _KLONE_CODEC_CIPHER_H_
#define _KLONE_CODEC_CIPHER_H_

#include "klone_conf.h"
#include <klone/codec.h>

#ifdef SSL_ON
#include <openssl/evp.h>

#ifdef __cplusplus
extern "C" {
#endif 

#ifdef SSL_CYASSL
#define EVP_MAX_KEY_LENGTH      32
#define EVP_MAX_IV_LENGTH       16
#define EVP_MAX_BLOCK_LENGTH        32
#define EVP_aes_256_cbc()       "AES-CBC-256"

#endif

/* possibile values for io_gzip_create */
enum { CIPHER_ENCRYPT, CIPHER_DECRYPT };

enum {
    CODEC_CIPHER_KEY_LEN = EVP_MAX_KEY_LENGTH, 
    CODEC_CIPHER_KEY_BUFSZ = 2 * EVP_MAX_KEY_LENGTH, 
    CODEC_CIPHER_IV_LEN = EVP_MAX_IV_LENGTH,
    CODEC_CIPHER_BLOCK_LEN = EVP_MAX_BLOCK_LENGTH,
};

int codec_cipher_create(int op, const EVP_CIPHER *cipher, 
    unsigned char *key, unsigned char *iv, codec_t **pcc);

#ifdef __cplusplus
}
#endif 

#else /* SSL_ON */
/* to avoid ifdefs in local variable declaration (such vars will not be used 
   anyway because the code that use them is ifdef-out) */
enum {
    CODEC_CIPHER_KEY_LEN = 0, 
    CODEC_CIPHER_KEY_BUFSZ = 0, 
    CODEC_CIPHER_IV_LEN = 0
};

#endif /* SSL_ON */

#endif /* !_KLONE_CODEC_CIPHER_H_ */
