/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: varprv.h,v 1.5 2005/11/23 17:27:01 tho Exp $
 */

#ifndef _KLONE_VAR_PRV_H_
#define _KLONE_VAR_PRV_H_

#include <u/libu.h>

struct var_s
{
    TAILQ_ENTRY(var_s) np;  /* next & prev pointers   */
    u_string_t *sname;      /* var string name        */
    u_string_t *svalue;     /* var string value       */
    char *data;
    size_t size;
};

#endif
