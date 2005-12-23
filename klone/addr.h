/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: addr.h,v 1.7 2005/12/23 10:14:57 tat Exp $
 */

#ifndef _KLONE_ADDR_H_
#define _KLONE_ADDR_H_

#include <u/libu.h>

typedef struct addr_s
{
    enum type_e { ADDR_IPV4, ADDR_IPV6, ADDR_UNIX } type;
    union
    {
        struct sockaddr_in  sin;
        struct sockaddr_in6 sin6;
#ifdef OS_UNIX
        struct sockaddr_un  sun;
#endif
    } sa;
} addr_t;

int addr_create(addr_t **pa);
int addr_set_from_config(addr_t *a, u_config_t *c);
int addr_set_from_sa(addr_t *a, struct sockaddr *sa, size_t sz);
int addr_set_ip(addr_t *a, const char *ip, int port);
int addr_free(addr_t *a);

#endif
