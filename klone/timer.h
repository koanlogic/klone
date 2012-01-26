/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: timer.h,v 1.5 2006/04/22 13:59:01 tat Exp $
 */

#ifndef _KLONE_TIMERM_H_
#define _KLONE_TIMERM_H_

#ifdef __cplusplus
extern "C" {
#endif

struct timerm_s;
typedef struct timerm_s timerm_t;

struct talarm_s;
typedef struct talarm_s talarm_t;

typedef int (*talarm_cb_t)(talarm_t *, void *arg);

int timerm_add(int secs, talarm_cb_t cb, void *arg, talarm_t **pa);
int timerm_del(talarm_t *a);
int timerm_reschedule(talarm_t *a, int secs, talarm_cb_t cb, void *arg);

#ifdef __cplusplus
}
#endif 

#endif
