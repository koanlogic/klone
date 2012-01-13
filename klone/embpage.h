/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: embpage.h,v 1.7 2006/01/09 12:38:37 tat Exp $
 */

#ifndef _KLONE_EMB_PAGE_H_
#define _KLONE_EMB_PAGE_H_

#include <klone/page.h>
#include <u/libu.h>

#ifdef __cplusplus
extern "C" {
#endif 

void register_page(page_t *pg);
void unregister_page(page_t *pg);

#ifdef __cplusplus
}
#endif 

#endif
