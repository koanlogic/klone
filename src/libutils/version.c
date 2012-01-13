/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: version.c,v 1.7 2007/10/26 08:57:59 tho Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <klone/version.h>
#include <u/libu.h>

/**
 *  \addtogroup ut 
 *  \{
 *      \par
 */

/**
 * \brief   Return KLone version string (x.y.z)
 * 
 * Return KLone version string in the format x.y.z.
 *
 * \return the version string
 */
const char *klone_version(void)
{
    return KLONE_VERSION;
}

/**
 *  \}
 */
