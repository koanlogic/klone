/*
 * (c)opyright 2005 - KoanLogic S.r.l.
 */

#include "conf.h"
#include <time.h>
#include <klone/klog.h>
#include <klone/utils.h>
#include <u/libu.h>

static int klog_args_new (klog_args_t **pka);
static void klog_args_free (klog_args_t *ka);
static int klog_type (const char *type);
static int klog_facility (const char *facility);
static int klog_threshold (const char *threshold);
static int klog_logopt (const char *options);
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
static int klog_args_check (klog_args_t *ka);

static const char *kloglev[] = { 
    "DEBUG", "INFO", "NOTICE", "WARNING", "ERR", "CRIT", "ALERT", "EMERG" 
};

static int sysloglev[] = {
    LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING, 
    LOG_ERR, LOG_CRIT, LOG_ALERT, LOG_EMERG
};

/**
 *  \defgroup klog KLOG
 *  \{
 */

/**
 * \brief   create a \c klog_args_t object with configuration parameters read
 *          from a log subsection of a kloned configuration file
 *
 * \param ls        a log configuration record
 * \param pka       the corresponding \c klog_args_t object as a value-result 
 *                  argument
 * \return
 * - \c 0  success
 * - \c ~0 on failure (\p pka MUST not be referenced)
 */
int klog_args (u_config_t *ls, klog_args_t **pka)
{
    const char *cs;
    klog_args_t *ka = NULL;

    dbg_return_if (ls == NULL, ~0);
    dbg_return_if (pka == NULL, ~0);

    /* here defaults are set */
    dbg_err_if (klog_args_new(&ka));

    /* read in config values */
    ka->type = klog_type(u_config_get_subkey_value(ls, "type"));

    if ((cs = u_config_get_subkey_value(ls, "ident")) != NULL)
        ka->ident = u_strdup(cs);

    ka->threshold = klog_threshold(u_config_get_subkey_value(ls, "threshold"));

    if ((cs = u_config_get_subkey_value(ls, "mem.limit")) != NULL)
        ka->mlimit = atoi(cs);

    if ((cs = u_config_get_subkey_value(ls, "file.basename")) != NULL) 
        ka->fbasename = u_strdup(cs);

    if ((cs = u_config_get_subkey_value(ls, "file.limit")) != NULL)
        ka->flimit = atoi(cs);

    if ((cs = u_config_get_subkey_value(ls, "file.splits")) != NULL)
        ka->fsplits = atoi(cs);

    ka->sfacility = 
        klog_facility(u_config_get_subkey_value(ls, "syslog.facility"));

    ka->soptions = klog_logopt(u_config_get_subkey_value(ls, "syslog.options"));

    dbg_return_if (klog_args_check(ka), ~0);

    *pka = ka;
    
    return 0;
err:
    if (ka)
        klog_args_free(ka);
    return ~0;
}

/**
 * \brief   Create a new \c klog_t object from the corresponding \c klog_args_t 
 *
 * \param ka        an initialised \c klog_args_t object
 * \param pkl       the newly created \c klog_t object as a value-result
 *                  argument  
 * \return
 * - \c 0   success
 * - \c ~0  on error (\p pkl MUST not be referenced)
 */
int klog_open (klog_args_t *ka, klog_t **pkl)
{
    int rv;

    dbg_return_if (pkl == NULL, ~0);
    dbg_return_if (ka == NULL, ~0);

    switch (ka->type)
    {
        case KLOG_TYPE_MEM:
            rv = klog_open_mem(ka->ident, ka->mlimit, pkl);
            break;
        case KLOG_TYPE_FILE: /* TODO */
            rv = ~0;
            break;
        case KLOG_TYPE_SYSLOG:
            rv = klog_open_syslog(ka->ident, ka->sfacility, ka->soptions, pkl);
            break;
        default:
            return ~0;
    }

    /* go out if something went wrong (deallocation done in klog_open_*()) */
    if (rv)
        return rv;

    /* otherwise fill the rest of klog_t */
    (*pkl)->threshold = ka->threshold;

    return 0;
}

/** 
 * \brief   Log a \c KLOG message
 *
 * \param kl        The klog context in use
 * \param level     log severity, from KLOG_DEBUG to KLOG_EMERG
 * \param fmt       log format string.  Note that the conversion specification
 *                  depends on the underying log type: \c KLOG_TYPE_MEM and 
 *                  \c KLOG_TYPE_FILE have printf(3)-like behaviour, while 
 *                  \c KLOG_TYPE_SYSLOG follows syslog(3) format rules.
 * \param ...       parameters to \p fmt
 *
 * \return
 * - \c 0   success
 * - \c ~0  on failure
 */
int klog (klog_t *kl, int level, const char *fmt, ...)
{
    int rv = 0;
    va_list ap;

    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (fmt == NULL, ~0);

    va_start(ap, fmt);
    
    /* get rid of spurious stuff */
    dbg_goto_if (level < KLOG_DEBUG || level >= KLOG_LEVEL_UNKNOWN, end);

    /* early filtering of msgs with level lower than threshold */
    if (level < kl->threshold)
        goto end;
    
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

end:
    va_end(ap);

    return rv;
}

