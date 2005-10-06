/*
 * (c)opyright 2005 - KoanLogic S.r.l.
 */

#include "conf.h"
#include <time.h>
#include <klone/klog.h>
#include <klone/debug.h>
#include <klone/utils.h>

static int klog_new (int type, klog_t **pkl);
static int klog_to_syslog (int lev);
static const char *klog_to_str (int lev);
static int klog_mem (klog_mem_t *klm, int level, const char *fmt, va_list ap);
static int klog_mem_msg_new (const char *s, int level, klog_mem_msg_t **pmmsg);
static int klog_mem_msg_push (klog_mem_t *klm, klog_mem_msg_t *mmsg);
static void klog_mem_msg_free (klog_mem_msg_t *mmsg);
static int klog_file (klog_file_t *klm, int level, const char *fmt, va_list ap);
static int klog_syslog (klog_syslog_t *klm, int level, const char *fmt, 
        va_list ap);
static int klog_open_mem (const char *id, size_t ln_max, klog_t **pkl);
static int klog_open_syslog (const char *ident, int facility, int logopt, 
        klog_t **pkl);
static void klog_close_mem (klog_mem_t *klm);
static void klog_close_file (klog_file_t *klf);
static void klog_close_syslog (klog_syslog_t *kls);
static void klog_mem_msg_free (klog_mem_msg_t *mmsg);
static ssize_t klog_countln_mem (klog_mem_t *klm);
static int klog_getln_mem (klog_mem_t *klm, size_t nth, char ln[]);
static int klog_clear_mem (klog_mem_t *klm);
static void klog_mem_msgs_free (klog_mem_t *klm);


int klog_open (int type, const char *id, int facility, int opt, size_t bound,
        klog_t **pkl)
{
    int rv;

    dbg_return_if (pkl == NULL, ~0);

    switch (type)
    {
        case KLOG_TYPE_MEM:
            rv = klog_open_mem(id, bound, pkl);
            break;
        case KLOG_TYPE_FILE: /* TODO */
            rv = ~0;
            break;
        case KLOG_TYPE_SYSLOG:
            rv = klog_open_syslog(id, facility, opt, pkl);
            break;
        default:
            return ~0;
    }

    return rv;
}

/* each klog_*_open() will push its private data afterwards */
static int klog_new (int type, klog_t **pkl)
{
    klog_t *kl;

    dbg_return_if (pkl == NULL, ~0);

    kl = calloc(1, sizeof(klog_t));
    dbg_err_if (kl == NULL);

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
    klog_close(kl);
    return ~0;
}

void klog_close (klog_t *kl)
{
    dbg_return_if (kl == NULL, );

    switch (kl->type)
    {
        case KLOG_TYPE_MEM:
            klog_close_mem(kl->u.m);
            break;
        case KLOG_TYPE_FILE:
            klog_close_file(kl->u.f);
            break;
        case KLOG_TYPE_SYSLOG:
            klog_close_syslog(kl->u.s);
            break;
        default:
            warn("bad klog_s record !");
            return;
    }

    u_free(kl);

    return;
}

/* TODO */
static void klog_close_file (klog_file_t *klf)
{
    dbg_return_if (klf == NULL, );
    return;
}

static void klog_close_syslog (klog_syslog_t *kls)
{
    dbg_return_if (kls == NULL, );
    
    u_free(kls->ident);
    u_free(kls);
     
    return;
}

static void klog_mem_msgs_free (klog_mem_t *klm)
{
    klog_mem_msg_t *mmsg, *nmmsg;

    dbg_return_if (klm == NULL, );
    
    for (mmsg = TAILQ_FIRST(&klm->msgs); mmsg != NULL; mmsg = nmmsg)
    {
        TAILQ_REMOVE(&klm->msgs, mmsg, next);
        nmmsg = TAILQ_NEXT(mmsg, next);
        klm->count--;
        klog_mem_msg_free(mmsg);
    }

    return;
}

static void klog_close_mem (klog_mem_t *klm)
{
    klog_mem_msgs_free(klm);
    u_free(klm);

    return;
}

