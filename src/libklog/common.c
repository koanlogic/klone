/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: common.c,v 1.6 2006/01/09 12:38:38 tat Exp $
 */

#include "klone_conf.h"
#include <time.h>
#include <u/libu.h>
#include <klone/klog.h>
#include <klone/klogprv.h>

/* each klog_*_open() will push its private data afterwards */
int klog_new (int type, int threshold, const char *ident, klog_t **pkl)
{
    klog_t *kl;

    dbg_return_if (pkl == NULL, ~0);

    kl = u_zalloc(sizeof(klog_t));
    dbg_err_if (kl == NULL);

    kl->threshold = threshold;
    u_strlcpy(kl->ident, ident ? ident : "", sizeof kl->ident);

    /* check the supplied type */
    switch (type)
    {
        case KLOG_TYPE_MEM:
        case KLOG_TYPE_FILE:
        case KLOG_TYPE_SYSLOG:
            kl->type = type;
            break;
        default:
            warn_err("bad klog_t type !");
    }

    /* push out the klog_t object */
    *pkl = kl;
    
    return 0;
err:
    U_FREE(kl);
    return ~0;
}

const char *klog_to_str (int lev)
{
    return (lev < KLOG_DEBUG || lev > KLOG_EMERG) ? "?" : kloglev[lev];
}

