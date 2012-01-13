/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: run.h,v 1.5 2006/01/09 12:38:38 tat Exp $
 */

#ifndef _KLONE_RUN_H_
#define _KLONE_RUN_H_

#include <klone/request.h>
#include <klone/response.h>

#ifdef __cplusplus
extern "C" {
#endif

/* run a dynamic object page */
int run_page(const char *, request_t*, response_t*);

#ifdef __cplusplus
}
#endif 

#endif
