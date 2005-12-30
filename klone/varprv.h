/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: varprv.h,v 1.6 2005/12/30 17:21:53 tat Exp $
 */

#ifndef _KLONE_VAR_PRV_H_
#define _KLONE_VAR_PRV_H_

#include <u/libu.h>

#ifdef __cplusplus
extern "C" {
#endif

struct var_s
{
    TAILQ_ENTRY(var_s) np;  /* next & prev pointers   */
    u_string_t *sname;      /* var string name        */
    u_string_t *svalue;     /* var string value       */
    char *data;
    size_t size;
};

#ifdef __cplusplus
}
#endif 

#endif
