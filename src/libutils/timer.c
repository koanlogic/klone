/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: timer.c,v 1.13 2006/01/09 11:57:16 tat Exp $
 */

#include "klone_conf.h"
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <klone/timer.h>
#include <klone/utils.h>
#include <u/libu.h>

#ifdef OS_WIN
#include <windows.h>
#endif

TAILQ_HEAD(alarm_list_s, alarm_s);
typedef struct alarm_list_s alarm_list_t;

typedef void (*timerm_cb_t)(int);

struct alarm_s
{
    TAILQ_ENTRY(alarm_s) np;    /* next & prev pointers         */
    timerm_t *timer;            /* timerm_t that owns the alarm */
    time_t expire;              /* when to fire the alarm       */
    alarm_cb_t cb;              /* alarm callback               */
    void *arg;                  /* cb opaque argument           */
};

struct timerm_s
{
    alarm_list_t alist;         /* alarm list                   */

#ifdef OS_WIN
    CRITICAL_SECTION cs;
    time_t next;                /* next timestamp               */
    HANDLE hthread;             /* thread handle                */
    DWORD tid;                  /* thread id                    */
#endif
};

/* this must be a singleton */
static timerm_t *timer = NULL;

static int timerm_set_alarm(int timeout)
{
#ifdef OS_UNIX
    /* if timeout == 0 disable the alarm */
    alarm(timeout);
#else
    if(timeout > 0)
        timer->next = time(0) + timeout;
    else
        timer->next = NULL;
#endif

    return 0;
}

static int timerm_set_next(void)
{
    alarm_t *al = NULL;
    time_t now = time(0);

    if((al = TAILQ_FIRST(&timer->alist)) == NULL)
        timerm_set_alarm(0);   /* disable the alarm */
    else
        timerm_set_alarm(MAX(1, al->expire - now));

    return 0;
}

void timerm_sigalrm(int sigalrm)
{
    alarm_t *al = NULL, *next = NULL;
    int expire;

    u_unused_args(sigalrm);

    dbg_err_if(timer == NULL);

    for(;;)
    {
        /* get the topmost item and remove it from the list */
        al = TAILQ_FIRST(&timer->alist);
        dbg_err_if(al == NULL);

        expire = al->expire;

        TAILQ_REMOVE(&timer->alist, al, np);

        /* call the callback function */
        al->cb(al, al->arg);

        /* handle alarms with the same expiration date */
        next = TAILQ_FIRST(&timer->alist);
        if(next && next->expire == expire)
            continue;

        break;
    }

    /* prepare for the next alarm */
    if(TAILQ_FIRST(&timer->alist))
        timerm_set_next();

err:
    return;
}

static int timerm_block_alarms(void)
{
#ifdef OS_UNIX
    dbg_err_if(u_sig_block(SIGALRM));
#endif

#ifdef OS_WIN
    EnterCriticalSection(&timer->cs);
#endif

    return 0;
err:
    return ~0;
}

static int timerm_unblock_alarms(void)
{
#ifdef OS_UNIX
    dbg_err_if(u_sig_unblock(SIGALRM));
#endif

#ifdef OS_WIN
    LeaveCriticalSection(&timer->cs);
#endif

    return 0;
err:
    return ~0;
}

static int timerm_free(timerm_t *t)
{
    alarm_t *a = NULL;

    dbg_return_if (t == NULL, ~0);
    
    if(t)
    {
        while((a = TAILQ_FIRST(&t->alist)) != NULL)
            dbg_if(timerm_del(a));

        U_FREE(t);
    }

    return 0;
}

#ifdef OS_WIN
static DWORD WINAPI thread_func(LPVOID param)
{
    for(;;Sleep(250))
    {
        if(timer->next == NULL)
            continue;

        if((timer->next - time(0)) <= 0)
            timerm_sigalrm(0);  /* raise the alarm */
    }

    return 0;
}
#endif

static int timerm_create(timerm_t **pt)
{
    timerm_t *t = NULL;

    dbg_return_if (pt == NULL, ~0);

    t = u_zalloc(sizeof(timerm_t));
    dbg_err_if(t == NULL);

    TAILQ_INIT(&t->alist);

#ifdef OS_WIN
    InitializeCriticalSection(&t->cs);

    dbg_err_if((t->hthread = CreateThread(NULL, 0, thread_func, NULL, 0, 
        &t->tid)) == NULL); 
#endif

    *pt = t;

    return 0;
err:
    if(t)
        timerm_free(t);
    return ~0;
}

int timerm_add(int secs, alarm_cb_t cb, void *arg, alarm_t **pa)
{
    alarm_t *al = NULL;
    alarm_t *item = NULL;
    time_t now = time(0);

    dbg_return_if (cb == NULL, ~0);
    dbg_return_if (pa == NULL, ~0);

    if(timer == NULL)
    {
        dbg_err_if(timerm_create(&timer));
        #ifdef OS_UNIX
        dbg_err_if(u_signal(SIGALRM, timerm_sigalrm));
        #endif
    }

    al = (alarm_t*)u_zalloc(sizeof(alarm_t));
    dbg_err_if(al == NULL);

    al->timer = timer;
    al->cb = cb;
    al->arg = arg;
    al->expire = now + secs;

    dbg_err_if(timerm_block_alarms());

    /* insert al ordered by the expire field (smaller first) */
    TAILQ_FOREACH(item, &timer->alist, np)
        if(al->expire <= item->expire)
            break;

    if(item)
        TAILQ_INSERT_BEFORE(item, al, np);
    else
        TAILQ_INSERT_TAIL(&timer->alist, al, np);

    /* set the timer for the earliest alarm */
    timerm_set_next();

    dbg_err_if(timerm_unblock_alarms());

    *pa = al;

    return 0;
err:
    dbg("[%lu] timerm_add error", getpid());
    if(timer)
    {
        (void) timerm_free(timer);
        timer = NULL;
    }
    U_FREE(al);

    dbg_err_if(timerm_unblock_alarms());

    return ~0;
}

static int timerm_alarm_pending(alarm_t *a)
{
    alarm_t *t;

    TAILQ_FOREACH(t, &timer->alist,np)
    {
        if(t == a)
            return 1;   /* found */
    }
    return 0;
}

int timerm_del(alarm_t *a)
{
    dbg_return_if(a == NULL, ~0);

    dbg_err_if(timerm_block_alarms());

    /* if not expired remove it from the list */
    if(timerm_alarm_pending(a))
        TAILQ_REMOVE(&timer->alist, a, np);

    /* set the timer for the earliest alarm */
    timerm_set_next();

    dbg_err_if(timerm_unblock_alarms());

    U_FREE(a);

    return 0;
err:
    dbg_err_if(timerm_unblock_alarms());
    return ~0;
}
