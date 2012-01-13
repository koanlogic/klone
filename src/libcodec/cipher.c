/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: cipher.c,v 1.11 2007/10/26 08:57:59 tho Exp $
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/codec.h>
#include <klone/ccipher.h>
#include <klone/utils.h>

enum { CODEC_CIPHER_MAX_INPUT = 4096 /* max src block size for transform() */ };

typedef int (*EVP_Update_t)(EVP_CIPHER_CTX *ctx, unsigned char *out,
    int *outl, const unsigned char *in, int inl);
typedef int (*EVP_Final_ex_t)(EVP_CIPHER_CTX *ctx, unsigned char *out,
    int *outl);

struct codec_cipher_s
{
    codec_t codec;
    const EVP_CIPHER *cipher;   /* encryption cipher algorithm */
    EVP_CIPHER_CTX cipher_ctx;  /* encrypt context */
    char *cbuf;
    size_t coff, ccount, cbuf_size;
    EVP_Update_t update;        /* EVP_{Encrypt,Decrypt}Update func ptr */
    EVP_Final_ex_t final;       /* EVP_{Encrypt,Decrypt}Final_ex func ptr */
};

typedef struct codec_cipher_s codec_cipher_t;

static void codec_cbufcpy(codec_cipher_t *cc, char *dst, size_t *dcount)
{
    size_t count;

    count = U_MIN(*dcount, cc->ccount);

    memcpy(dst, cc->cbuf + cc->coff, count);
    cc->ccount -= count;
    if(cc->ccount)
        cc->coff += count;
    else
        cc->coff = 0;
    *dcount = count;
}

static ssize_t cipher_flush(codec_t *codec, char *dst, size_t *dcount)
{
    codec_cipher_t *cc;
    int wr;

    dbg_err_if (codec == NULL);
    dbg_err_if (dst == NULL);
    dbg_err_if (dcount == NULL);

    cc = (codec_cipher_t*)codec;
    
    for(;;)
    {
        if(cc->ccount)
        {
            codec_cbufcpy(cc, dst, dcount);
            return CODEC_FLUSH_CHUNK; /* call flush again */
        }

        if(cc->final)
        {
            wr = -1; /* just used to return an int value */
            dbg_err_if(!cc->final(&cc->cipher_ctx, cc->cbuf, &wr));

            cc->ccount += wr;
            cc->final = NULL; /* can be called just once */

            if(wr)
                continue;
        }
        break;
    }

    *dcount = 0;
    return CODEC_FLUSH_COMPLETE;
err:
    return -1;
}

static ssize_t cipher_transform(codec_t *codec, char *dst, size_t *dcount, 
        const char *src, size_t src_sz)
{
    codec_cipher_t *cc;
    ssize_t c;
    int wr;

    dbg_err_if (codec == NULL);
    dbg_err_if (src == NULL);
    dbg_err_if (dst == NULL); 
    dbg_err_if (dcount == NULL || *dcount == 0); 
    dbg_err_if (src_sz == 0);

    cc = (codec_cipher_t*)codec;

    c = 0;
    for(;;)
    {
        if(cc->ccount)
        {
            codec_cbufcpy(cc, dst, dcount);
            return c; /* consumed */
        }

        /* the cbuf must be empty because we need the whole buffer to be sure to
           have enough output space for EVP_{Encrypt,Decrypt}Update */

        c = U_MIN(src_sz, CODEC_CIPHER_MAX_INPUT);

        wr = -1; /* just used to return an int value */
        dbg_err_if(!cc->update(&cc->cipher_ctx, cc->cbuf, &wr, src, c));
        cc->ccount += wr;

        if(wr == 0)
        {
            *dcount = 0;
            break; /* cipher need more input to produce any output */
        }
    }

    dbg_err_if(c == 0 && *dcount == 0);
    return c;
err:
    return -1;
}

static int cipher_free(codec_t *codec)
{
    codec_cipher_t *cc;
       
    nop_return_if (codec == NULL, 0);   
        
    cc = (codec_cipher_t*)codec;

    U_FREE(cc->cbuf);
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
    codec_cipher_t *cc = NULL;

    dbg_return_if (cipher == NULL, ~0);
    dbg_return_if (key == NULL, ~0);
    /* iv can be NULL */
    dbg_return_if (pcc == NULL, ~0);

    cc = u_zalloc(sizeof(codec_cipher_t));
    dbg_err_if(cc == NULL);

    cc->codec.transform = cipher_transform;
    cc->codec.flush = cipher_flush;
    cc->codec.free = cipher_free;      

    cc->cipher = cipher;

    /* be sure that the cipher stuff is loaded */
    EVP_add_cipher(cc->cipher);

    EVP_CIPHER_CTX_init(&cc->cipher_ctx);

    cc->cbuf_size = CODEC_CIPHER_MAX_INPUT + 
        EVP_CIPHER_block_size(cc->cipher) -1;

    cc->cbuf = u_malloc(cc->cbuf_size);
    dbg_err_if(cc->cbuf == NULL);

    switch(op)
    {
    case CIPHER_ENCRYPT:
        dbg_err_if(!EVP_EncryptInit_ex(&cc->cipher_ctx, cc->cipher, NULL, 
            key, iv));
        cc->update = EVP_EncryptUpdate;
        cc->final = EVP_EncryptFinal_ex;
        break;
    case CIPHER_DECRYPT:
        dbg_err_if(!EVP_DecryptInit_ex(&cc->cipher_ctx, cc->cipher, NULL, 
            key, iv));
        cc->update = EVP_DecryptUpdate;
        cc->final = EVP_DecryptFinal_ex;
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
