/*
 * (c)opyright 2005 - KoanLogic S.r.l.
 */

#include "conf.h"
#include <time.h>
#include <u/libu.h>
#include <klone/klog.h>
#include <klone/klogprv.h>

static int klog_to_syslog (int lev);

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
int klog_open_syslog (klog_t *kl, const char *ident, int facility, int logopt)
{
    klog_syslog_t *kls = NULL;
    
    dbg_return_if (kl == NULL, ~0);

    kls = u_zalloc(sizeof(klog_syslog_t));
    dbg_err_if (kls == NULL);

    kls->ident = ident ? u_strdup(ident) : u_strdup("");
    kls->facility = facility;
    kls->logopt = logopt;
    
    /* stick child to its parent */
    kl->u.s = kls, kls = NULL;

    return 0;
err:
    klog_close_syslog(kls);
    return ~0;
}

void klog_close_syslog (klog_syslog_t *kls)
{
    dbg_return_if (kls == NULL, );
    
    u_free(kls->ident);
    u_free(kls);
     
    return;
}

int klog_syslog (klog_syslog_t *kls, int level, const char *fmt, va_list ap)
{
    dbg_return_if (kls == NULL, ~0);
    dbg_return_if (fmt == NULL, ~0);
    
#ifdef HAVE_SYSLOG
    openlog(kls->ident, kls->logopt, kls->facility);
    vsyslog(klog_to_syslog(level), fmt, ap);
    closelog();
#else
    vsyslog(kls->facility | klog_to_syslog(level), fmt, ap);
#endif /* HAVE_SYSLOG */

    return 0;
}

static int klog_to_syslog (int lev)
{
    return (lev < KLOG_DEBUG || lev > KLOG_EMERG) ? -1 : sysloglev[lev];
}
