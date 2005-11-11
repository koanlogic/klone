/*
 * (c)opyright 2005 - KoanLogic S.r.l.
 */

#include "conf.h"
#include <time.h>
#include <u/libu.h>
#include <klone/klog.h>
#include <klone/klogprv.h>

static int klog_args_new (klog_args_t **pka);
static int klog_args_check (klog_args_t *ka);

static int klog_type (const char *type);
static int klog_facility (const char *fac);
static int klog_threshold (const char *threshold);
static int klog_logopt (const char *options);

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
    klog_t *kl = NULL;

    dbg_return_if (pkl == NULL, ~0);
    dbg_return_if (ka == NULL, ~0);

    /* create a klog_t object: each per-type init will create and stick 
     * its klog_*_t obj */
    dbg_err_if (klog_new(ka->type, ka->threshold, ka->ident, &kl));

    switch (ka->type)
    {
        case KLOG_TYPE_MEM:
            rv = klog_open_mem(kl, ka->mlimit);
            break;
        case KLOG_TYPE_FILE:
            rv = klog_open_file(kl, ka->fbasename, ka->fsplits, ka->flimit);
            break;
        case KLOG_TYPE_SYSLOG:
            rv = klog_open_syslog(kl, ka->sfacility, ka->soptions);
            break;
        default:
            return ~0;
    }

    dbg_err_if (rv);
    *pkl = kl;
    
    return 0;
err:
    klog_close(kl);
    return ~0;
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
    va_list ap;
    int rv = 0;

    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (fmt == NULL, ~0);
    dbg_return_if (!IS_KLOG_TYPE(kl->type), ~0);

    va_start(ap, fmt);
    
    /* get rid of spurious stuff */
    dbg_goto_if (level < KLOG_DEBUG || level >= KLOG_LEVEL_UNKNOWN, end);

    /* early filtering of msgs with level lower than threshold */
    if (level < kl->threshold)
        goto end;
    
    /* if the log function is set call it with the supplied args */
    if (kl->cb_log)
        rv = kl->cb_log(kl, level, fmt, ap);

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
    nop_return_if (kl == NULL, );
    nop_return_if (!IS_KLOG_TYPE(kl->type), );

    /* call private close function */
    if (kl->cb_close)
        kl->cb_close(kl);

    U_FREE(kl);

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
    dbg_return_if (!IS_KLOG_TYPE(kl->type), ~0);

    if (kl->cb_getln)
        return kl->cb_getln(kl, nth, ln);
    
    return 0;
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
    dbg_return_if (!IS_KLOG_TYPE(kl->type), ~0);

    if (kl->cb_clear)
        return kl->cb_clear(kl);

    return ~0;
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
    dbg_return_if (kl == NULL, -1);
    dbg_return_if (!IS_KLOG_TYPE(kl->type), ~0);

    if (kl->cb_countln)
        return kl->cb_countln(kl);

    return -1;      /* XXX should be 0 ? */
}

/**
 * \brief   create a \c klog_t object reading configuration parameters from
 *          a configuration "log" record
 *
 * \param ls        a log configuration record
 * \param pka       the corresponding \c klog_args_t object as a value-result 
 *                  argument
 * \return
 * - \c 0  success
 * - \c ~0 on failure (\p pka MUST not be referenced)
 */
int klog_open_from_config(u_config_t *ls, klog_t **pkl)
{
    klog_t *kl = NULL;;
    klog_args_t *kargs = NULL;;

    /* parse config parameters */
    dbg_err_if(klog_args(ls, &kargs));

    /* create a klog object */
    dbg_err_if(klog_open(kargs, &kl));

    klog_args_free(kargs);
    kargs = NULL;

    /* stick it */
    *pkl = kl;

    return 0;
err:
    if(kargs)
        klog_args_free(kargs);
    if(kl)
        klog_close(kl);
    return ~0;
}

/**
 * \brief   flush all buffered data to the \c klog_t (file) device
 *
 * \param kl    a klog device
 *
 * \return
 * - \c 0  success
 * - \c ~0 on failure
 */
int klog_flush (klog_t *kl)
{
    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (!IS_KLOG_TYPE(kl->type), ~0);

    /* call private flush function */
    if (kl->cb_flush)
        return kl->cb_flush(kl);

    return 0;
}

/**
 *  \}
 */


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

void klog_args_free (klog_args_t *ka)
{
    U_FREE(ka->ident);
    U_FREE(ka->fbasename);
    U_FREE(ka);
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
    int i, logopt = 0;
    enum { NOPTS = 4 };
    char *optv[NOPTS + 1];
    
    dbg_return_if (options == NULL, 0);
    dbg_return_if ((o2 = u_strdup(options)) == NULL, 0);

    dbg_err_if (u_tokenize(o2, " \t", optv, NOPTS + 1));
    
    for (i = 0; optv[i] != NULL; i++)
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
    }

    U_FREE(o2);
    return logopt;

err:
    U_FREE(o2);
    return 0;
}

/* map LOG_LOCAL[0-7] strings into syslog(3) #define'd values */
static int klog_facility (const char *fac)
{
    dbg_err_if (fac == NULL);

    if (!strcasecmp(fac, "LOG_LOCAL0")) 
        return LOG_LOCAL0;
    else if (!strcasecmp(fac, "LOG_LOCAL1")) 
        return LOG_LOCAL1;
    else if (!strcasecmp(fac, "LOG_LOCAL2")) 
        return LOG_LOCAL2;
    else if (!strcasecmp(fac, "LOG_LOCAL3")) 
        return LOG_LOCAL3;
    else if (!strcasecmp(fac, "LOG_LOCAL4")) 
        return LOG_LOCAL4;
    else if (!strcasecmp(fac, "LOG_LOCAL5")) 
        return LOG_LOCAL5;
    else if (!strcasecmp(fac, "LOG_LOCAL6")) 
        return LOG_LOCAL6;
    else if (!strcasecmp(fac, "LOG_LOCAL7")) 
        return LOG_LOCAL7;

err:    
    warn("bad facility value \'%s\'", fac);
    return KLOG_FACILITY_UNKNOWN; 
}

static int klog_args_new (klog_args_t **pka)
{
    dbg_return_if (pka == NULL, ~0);

    *pka = u_zalloc(sizeof(klog_args_t));
    dbg_return_if (*pka == NULL, ~0);

    return 0;
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


