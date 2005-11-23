/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: timer.h,v 1.2 2005/11/23 17:27:01 tho Exp $
 */

#ifndef _KLONE_TIMERM_H_
#define _KLONE_TIMERM_H_

struct timerm_s;
typedef struct timerm_s timerm_t;

struct alarm_s;
typedef struct alarm_s alarm_t;

typedef int (*alarm_cb_t)(alarm_t *, void *arg);

int timerm_add(int secs, alarm_cb_t cb, void *arg, alarm_t **pa);
int timerm_del(alarm_t *a);

#endif