/** 
 * \brief   Destroy a \c klog_t object
 *
 * \param kl    The \c klog_t object to be destroyed.  When destroying a 
 *              \c KLOG_TYPE_MEM object, all log messages are lost.
 * \return nothing
 */
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

/** 
 * \brief   Return the nth memory-log message
 *
 * \param kl    A \c KLOG_TYPE_MEM context
 * \param nth   a log message index: elements are retrieved in reverse order 
 *              with respect to insertion
 * \param ln    where the log string shall be stored: it must be a preallocated 
 *              buffer, at least \c KLOG_MSG_SZ \c + \c 1 bytes long
 * \return
 * - \c 0   success
 * - \c ~0  on failure
 */
int klog_getln (klog_t *kl, size_t nth, char ln[])
{
    dbg_return_if (kl == NULL, ~0);

    switch (kl->type)
    {
        case KLOG_TYPE_MEM:
            return klog_getln_mem(kl->u.m, nth, ln);
        case KLOG_TYPE_FILE:
        case KLOG_TYPE_SYSLOG:
        default:
            warn("bad klog_s record !");
            return ~0;
    }
}

/** 
 * \brief   Remove all memory-log messages from the supplied \c klog_t context 
 *
 * \param kl    A \c KLOG_TYPE_MEM context
 *
 * \return
 * - \c 0   success
 * - \c ~0  on failure
 */
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

/** 
 * \brief   Count the number of lines in the supplied memory-log context \p kl
 *
 * \param kl    A \c KLOG_TYPE_MEM context
 *
 * \return nothing
 */
ssize_t klog_countln (klog_t *kl)
{
    dbg_return_if (kl == NULL, ~0);

    switch (kl->type)
    {
        case KLOG_TYPE_MEM:
            return klog_countln_mem(kl->u.m);
        case KLOG_TYPE_FILE:
        case KLOG_TYPE_SYSLOG:
        default:
            warn("bad klog_s record !");
            return -1;
    }
}

/**
 *  \}
 */

