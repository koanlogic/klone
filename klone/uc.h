/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: uc.h,v 1.1 2007/08/07 17:13:09 tat Exp $
 */

#ifndef _KLONE_UC_H_
#define _KLONE_UC_H_

#ifdef __cplusplus
extern "C" {
#endif 

/* user-provided startup/term functions (--enable_uc configure arg is needed) */
void uc_init(void);
void uc_term(void);

#ifdef __cplusplus
}
#endif 

#endif

