/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: embpage.h,v 1.5 2005/11/23 17:27:01 tho Exp $
 */

#ifndef _KLONE_EMB_PAGE_H_
#define _KLONE_EMB_PAGE_H_

#include <klone/page.h>
#include <u/libu.h>

void register_page(page_t *pg);
void unregister_page(page_t *pg);

#endif
