/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: rsfilter.h,v 1.8 2007/12/03 16:05:55 tat Exp $
 */

#ifndef _KLONE_RSFILTER_H_
#define _KLONE_RSFILTER_H_

#include <klone/codec.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the filter will buffer the first RFBUFSZ bytes printed so page developers
 * can postpone header modifications (i.e. header will be sent after RFBUFSZ
 * bytes of printed data or on io_flush()
 */
enum { RFBUFSZ = 4096 };

struct response_s;
struct request_s;
struct session_s;
typedef struct response_filter_s response_filter_t;

int response_filter_create(struct request_s *rq, struct response_s *rs, 
    struct session_s *ss, codec_t **prf);
int response_filter_feeded(codec_t *codec);

#ifdef __cplusplus
}
#endif 

#endif