static int klog_clear_mem (klog_mem_t *klm)
{
    dbg_return_if (klm == NULL, ~0);

    /* wipe out all msgs in buffer */
    klog_mem_msgs_free(klm);

    /* reset header informations */
    klm->count = 0;
    TAILQ_INIT(&klm->msgs);

    return 0;
}

static int klog_open_mem (const char *id, size_t ln_max, klog_t **pkl)
{
    klog_mem_t *klm = NULL;

    dbg_return_if (pkl == NULL, ~0);

    /* create a new klog_t object (no specialisation) */
    dbg_err_if (klog_new(KLOG_TYPE_MEM, pkl));

    /* create a new klog_mem_t object */
    klm = calloc(1, sizeof(klog_mem_t));
    dbg_err_if (klm == NULL);
    
    /* initialise the klog_mem_t object to the supplied values */
    klm->id = id ? strdup(id) : NULL;   /* NULL is for anonymous log sink */
    klm->bound = ln_max;
    klm->count = 0;
    TAILQ_INIT(&klm->msgs);

    /* stick the klog_mem_t object to its parent */
    (*pkl)->u.m = klm;
    klm = NULL; /* ownership lost */

    return 0;

err:
    if (klm)
        klog_close_mem(klm);
    if (*pkl)
        klog_close(*pkl);
    return ~0;
}

static int klog_open_syslog (const char *ident, int facility, int logopt, 
        klog_t **pkl)
{
    klog_syslog_t *kls = NULL;
    
    dbg_return_if (pkl == NULL, ~0);

    dbg_err_if (klog_new(KLOG_TYPE_SYSLOG, pkl));

    kls = calloc(1, sizeof(klog_syslog_t));
    dbg_err_if (kls == NULL);

    kls->ident = ident ? strdup(ident) : strdup("");
    kls->facility = facility;
    kls->logopt = logopt;
    
    (*pkl)->u.s = kls;
    kls = NULL;

    return 0;
err:
    u_free(kls);
    if (*pkl)
        klog_close(*pkl);
    return ~0;
}

int klog (klog_t *kl, int level, const char *fmt, ...)
{
    int rv = 0;
    va_list ap;

    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (fmt == NULL, ~0);

    va_start(ap, fmt);
    
    switch (kl->type)
    {
        case KLOG_TYPE_MEM:
            rv = klog_mem(kl->u.m, level, fmt, ap);
            break;
        case KLOG_TYPE_FILE:
            rv = klog_file(kl->u.f, level, fmt, ap);
            break;
        case KLOG_TYPE_SYSLOG:
            rv = klog_syslog(kl->u.s, level, fmt, ap);
            break;
        default:
            rv = ~0;
            break;
    }

    va_end(ap);

    return rv;
}

static int klog_mem (klog_mem_t *klm, int level, const char *fmt, va_list ap)
{
    char ln[KLOG_LN_SZ + 1];
    klog_mem_msg_t *mmsg = NULL;

    /* check overflow */
    vsnprintf(ln, sizeof ln, fmt, ap);
    dbg_err_if (klog_mem_msg_new(ln, level, &mmsg));
    dbg_err_if (klog_mem_msg_push(klm, mmsg));
    mmsg = NULL;

    return 0;
err:
    if (mmsg)
        klog_mem_msg_free(mmsg);
    return ~0;
}

static int klog_mem_msg_new (const char *s, int level, klog_mem_msg_t **pmmsg)
{
    klog_mem_msg_t *mmsg = NULL;

    dbg_return_if (s == NULL, ~0);
    dbg_return_if (pmmsg == NULL, ~0);

    mmsg = calloc(1, sizeof(klog_mem_msg_t));
    dbg_err_if (mmsg == NULL);

    mmsg->line = u_strndup(s, KLOG_LN_SZ);
    mmsg->timestamp = time(NULL);
    mmsg->level = level;

    *pmmsg = mmsg;

    return 0;

err:
    if (mmsg)
        klog_mem_msg_free(mmsg);
    return ~0;
}

