#ifndef _KLONE_CODEC_CIPHER_H_
#define _KLONE_CODEC_CIPHER_H_
#include "klone_conf.h"
#include <klone/codec.h>

#ifdef HAVE_LIBOPENSSL
#include <openssl/evp.h>

/* possibile values for io_gzip_create */
enum { CIPHER_ENCRYPT, CIPHER_DECRYPT };

enum {
    CODEC_CIPHER_KEY_SIZE   = EVP_MAX_KEY_LENGTH, 
    CODEC_CIPHER_IV_SIZE    = EVP_MAX_IV_LENGTH,
    CODEC_CIPHER_BLOCK_SIZE = EVP_MAX_BLOCK_LENGTH
};

int codec_cipher_create(int op, const EVP_CIPHER *cipher, 
    unsigned char *key, unsigned char *iv, codec_t **pcc);
#else
/* to avoid ifdefs in local variable declaration (such vars will not be used 
   anyway because the code that use them is ifdef-out) */
enum {
    CODEC_CIPHER_KEY_SIZE   = 0, 
    CODEC_CIPHER_IV_SIZE    = 0,
    CODEC_CIPHER_BLOCK_SIZE = 0
};

#endif /* HAVE_LIBOPENSSL */

#endif /* !_KLONE_CODEC_CIPHER_H_ */
