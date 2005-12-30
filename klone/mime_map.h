/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: mime_map.h,v 1.4 2005/12/30 17:21:53 tat Exp $
 */

#ifndef _KLONE_MIME_MAP_H_
#define _KLONE_MIME_MAP_H_
 
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mime_map_s
{
    const char *ext, *mime_type;
    int comp;   /* if >0 compression is recommended */
} mime_map_t;

extern mime_map_t mime_map[];

#ifdef __cplusplus
}
#endif 

#endif
