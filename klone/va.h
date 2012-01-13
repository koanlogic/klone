/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: va.h,v 1.2 2008/10/03 16:03:04 tho Exp $
 */

#ifndef _KLONE_VA_H_
#define _KLONE_VA_H_

#include "klone_conf.h"
#include <u/libu.h>
#include <stdarg.h>

/* provide some suitable method for performing C99 va_copy */
#if defined(va_copy)
    /* C99 va_copy */
    #define kl_va_copy(a, b)  va_copy(a, b)
#elif defined(__va_copy)
    /* GNU libc va_copy replacement */
    #define kl_va_copy(a, b)  __va_copy(a, b)
#else
    #define VA_COPY_UNAVAIL 1
#endif

#endif  /* !_KLONE_VA_H_ */
