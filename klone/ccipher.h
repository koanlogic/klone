#ifndef _KLONE_CODEC_CIPHER_H__
#define _KLONE_CODEC_CIPHER_H__
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

#endif
