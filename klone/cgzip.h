/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: cgzip.h,v 1.7 2006/01/09 12:38:37 tat Exp $
 */

#ifndef _KLONE_CODEC_GZIP_H_
#define _KLONE_CODEC_GZIP_H_

#ifdef __cplusplus
extern "C" {
#endif 

/* the codec [un]compresses (using libz) the stream to whom it's applied */

/* possibile values for io_gzip_create */
enum { GZIP_COMPRESS, GZIP_UNCOMPRESS };

int codec_gzip_create(int operation, codec_t **pioz);

#ifdef __cplusplus
}
#endif 

#endif
