/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: rsfilter.h,v 1.4 2005/11/23 17:27:01 tho Exp $
 */

#ifndef _KLONE_RSFILTER_H_
#define _KLONE_RSFILTER_H_

#include <klone/codec.h>

/* the filter will buffer the first RFBUFSZ bytes printed so page developers
 * can postpone header modifications (i.e. header will be sent after RFBUFSZ
 * bytes of printed data or on io_flush()
 */
enum { RFBUFSZ = 4096 };

struct response_s;
typedef struct response_filter_s response_filter_t;

int response_filter_create(struct response_s *rs, codec_t **prf);

#endif
