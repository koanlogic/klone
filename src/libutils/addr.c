/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: addr.c,v 1.19 2008/06/04 17:48:02 tat Exp $
 */

#include "klone_conf.h"
#include <stdlib.h>
#ifdef HAVE_STDINT
#include <stdint.h>
#endif /* HAVE_STDINT */
#include <klone/addr.h>
#include <klone/server.h>
#include <u/libu.h>

int addr_free(kaddr_t *a)
{
    U_FREE(a);
    return 0;
}

static int addr_ipv4_create(u_config_t *c, kaddr_t *addr)
{
    u_config_t *subkey;
    int portn;

    dbg_return_if (c == NULL, ~0);
    dbg_return_if (addr == NULL, ~0);
    
    /* set default values */
    addr->type = ADDR_IPV4;
    addr->sa.sin.sin_family = AF_INET;

    /* use user-defined ip or port */
    if(!u_config_get_subkey(c, "ip", &subkey))
    {
#ifdef OS_UNIX
        warn_err_ifm(inet_pton(AF_INET, u_config_get_value(subkey), 
            &addr->sa.sin.sin_addr) <= 0, 
                "bad '<servname>.addr.ip' value");
#else
        addr->sa.sin.sin_addr.s_addr = inet_addr(u_config_get_value(subkey));
        warn_err_ifm(addr->sa.sin.sin_addr.s_addr == INADDR_NONE,
                "bad '<servname>.addr.ip' value");
#endif
    }

    if(!u_config_get_subkey(c, "port", &subkey))
    {
        portn = atoi(u_config_get_value(subkey));
        warn_err_ifm(portn < 1 || portn > 65535, "port out of range");
        addr->sa.sin.sin_port = htons(portn);
    }

    return 0;
err:
    return ~0;
}

static int addr_is_ipv4(const char *ip)
{
    size_t i, len = strlen(ip);

    dbg_return_if (ip == NULL, 0);

    /* assume ip it's of type IPv4 if it contains a dot '.' */
    for(i = 0; i < len; ++i)
        if(ip[i] == '.')
            return 1;
    
    return 0;
}

int addr_set_ipv4_port(kaddr_t *addr, int port)
{
    dbg_return_if (addr == NULL, ~0);
    dbg_return_if (port == 0, ~0);

    addr->sa.sin.sin_port = htons(port);

    return 0;
}

int addr_set_ipv4_ip(kaddr_t *addr, const char *ip)
{
    dbg_return_if (addr == NULL, ~0);
    dbg_return_if (ip == NULL, ~0);

    addr->type = ADDR_IPV4;

    /* set default values */
    memset(&addr->sa.sin, 0, sizeof(addr->sa.sin));
    addr->sa.sin.sin_family = AF_INET;
    addr->sa.sin.sin_addr.s_addr = inet_addr(ip);

    return 0;
}

int addr_set(kaddr_t *addr, const char *ip, int port)
{
    dbg_return_if (addr == NULL, ~0);
    dbg_return_if (ip == NULL, ~0);
    dbg_return_if (port == 0, ~0);

    if(addr_is_ipv4(ip))
    {
        addr->type = ADDR_IPV4;

        /* set default values */
        memset(&addr->sa.sin, 0, sizeof(addr->sa.sin));
        addr->sa.sin.sin_family = AF_INET;
        addr->sa.sin.sin_addr.s_addr = inet_addr(ip);
        addr->sa.sin.sin_port = htons(port);
    } else {
        return ~0; /* FIXME IPv6 */
    }

    return 0;
}

int addr_set_from_sa(kaddr_t *addr, struct sockaddr *sa, size_t sz)
{
    dbg_return_if (addr == NULL, ~0);
    dbg_return_if (sa == NULL, ~0);
    
    switch(sz)
    {
    case sizeof(struct sockaddr_in):
        addr->type = ADDR_IPV4;
        memcpy(&addr->sa.sin, sa, sz);
        break;
#ifndef NO_IPV6
    case sizeof(struct sockaddr_in6):
        addr->type = ADDR_IPV6;
        memcpy(&addr->sa.sin6, sa, sz);
        break;
#endif
#ifndef NO_UNIXSOCK
    case sizeof(struct sockaddr_un):
        addr->type = ADDR_UNIX;
        memcpy(&addr->sa.sunx, sa, sz);
        break;
#endif  
    default:
        u_dbg("bad sockaddr size");
        return ~0;
    }

    return 0;
}

int addr_set_from_config(kaddr_t *addr, u_config_t *c)
{
    u_config_t *subkey;
    const char *type;

    dbg_return_if (addr == NULL, ~0);
    dbg_return_if (c == NULL, ~0);
    
    dbg_err_if(strcasecmp(u_config_get_key(c), "addr"));

    warn_err_ifm(u_config_get_subkey(c, "type", &subkey),
        "missing or bad '<servname>.addr.type' value");

    type = u_config_get_value(subkey); /* IPv4, IPv6, unix, etc.  */

    if(!strcasecmp(type, "IPv4"))
        dbg_err_if(addr_ipv4_create(c, addr));
    else 
        warn_err_if("bad '<servname>.addr.type', only IPv4 is supported");

    return 0;
err:
    return ~0;
}

int addr_create(kaddr_t **pa)
{
    kaddr_t *addr = NULL;

    dbg_return_if (pa == NULL, ~0);
    
    addr = u_zalloc(sizeof(kaddr_t));
    dbg_err_if(addr == NULL);

    /* set default ipv4 values */
    memset(&addr->sa.sin, 0, sizeof(addr->sa.sin));
    addr->sa.sin.sin_family = AF_INET;
    addr->sa.sin.sin_addr.s_addr = htonl(INADDR_ANY);
    addr->sa.sin.sin_port = htons(0);

    *pa = addr;

    return 0;
err:
    if(addr)
        addr_free(addr);
    return ~0;
}
