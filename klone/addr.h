/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: addr.h,v 1.15 2008/06/04 17:48:01 tat Exp $
 */

#ifndef _KLONE_ADDR_H_
#define _KLONE_ADDR_H_

#include <sys/types.h>

#include <u/libu.h>

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct kaddr_s
{
    enum type_e { ADDR_IPV4, ADDR_IPV6, ADDR_UNIX } type;
    union
    {
        struct sockaddr_in  sin;
#ifndef NO_IPV6
        struct sockaddr_in6 sin6;
#endif
#ifndef NO_UNIXSOCK
        struct sockaddr_un  sunx;
#endif
    } sa;
} kaddr_t;

int addr_create(kaddr_t **pa);
int addr_set_from_config(kaddr_t *a, u_config_t *c);
int addr_set_from_sa(kaddr_t *a, struct sockaddr *sa, size_t sz);
int addr_set(kaddr_t *a, const char *ip, int port);
int addr_set_ipv4_ip(kaddr_t *a, const char *ip);
int addr_set_ipv4_port(kaddr_t *a, int port);
int addr_free(kaddr_t *a);

#ifdef __cplusplus
}
#endif 

#endif
