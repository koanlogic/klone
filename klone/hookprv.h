/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: hookprv.h,v 1.1 2007/09/04 12:15:16 tat Exp $
 */

#ifndef _KLONE_HOOK_PRV_H_
#define _KLONE_HOOK_PRV_H_
#include <klone/hook.h>

#ifdef __cplusplus
extern "C" {
#endif 

#ifndef ENABLE_HOOKS
    #define hook_call( func, ... )
#else
    #define hook_call( func, ... ) \
        do { if(ctx && ctx->hook && ctx->hook->func) \
                ctx->hook->func( __VA_ARGS__ ); \
        } while(0)

struct hook_s
{
    /* server hooks function pointers */
    hook_server_init_t server_init;
    hook_server_term_t server_term;

    /* children hooks */
    hook_child_init_t child_init;
    hook_child_term_t child_term;

    /* per-connection hook */
    hook_request_t request;

    /* server loop hook */
    hook_server_loop_t server_loop;
};

#endif  /* ENABLE_HOOKS */

#ifdef __cplusplus
}
#endif 

#endif

