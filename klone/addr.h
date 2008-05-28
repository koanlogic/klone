/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: addr.h,v 1.12 2008/05/28 16:38:30 tho Exp $
 */

#ifndef _KLONE_ADDR_H_
#define _KLONE_ADDR_H_

#include <sys/types.h>
#include <netinet/in.h>

#include <u/libu.h>

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct addr_s
{
    enum type_e { ADDR_IPV4, ADDR_IPV6, ADDR_UNIX } type;
    union
    {
        struct sockaddr_in  sin;
#ifndef NO_IPV6
        struct sockaddr_in6 sin6;
#endif
#ifndef NO_UNIXSOCK
        struct sockaddr_un  sun;
#endif
    } sa;
} addr_t;

int addr_create(addr_t **pa);
int addr_set_from_config(addr_t *a, u_config_t *c);
int addr_set_from_sa(addr_t *a, struct sockaddr *sa, size_t sz);
int addr_set(addr_t *a, const char *ip, int port);
int addr_set_ipv4_ip(addr_t *a, const char *ip);
int addr_set_ipv4_port(addr_t *a, int port);
int addr_free(addr_t *a);

#ifdef __cplusplus
}
#endif 

#endif
