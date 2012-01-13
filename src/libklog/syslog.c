/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: syslog.c,v 1.7 2006/01/09 12:38:38 tat Exp $
 */

#include "klone_conf.h"
#include <time.h>
#include <u/libu.h>
#include <klone/klog.h>
#include <klone/klogprv.h>

static int klog_to_syslog (int lev);
static void klog_free_syslog (klog_syslog_t *kls);

static int klog_syslog (klog_t *kl, int level, const char *fmt, va_list ap);
static void klog_close_syslog (klog_t *kl);

/* must be ordered */
static int sysloglev[] =
{
    LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING, 
    LOG_ERR, LOG_CRIT, LOG_ALERT, LOG_EMERG
};  

/* we've a problem here because syslog(3) mantains a global context, and
 * we can't bind some syslog state specific to the supplied klog_t object.
 * instead we are forced to do openlog->vsyslog->closelog for each logged
 * message which is rather silly ... anyway this should be not so heavy as
 * it seems: in fact (a) the underlying transport is connectionless (UDP) 
 * and (b) most of the time is spent inside the vsyslog call. */
int klog_open_syslog (klog_t *kl, int fac, int logopt)
{
    klog_syslog_t *kls = NULL;
    
    dbg_return_if (kl == NULL, ~0);

    kls = u_zalloc(sizeof(klog_syslog_t));
    dbg_err_if (kls == NULL);

    kls->facility = fac;
    kls->logopt = logopt;
    
    /* set private methods */
    kl->cb_log = klog_syslog;
    kl->cb_close = klog_close_syslog;
    kl->cb_getln = NULL;
    kl->cb_countln = NULL;
    kl->cb_clear = NULL;
    kl->cb_flush = NULL;

    /* stick child to its parent */
    kl->u.s = kls, kls = NULL;

    return 0;
err:
    if (kls)
        klog_free_syslog(kls);
    return ~0;
}

static void klog_close_syslog (klog_t *kl)
{
    if (kl == NULL || kl->type != KLOG_TYPE_SYSLOG || kl->u.s == NULL)
        return;

    klog_free_syslog(kl->u.s), kl->u.s = NULL;
 
    return;
}

static void klog_free_syslog (klog_syslog_t *kls)
{
    if (kls == NULL)
        return;
    U_FREE(kls);
    return;
}

static int klog_syslog (klog_t *kl, int level, const char *fmt, va_list ap)
{
    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (kl->type != KLOG_TYPE_SYSLOG, ~0);
    dbg_return_if (kl->u.s == NULL, ~0);
    dbg_return_if (fmt == NULL, ~0);
    
#ifdef HAVE_SYSLOG
    openlog(kl->ident, kl->u.s->logopt, kl->u.s->facility);
    vsyslog(klog_to_syslog(level), fmt, ap);
    closelog();
#else
    vsyslog(kl->u.s->facility | klog_to_syslog(level), fmt, ap);
#endif /* HAVE_SYSLOG */

    return 0;
}

static int klog_to_syslog (int lev)
{
    return (lev < KLOG_DEBUG || lev > KLOG_EMERG) ? -1 : sysloglev[lev];
}
