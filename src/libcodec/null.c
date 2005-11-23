/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: null.c,v 1.9 2005/11/23 17:44:16 stewy Exp $
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <klone/codec.h>
#include <klone/cnull.h>
#include <klone/utils.h>

/**
 *  \addtogroup codec_t
 *  \{
 */

typedef struct codec_null_s
{
    codec_t codec;
} codec_null_t;

static ssize_t null_flush(codec_t *cn, char *dst, size_t *dcount)
{
    u_unused_args(cn, dst);
    *dcount = 0;
    return CODEC_FLUSH_COMPLETE;
}

static ssize_t null_transform(codec_t *cn, char *dst, size_t *dcount, 
        const char *src, size_t src_sz)
{
    ssize_t wr;
    
    dbg_err_if(src == NULL || dst == NULL || *dcount == 0 || src_sz == 0);

    u_unused_args(cn);

    wr = MIN(src_sz, *dcount); 
    memcpy(dst, src, wr);
    *dcount = wr;

    dbg_err_if(wr == 0);
    return wr;
err:
    return -1;
}

static int null_free(codec_t *cn)
{
    u_free(cn);

    return 0;
}

/** 
 * \brief   Create a cipher \c codec_t object 
 *      
 * Create a null \c codec_t object at \p *pcn.
 *  
 * \param   pcn     the created codec as a value-result arguement
 *  
 * \return \c 0 on success, \c ~0 otherwise
 */ 
int codec_null_create(codec_t **pcn)
{
    codec_null_t *cn = NULL;

    cn = u_zalloc(sizeof(codec_null_t));
    dbg_err_if(cn == NULL);

    cn->codec.transform = null_transform;
    cn->codec.flush = null_flush;
    cn->codec.free = null_free;      

    *pcn = (codec_t*)cn;

    return 0;
err:
    if(cn)
        u_free(cn);
    return ~0;
}

/**
 *  \}
 */
