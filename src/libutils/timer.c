/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: timer.c,v 1.16 2007/10/26 10:14:52 tat Exp $
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

TAILQ_HEAD(talarm_list_s, talarm_s);
typedef struct talarm_list_s talarm_list_t;

typedef void (*timerm_cb_t)(int);

struct talarm_s
{
    TAILQ_ENTRY(talarm_s) np;   /* next & prev pointers         */
    timerm_t *timer;            /* timerm_t that owns the alarm */
    time_t expire;              /* when to fire the alarm       */
    talarm_cb_t cb;             /* alarm callback               */
    void *arg;                  /* cb opaque argument           */
    pid_t owner;                /* process that set the alarm   */
};

struct timerm_s
{
    talarm_list_t alist;        /* alarm list                   */
    time_t next;                /* next timestamp               */
    int cb_running;             /* set when timer callback is running */
#ifdef OS_WIN
    CRITICAL_SECTION cs;
    HANDLE hthread;             /* thread handle                */
    DWORD tid;                  /* thread id                    */
#endif
};

/* this must be a singleton */
static timerm_t *timer = NULL;

static int timerm_set_alarm(int timeout)
{
    time_t n = time(0) + timeout;

    if(timeout && (timer->next == 0 || n < timer->next))
    {
        timer->next = n;
#ifdef OS_UNIX
        alarm(timeout);
#endif
    } 

    return 0;
}

static int timerm_set_next(void)
{
    talarm_t *al = NULL;
    time_t now = time(0);

    if((al = TAILQ_FIRST(&timer->alist)) != NULL)
        timerm_set_alarm(U_MAX(1, al->expire - now));

    return 0;
}

void timerm_sigalrm(int sigalrm)
{
    talarm_t *al = NULL;
    pid_t pid = getpid();
    time_t now = time(0);

    u_unused_args(sigalrm);

    dbg_err_if(timer == NULL);

    timer->next = 0;

    for(;;)
    {
        /* get the topmost item and remove it from the list */
        al = TAILQ_FIRST(&timer->alist);
        nop_err_if(al == NULL);

        if(al->owner != pid)
        {
            /* this alert has been inherited from the parent, we cannot
             * timerm_del() it because the user may have a reference to it
             * somewhere so we just remove it from the list of timers */
            TAILQ_REMOVE(&timer->alist, al, np);
            continue;
        }

        if(al->expire > now)
            break;
        
        TAILQ_REMOVE(&timer->alist, al, np);

        /* use to serialize callbacks nd avoid handler recursive */
        timer->cb_running = 1;

        /* call the callback function */
        al->cb(al, al->arg);

        timer->cb_running = 0;
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
    if(timer->cb_running)
        return 0;
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
    if(timer->cb_running)
        return 0;
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
    talarm_t *a = NULL;

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

/* use this function if you need to re-set the alarm in the signal
 * handler (don't timerm_del() on the alarm) */
int timerm_reschedule(talarm_t *al, int secs, talarm_cb_t cb, void *arg)
{
    talarm_t *item = NULL;
    time_t now = time(0);

    dbg_return_if (cb == NULL, ~0);
    dbg_return_if (al == NULL, ~0);

    al->cb = cb;
    al->arg = arg;
    al->expire = now + secs;

    dbg_err_if(timerm_block_alarms());

    /* insert al ordered by the expire field (smaller first) */
    TAILQ_FOREACH(item, &timer->alist, np)
        if(al->expire < item->expire)
            break; 
    
    if(item)
        TAILQ_INSERT_BEFORE(item, al, np);
    else
        TAILQ_INSERT_TAIL(&timer->alist, al, np);

    /* set the timer for the earliest alarm */
    timerm_set_next();
                                                              
    dbg_err_if(timerm_unblock_alarms());                      
                                                              
    return 0;                                                 
err:                                                          
    return ~0;                                                
}      

int timerm_add(int secs, talarm_cb_t cb, void *arg, talarm_t **pa)
{
    talarm_t *al = NULL;
    talarm_t *item = NULL;
    time_t now = time(0);
    pid_t pid = getpid();

    dbg_return_if (cb == NULL, ~0);
    dbg_return_if (pa == NULL, ~0);

    if(timer == NULL)
    {
        dbg_err_if(timerm_create(&timer));
        #ifdef OS_UNIX
        dbg_err_if(u_signal(SIGALRM, timerm_sigalrm));
        #endif
    }

    al = (talarm_t*)u_zalloc(sizeof(talarm_t));
    dbg_err_if(al == NULL);

    al->timer = timer;
    al->owner = pid;

    dbg_err_if(timerm_reschedule(al, secs, cb, arg));

    *pa = al;

    return 0;
err:
    u_dbg("[%lu] timerm_add error", (unsigned long) getpid());
    if(timer)
    {
        (void) timerm_free(timer);
        timer = NULL;
    }
    U_FREE(al);

    dbg_err_if(timerm_unblock_alarms());

    return ~0;
}

static int timerm_alarm_pending(talarm_t *a)
{
    talarm_t *t;

    TAILQ_FOREACH(t, &timer->alist,np)
    {
        if(t == a)
            return 1;   /* found */
    }
    return 0;
}

int timerm_del(talarm_t *a)
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
