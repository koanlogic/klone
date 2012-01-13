/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/codec.h>
#include <klone/ccipher.h>
#include <klone/utils.h>
#include <config.h>
#include <types.h>
#include <ctc_aes.h>

typedef void (*aes_op_t)(Aes*, byte*, const byte*, word32);

struct codec_cipher_s
{
    codec_t codec;
    const EVP_CIPHER *cipher;   /* encryption cipher algorithm */
    u_buf_t *ubuf;
    Aes aes;
    aes_op_t op;
    int outready;
    size_t off;
};


typedef struct codec_cipher_s codec_cipher_t;

static ssize_t cipher_flush(codec_t *codec, char *dst, size_t *dcount)
{
    codec_cipher_t *cc;
    size_t sz, len, orig_len, c;
    char *ptr, pad;
    char padding[AES_BLOCK_SIZE];
       
    cc = (codec_cipher_t*)codec;

    if(cc->outready == 0)
    {
        if(cc->op == AesCbcEncrypt)
        {
            len = (u_buf_len(cc->ubuf)/AES_BLOCK_SIZE + 1) * AES_BLOCK_SIZE;
            u_buf_reserve(cc->ubuf, len);

            pad = len - u_buf_len(cc->ubuf);

            /* PKCS padding */
            memset(padding, pad, AES_BLOCK_SIZE);
            dbg_err_if(u_buf_append(cc->ubuf, padding, 
                        len-u_buf_len(cc->ubuf)));

            cc->op(&cc->aes, u_buf_ptr(cc->ubuf),
                u_buf_ptr(cc->ubuf), u_buf_len(cc->ubuf));

        } else {

            crit_err_if(u_buf_len(cc->ubuf) % AES_BLOCK_SIZE != 0);

            cc->op(&cc->aes, u_buf_ptr(cc->ubuf),
                u_buf_ptr(cc->ubuf), u_buf_len(cc->ubuf));

            ptr = u_buf_ptr(cc->ubuf);

            pad = ptr[u_buf_len(cc->ubuf) -1];

            /* wrong key? */
            dbg_err_if(pad < 1 || pad > 16);

            /* length of the buffer without padding */
            len = u_buf_len(cc->ubuf) - pad;

            /* verify that the padding data is what we expect; if not
             * then the packet has been corrupted or the key is wrong */
            for(c = len; c < u_buf_len(cc->ubuf); c++) 
                dbg_err_if(ptr[c] != pad);

            dbg_err_if(u_buf_shrink(cc->ubuf, len));
        }

        cc->outready++;
    }

    sz = U_MIN(*dcount, u_buf_len(cc->ubuf) - cc->off);
    if(sz)
    {
        ptr = (char*)u_buf_ptr(cc->ubuf) + cc->off; 
        memcpy(dst, ptr, sz);
        cc->off += sz;
    }

    *dcount = sz;

    return *dcount == 0 ? CODEC_FLUSH_COMPLETE : CODEC_FLUSH_CHUNK;
err:
    if(dcount)
        *dcount = 0;
    return -1;
}

static ssize_t cipher_transform(codec_t *codec, char *dst, size_t *dcount, 
        const char *src, size_t src_sz)
{
    codec_cipher_t *cc;
       
    cc = (codec_cipher_t*)codec;
    
    dbg_err_if (src == NULL);
    dbg_err_if (dst == NULL);
    dbg_err_if (dcount == NULL || *dcount == 0);
    dbg_err_if (src_sz == 0);

    *dcount = 0; /* zero bytes written out */

    /* stream functions aren't available in cyassl so we must buffer all data
     * and emit the ciphertext on cipher_flush() */
    dbg_err_if(u_buf_append(cc->ubuf, src, src_sz));

    return src_sz; /* all input bytes consumed */
err:
    return -1;
}

static int cipher_free(codec_t *codec)
{
    codec_cipher_t *cc;
       
    nop_return_if (codec == NULL, 0);   
        
    cc = (codec_cipher_t*)codec;

    if(cc->ubuf)
        u_buf_free(cc->ubuf);

    U_FREE(cc);

    return 0;
}

/**
 *  \addtogroup filters
 *  \{
 */

/**
 * \brief   Create a cipher \c codec_t object 
 *
 * Create a cipher \c codec_t object at \p *pcc suitable for encryption or
 * decryption (depending on \p op).  The \p cipher, \p key and \p iv parameters
 * hold the algorithm, key and initialisation vector respectively, used for
 * the data transforms.
 *
 * \param   op      one of \c CIPHER_ENCRYPT or \c CIPHER_DECRYPT
 * \param   cipher  an OpenSSL \c EVP_CIPHER object 
 * \param   key     the encryption/decryption key
 * \param   iv      the initialisation vector
 * \param   pcc     the created codec as a value-result arguement
 *
 * \return \c 0 on success, \c ~0 otherwise
 */
int codec_cipher_create(int op, const EVP_CIPHER *cipher, 
    unsigned char *key, unsigned char *iv, codec_t **pcc)
{
    static unsigned char zero_iv[CODEC_CIPHER_IV_LEN]; /* all zeros */
    codec_cipher_t *cc = NULL;

    dbg_return_if (cipher == NULL, ~0);
    dbg_return_if (key == NULL, ~0);
    /* iv can be NULL */
    dbg_return_if (pcc == NULL, ~0);

    if(iv == NULL)
        iv = zero_iv; /* iv must be set in AesSetKey */

    cc = u_zalloc(sizeof(codec_cipher_t));
    dbg_err_if(cc == NULL);

    cc->codec.transform = cipher_transform;
    cc->codec.flush = cipher_flush;
    cc->codec.free = cipher_free;      

    cc->cipher = cipher;

    dbg_err_if(u_buf_create(&cc->ubuf));

    /* key lentgh is 256 bits so aes256 will be used */
    switch(op)
    {
    case CIPHER_ENCRYPT:
        dbg_err_if(AesSetKey(&cc->aes, key, CODEC_CIPHER_KEY_LEN, iv, 
                    AES_ENCRYPTION));

        cc->op = AesCbcEncrypt;
        break;
    case CIPHER_DECRYPT:
        dbg_err_if(AesSetKey(&cc->aes, key, CODEC_CIPHER_KEY_LEN, iv, 
                    AES_DECRYPTION));
        cc->op = AesCbcDecrypt;
        break;
    default:
        dbg_err_if("bad cipher op");
    }


    *pcc = (codec_t*)cc;

    return 0;
err:
    U_FREE(cc);
    return ~0;
}

/**
 *  \}
 */
