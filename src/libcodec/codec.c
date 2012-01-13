/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: codec.c,v 1.12 2007/10/26 11:21:51 tho Exp $
 */

#include <klone/codec.h>
#include <u/libu.h>

/**
 * \ingroup filters
 * \brief   Dispose all the resources allocated to the supplied codec
 *
 * Dispose all the resources allocated to the supplied \p codec
 *
 * \param   codec   the \c codec_t object to be disposed
 *
 * \return  always successful, i.e. \c 0
 */
int codec_free(codec_t *codec)
{
    if(codec)
    {
        if(codec->free)
            codec->free(codec);
        else
            U_FREE(codec);
    }
    return 0;
}