static int klog_mem_msg_push (klog_mem_t *klm, klog_mem_msg_t *mmsg)
{
    dbg_return_if (klm == NULL, ~0);
    dbg_return_if (mmsg == NULL, ~0);

    /* push out first-in element on KLOG_MEM_FULL event */
    if (KLOG_MEM_FULL(klm))
    {
        klog_mem_msg_t *last = TAILQ_LAST(&klm->msgs, h);

        /* last == NULL on KLOG_MEM_FULL should never happen :-) */
        if (last != NULL)
        {
            TAILQ_REMOVE(&klm->msgs, last, next);
            klm->count--;
        }
    }

    TAILQ_INSERT_HEAD(&klm->msgs, mmsg, next);
    klm->count++;

    return 0;
}

static void klog_mem_msg_free (klog_mem_msg_t *mmsg)
{
    dbg_return_if (mmsg == NULL, );

    u_free(mmsg->line);
    u_free(mmsg);

    return;
}

static int klog_file (klog_file_t *klf, int level, const char *fmt, va_list ap)
{
    U_UNUSED_ARG(klf);
    U_UNUSED_ARG(level);
    U_UNUSED_ARG(fmt);
    U_UNUSED_ARG(ap);
    return ~0;
}

static int klog_syslog (klog_syslog_t *kls, int level, const char *fmt, 
        va_list ap)
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

/* 'ln' must be KLOG_MSG_SZ + 1 bytes long */
int klog_getln (klog_t *kl, size_t nth, char ln[])
{
    dbg_return_if (kl == NULL, ~0);

    switch (kl->type)
    {
        case KLOG_TYPE_MEM:
            return klog_getln_mem(kl->u.m, nth, ln);
        case KLOG_TYPE_FILE:    /* TODO */
        case KLOG_TYPE_SYSLOG:
        default:
            warn("bad klog_s record !");
            return ~0;
    }
}

/* elements are retrieved in reverse order with respect to insertion */
static int klog_getln_mem (klog_mem_t *klm, size_t nth, char ln[])
{
    size_t i = nth;
    klog_mem_msg_t *cur; 
    char ct[26];

    dbg_return_if (klm == NULL, ~0);
    dbg_return_if (nth > klm->count, ~0);
    dbg_return_if (nth == 0, ~0);

    TAILQ_FOREACH (cur, &klm->msgs, next)
    {
        if (i-- == 1)
            break;
    }
    
    ctime_r((const time_t *) &cur->timestamp, ct);
    ct[24] = '\0';

    /* 
     * line format is:
     *      [DBG] Sep 23 13:26:19 <id>: log message line.
     */
    snprintf(ln, KLOG_LN_SZ + 1, "[%s] %s <%s>: %s", 
             klog_to_str(cur->level), ct, klm->id, cur->line);

    return 0;
}

ssize_t klog_countln (klog_t *kl)
{
    dbg_return_if (kl == NULL, ~0);

    switch (kl->type)
    {
        case KLOG_TYPE_MEM:
            return klog_countln_mem(kl->u.m);
        case KLOG_TYPE_FILE:    /* TODO */
        case KLOG_TYPE_SYSLOG:
        default:
            warn("bad klog_s record !");
            return -1;
    }
}

static ssize_t klog_countln_mem (klog_mem_t *klm)
{
    dbg_return_if (klm == NULL, -1);

    return klm->count;
}

static const char *klog_to_str (int lev)
{
    static const char *lstr[] =
        { "???", "DBG", "INF", "NTC", "WRN", "ERR", "CRT", "ALR", "EMR" };

    return (lev < KLOG_DEBUG || lev > KLOG_EMERG) ? lstr[0] : lstr[lev + 1];
}

static int klog_to_syslog (int lev)
{
    static int ldef[] = {
        LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING,
        LOG_ERR, LOG_CRIT, LOG_ALERT, LOG_EMERG
    };

    return (lev < KLOG_DEBUG || lev > KLOG_EMERG) ? -1 : ldef[lev];
}

int klog_clear (klog_t *kl)
{
    dbg_return_if (kl == NULL, ~0);

    switch (kl->type)
    {
        case KLOG_TYPE_MEM:
            return klog_clear_mem(kl->u.m);
        case KLOG_TYPE_FILE:    /* TODO */
        case KLOG_TYPE_SYSLOG:
        default:
            return ~0;
    }
}