/* each klog_*_open() will push its private data afterwards */
static int klog_new (int type, klog_t **pkl)
{
    klog_t *kl;

    dbg_return_if (pkl == NULL, ~0);

    kl = u_zalloc(sizeof(klog_t));
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
    klm = u_zalloc(sizeof(klog_mem_t));
    dbg_err_if (klm == NULL);
    
    /* initialise the klog_mem_t object to the supplied values */
    klm->id = id ? u_strdup(id) : NULL;   /* NULL is for anonymous log sink */
    klm->bound = ln_max ? ln_max : 1;   /* set at least a 1 msg window :) */
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

    kls = u_zalloc(sizeof(klog_syslog_t));
    dbg_err_if (kls == NULL);

    kls->ident = ident ? u_strdup(ident) : u_strdup("");
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

    mmsg = u_zalloc(sizeof(klog_mem_msg_t));
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
            klog_mem_msg_free(last);
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

/* TODO */
static int klog_file (klog_file_t *klf, int level, const char *fmt, va_list ap)
{
    u_unused_args(klf, level, fmt, ap);

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
     *      [DEBUG] Sep 23 13:26:19 <ident>: log message line.
     */
    snprintf(ln, KLOG_LN_SZ + 1, "[%s] %s <%s>: %s", 
             klog_to_str(cur->level), ct, klm->id, cur->line);

    return 0;
}

static ssize_t klog_countln_mem (klog_mem_t *klm)
{
    dbg_return_if (klm == NULL, -1);

    return klm->count;
}

static const char *klog_to_str (int lev)
{
    return (lev < KLOG_DEBUG || lev > KLOG_EMERG) ? "?" : kloglev[lev];
}

static int klog_to_syslog (int lev)
{
    return (lev < KLOG_DEBUG || lev > KLOG_EMERG) ? -1 : sysloglev[lev];
}

static int klog_args_check (klog_args_t *ka)
{
    dbg_return_if (ka == NULL, ~0);

    if (ka->type == KLOG_TYPE_UNKNOWN)
        warn_err("unknown log type");

    /* do not filter if not specified or if a wrong value has been supplied */
    if (ka->threshold == KLOG_LEVEL_UNKNOWN)
    {
        warn("threshold unspecified: assuming lowest possible (DEBUG)");
        ka->threshold = KLOG_DEBUG;
    }

    switch (ka->type)
    {
        case KLOG_TYPE_MEM:
            if (ka->mlimit == 0)
                ka->mlimit = KLOG_MLIMIT_DFL;
            break;
        case KLOG_TYPE_FILE:
            if (ka->fbasename == NULL)
                warn_err("log file path is mandatory !");
            if (ka->fsplits == 0)
                ka->fsplits =  KLOG_FSPLITS_DFL;
            if (ka->flimit == 0)
                ka->flimit = KLOG_FLIMIT_DFL;
            break;
        case KLOG_TYPE_SYSLOG:
            if (ka->sfacility == KLOG_FACILITY_UNKNOWN)
            {
                warn("facility unspecified: defaults to LOG_LOCAL7");
                ka->sfacility = LOG_LOCAL7;
            }
            break;
        default:
            warn_err("what are you doing here ?");
    }

    return 0;
err:
    return ~0;
}

static int klog_args_new (klog_args_t **pka)
{
    klog_args_t *ka = u_zalloc(sizeof(klog_args_t));

    dbg_return_if (ka == NULL, ~0);
    /* XXX should set defaults */
    *pka = ka;

    return 0;
}

/* just for testing */
void klog_args_print (FILE *fp, klog_args_t *ka)
{
    dbg_return_if (ka == NULL, );
    dbg_return_if (fp == NULL, );

    fprintf(fp, "ka->type: \t %d\n", ka->type);
    fprintf(fp, "ka->ident: \t %s\n", ka->ident);
    fprintf(fp, "ka->threshold: \t %d\n", ka->threshold);
    fprintf(fp, "ka->mlimit: \t %zd\n", ka->mlimit);
    fprintf(fp, "ka->fbasename: \t %s\n", ka->fbasename);
    fprintf(fp, "ka->fsplits: \t %zd\n", ka->fsplits);
    fprintf(fp, "ka->flimit: \t %zd\n", ka->flimit);
    fprintf(fp, "ka->soptions: \t %d\n", ka->soptions);
    fprintf(fp, "ka->sfacility: \t %d\n", ka->sfacility);

    return;
}

static void klog_args_free (klog_args_t *ka)
{
    u_free(ka->ident);
    u_free(ka->fbasename);
    u_free(ka);
    return;
}

/* map type directive to the internal representation */
static int klog_type (const char *type)
{
    dbg_return_if (type == NULL, KLOG_TYPE_UNKNOWN);

    if (!strcasecmp(type, "mem"))
        return KLOG_TYPE_MEM;
    else if (!strcasecmp(type, "file"))
        return KLOG_TYPE_FILE;
    else if (!strcasecmp(type, "syslog"))
        return KLOG_TYPE_SYSLOG;
    else
        return KLOG_TYPE_UNKNOWN;
}

/* map threshold directive to the internal representation */
static int klog_threshold (const char *threshold)
{
    int i;

    dbg_return_if (threshold == NULL, KLOG_LEVEL_UNKNOWN);

    for (i = KLOG_DEBUG; i <= KLOG_EMERG; i++)
        if (!strcasecmp(threshold, kloglev[i]))
            return i;

    warn("bad threshold value: \'%s\'", threshold);
    return KLOG_LEVEL_UNKNOWN;
}

/* map threshold directive to the internal representation */
static int klog_logopt (const char *options)
{
    char *o2 = NULL;    /* 'options' dupped for safe u_tokenize() */
    int i = 0, logopt = 0;
    enum { NOPTS = 4 };
    char *optv[NOPTS + 1];
    
    dbg_return_if (options == NULL, 0);
    dbg_return_if ((o2 = u_strdup(options)) == NULL, 0);

    dbg_err_if (u_tokenize(o2, " \t", optv, NOPTS + 1));
    
    while (optv[i])
    {
        if (!strcasecmp(optv[i], "LOG_CONS"))
            logopt |= LOG_CONS;
        else if (!strcasecmp(optv[i], "LOG_NDELAY"))
            logopt |= LOG_NDELAY;
        else if (!strcasecmp(optv[i], "LOG_PERROR"))
            logopt |= LOG_PERROR;
        else if (!strcasecmp(optv[i], "LOG_PID"))
            logopt |= LOG_PID;
        else
            warn("bad log option: \'%s\'", optv[i]);
        i++;
    }

    u_free(o2);
    return logopt;

err:
    u_free(o2);
    return 0;
}

/* map LOG_LOCAL[0-7] strings into syslog(3) #define'd values */
static int klog_facility (const char *facility)
{
    dbg_err_if (facility == NULL);

    if (!strcasecmp(facility, "LOG_LOCAL0")) 
        return LOG_LOCAL0;
    else if (!strcasecmp(facility, "LOG_LOCAL1")) 
        return LOG_LOCAL1;
    else if (!strcasecmp(facility, "LOG_LOCAL2")) 
        return LOG_LOCAL2;
    else if (!strcasecmp(facility, "LOG_LOCAL3")) 
        return LOG_LOCAL3;
    else if (!strcasecmp(facility, "LOG_LOCAL4")) 
        return LOG_LOCAL4;
    else if (!strcasecmp(facility, "LOG_LOCAL5")) 
        return LOG_LOCAL5;
    else if (!strcasecmp(facility, "LOG_LOCAL6")) 
        return LOG_LOCAL6;
    else if (!strcasecmp(facility, "LOG_LOCAL7")) 
        return LOG_LOCAL7;

err:    
    warn("bad facility value \'%s\'", facility);
    return KLOG_FACILITY_UNKNOWN; 
}
