/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: mem.c,v 1.12 2006/01/10 21:51:41 tat Exp $
 */

#include "klone_conf.h"
#include <time.h>
#include <u/libu.h>
#include <klone/klog.h>
#include <klone/klogprv.h>

static int klog_mem_msg_new (const char *s, int level, klog_mem_msg_t **pmmsg);
static int klog_mem_msg_push (klog_mem_t *klm, klog_mem_msg_t *mmsg);
static void klog_mem_msg_free (klog_mem_msg_t *mmsg);
static void klog_mem_msgs_free (klog_mem_t *klm);
static void klog_free_mem (klog_mem_t *klm);

static int klog_mem (klog_t *kl, int level, const char *fmt, va_list ap);
static int klog_getln_mem (klog_t *kl, size_t nth, char ln[]);
static ssize_t klog_countln_mem (klog_t *kl);
static void klog_close_mem (klog_t *kl);
static int klog_clear_mem (klog_t *kl);

/* mem log type initialisation */
int klog_open_mem (klog_t *kl, size_t ln_max)
{
    klog_mem_t *klm = NULL;

    dbg_return_if (kl == NULL, ~0);

    /* create a new klog_mem_t object */
    klm = u_zalloc(sizeof(klog_mem_t));
    dbg_err_if (klm == NULL);
    
    /* initialise the klog_mem_t object to the supplied values */
    klm->bound = ln_max ? ln_max : 1;   /* set at least a 1 msg window :) */
    klm->nmsgs = 0;
    TAILQ_INIT(&klm->msgs);

    /* set private methods */
    kl->cb_log = klog_mem;
    kl->cb_close = klog_close_mem;
    kl->cb_getln = klog_getln_mem;
    kl->cb_countln = klog_countln_mem;
    kl->cb_clear = klog_clear_mem;
    kl->cb_flush = NULL;

    /* stick the klog_mem_t object to its parent */
    kl->u.m = klm, klm = NULL;

    return 0;
err:
    if (klm)
        klog_free_mem(klm);
    return ~0;
}

/* write a log msg to memory */
static int klog_mem (klog_t *kl, int level, const char *fmt, va_list ap)
{
    klog_mem_t *klm;
    char ln[KLOG_LN_SZ + 1];
    klog_mem_msg_t *mmsg = NULL;

    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (kl->type != KLOG_TYPE_MEM, ~0);
    dbg_return_if (kl->u.m == NULL, ~0);
    dbg_return_if (fmt == NULL, ~0);

    klm = kl->u.m;

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
static int klog_getln_mem (klog_t *kl, size_t nth, char ln[])
{
    size_t i = nth;
    klog_mem_msg_t *cur; 
    char *ct;
    klog_mem_t *klm;

    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (kl->type != KLOG_TYPE_MEM, ~0);
    dbg_return_if (kl->u.m == NULL, ~0);
    nop_return_if (nth > kl->u.m->nmsgs, ~0);
    dbg_return_if (nth == 0, ~0);

    klm = kl->u.m;

    TAILQ_FOREACH (cur, &klm->msgs, next)
    {
        if (i-- == 1)
            break;
    }
    
    ct = ctime((const time_t *) &cur->timestamp);
    ct[24] = '\0';

    /* 
     * line format is:
     *      [DEBUG] Sep 23 13:26:19 <ident>: log message line.
     */
    snprintf(ln, KLOG_LN_SZ + 1, "[%s] %s <%s>: %s", 
             klog_to_str(cur->level), ct, kl->ident, cur->line);

    return 0;
}

static ssize_t klog_countln_mem (klog_t *kl)
{
    dbg_return_if (kl == NULL, -1);
    dbg_return_if (kl->type != KLOG_TYPE_MEM, -1);
    dbg_return_if (kl->u.m == NULL, -1);

    return kl->u.m->nmsgs;
}

static void klog_close_mem (klog_t *kl)
{
    if (kl == NULL || kl->type != KLOG_TYPE_MEM || kl->u.m == NULL)
        return;

    klog_free_mem(kl->u.m), kl->u.m = NULL;

    return;
}

static void klog_free_mem (klog_mem_t *klm)
{
    if (klm == NULL)
        return;
    klog_mem_msgs_free(klm);
    U_FREE(klm);
    return;
}

/* cancel all log messages but retain header information */
static int klog_clear_mem (klog_t *kl)
{
    klog_mem_t *klm;

    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (kl->type != KLOG_TYPE_MEM, ~0);
    dbg_return_if (kl->u.m == NULL, ~0);

    klm = kl->u.m;

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

    dbg_ifb (klm == NULL) return;
    
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
    dbg_ifb (mmsg == NULL) return;

    U_FREE(mmsg->line);
    U_FREE(mmsg);

    return;
}

