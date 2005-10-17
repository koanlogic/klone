/*
 * (c)opyright 2005 - KoanLogic S.r.l.
 */

#include "conf.h"
#include <time.h>
#include <u/libu.h>
#include <klone/klog.h>
#include <klone/klogprv.h>

static int klog_mem_msg_new (const char *s, int level, klog_mem_msg_t **pmmsg);
static int klog_mem_msg_push (klog_mem_t *klm, klog_mem_msg_t *mmsg);
static void klog_mem_msg_free (klog_mem_msg_t *mmsg);
static void klog_mem_msgs_free (klog_mem_t *klm);


/* mem log type initialisation */
int klog_open_mem (klog_t *kl, const char *id, size_t ln_max)
{
    klog_mem_t *klm = NULL;

    dbg_return_if (kl == NULL, ~0);

    /* create a new klog_mem_t object */
    klm = u_zalloc(sizeof(klog_mem_t));
    dbg_err_if (klm == NULL);
    
    /* initialise the klog_mem_t object to the supplied values */
    klm->id = id ? u_strdup(id) : NULL; /* NULL is for anonymous log sink */
    klm->bound = ln_max ? ln_max : 1;   /* set at least a 1 msg window :) */
    klm->nmsgs = 0;
    TAILQ_INIT(&klm->msgs);

    /* stick the klog_mem_t object to its parent */
    kl->u.m = klm, klm = NULL;

    return 0;
err:
    if (klm)
        klog_close_mem(klm);
    return ~0;
}

/* write a log msg to memory */
int klog_mem (klog_mem_t *klm, int level, const char *fmt, va_list ap)
{
    char ln[KLOG_LN_SZ + 1];
    klog_mem_msg_t *mmsg = NULL;

    /* NOTE: could check overflow here */
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

/* elements are retrieved in reverse order with respect to insertion */
int klog_getln_mem (klog_mem_t *klm, size_t nth, char ln[])
{
    size_t i = nth;
    klog_mem_msg_t *cur; 
    char ct[26];

    dbg_return_if (klm == NULL, ~0);
    dbg_return_if (nth > klm->nmsgs, ~0);
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

ssize_t klog_countln_mem (klog_mem_t *klm)
{
    dbg_return_if (klm == NULL, -1);

    return klm->nmsgs;
}

void klog_close_mem (klog_mem_t *klm)
{
    if (klm == NULL)
        return;
    klog_mem_msgs_free(klm);
    u_free(klm);

    return;
}

/* cancel all log messages but retain header information */
int klog_clear_mem (klog_mem_t *klm)
{
    dbg_return_if (klm == NULL, ~0);

    /* wipe out all msgs in buffer */
    klog_mem_msgs_free(klm);

    /* reset header informations */
    klm->nmsgs = 0;
    TAILQ_INIT(&klm->msgs);

    return 0;
}

static void klog_mem_msgs_free (klog_mem_t *klm)
{
    klog_mem_msg_t *mmsg;

    dbg_return_if (klm == NULL, );
    
    while((mmsg = TAILQ_FIRST(&klm->msgs)) != NULL)
    {
        TAILQ_REMOVE(&klm->msgs, mmsg, next);
        klm->nmsgs--;
        klog_mem_msg_free(mmsg);
    }

    return;
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
        klog_mem_msg_t *last = TAILQ_LAST(&klm->msgs, mh);

        /* last == NULL on KLOG_MEM_FULL should never happen :-) */
        if (last != NULL)
        {
            TAILQ_REMOVE(&klm->msgs, last, next);
            klog_mem_msg_free(last);
            klm->nmsgs--;
        }
    }

    TAILQ_INSERT_HEAD(&klm->msgs, mmsg, next);
    klm->nmsgs++;

    return 0;
}

static void klog_mem_msg_free (klog_mem_msg_t *mmsg)
{
    dbg_return_if (mmsg == NULL, );

    u_free(mmsg->line);
    u_free(mmsg);

    return;
}

