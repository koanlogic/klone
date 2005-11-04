#include "conf.h"
#include <stdlib.h>
#ifdef HAVE_STDINT
#include <stdint.h>
#endif /* HAVE_STDINT */
#include <klone/addr.h>
#include <klone/server.h>
#include <u/libu.h>

int addr_free(addr_t *a)
{
    u_free(a);
    return 0;
}

static int addr_ipv4_create(u_config_t *c, addr_t *addr)
{
    u_config_t *subkey;
    uint16_t portn;

    addr->type = ADDR_IPV4;

    /* set default values */
    memset(&addr->sa.sin, 0, sizeof(addr->sa.sin));
    addr->sa.sin.sin_family = AF_INET;
    addr->sa.sin.sin_addr.s_addr = htonl(INADDR_ANY);
    addr->sa.sin.sin_port = htons(80);

    /* use user-defined ip or port */
    if(!u_config_get_subkey(c, "ip", &subkey))
    {
        #ifdef OS_UNIX
        dbg_err_if(inet_pton(AF_INET, u_config_get_value(subkey), 
            &addr->sa.sin.sin_addr) <= 0);
        #else
        addr->sa.sin.sin_addr.s_addr  = inet_addr(u_config_get_value(subkey));
        #endif
    }

    if(!u_config_get_subkey(c, "port", &subkey))
    {
        portn = (uint16_t)atoi(u_config_get_value(subkey));
        addr->sa.sin.sin_port = htons(portn);
    }

    return 0;
err:
    return ~0;
}

int addr_set_from_sa(addr_t *addr, struct sockaddr *sa, size_t sz)
{
    u_unused_args(addr, sa, sz);

    return 0;
}

int addr_set_from_config(addr_t *addr, u_config_t *c)
{
    u_config_t *subkey;
    const char *type;

    dbg_err_if(strcasecmp(u_config_get_key(c), "addr"));

    dbg_err_if(u_config_get_subkey(c, "type", &subkey));

    type = u_config_get_value(subkey); /* IPv4, IPv6, unix, etc.  */

    if(!strcasecmp(type, "IPv4"))
        dbg_err_if(addr_ipv4_create(c, addr));
    else 
        dbg_err_if("Only IPv4 is supported");

    return 0;
err:
    return ~0;
}

int addr_create(addr_t **pa)
{
    addr_t *addr = NULL;

    addr = u_zalloc(sizeof(addr_t));
    dbg_err_if(addr == NULL);

    *pa = addr;

    return 0;
err:
    if(addr)
        addr_free(addr);
    return ~0;
}

