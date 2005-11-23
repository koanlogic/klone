/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: os.h,v 1.6 2005/11/23 17:27:01 tho Exp $
 */

#ifndef _KLONE_OS_H_
#define _KLONE_OS_H_

#include "klone_conf.h"
#include <stdarg.h>

/* OS compatibility layer */
#ifdef OS_WIN
    #include <windows.h>
    #include <process.h>
    #include <io.h>
    #include <time.h>
    #include <stdio.h>
    #include <winsock2.h>

    typedef unsigned int uint;
    typedef void (*sig_t)(int);

    #ifndef isblank
    #define isblank(c) (c == ' ' || c == '\t')
    #endif

#elif defined(OS_UNIX)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/wait.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/un.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <unistd.h>
#else
    #error unsupported platform
#endif

#endif 

