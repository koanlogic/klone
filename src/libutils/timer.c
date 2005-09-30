#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <klone/timer.h>
#include <klone/queue.h>
#include <klone/utils.h>
#include <klone/debug.h>

TAILQ_HEAD(alarm_list_s, alarm_s);
typedef struct alarm_list_s alarm_list_t;

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
};

/* this must be a singleton */
static timerm_t *timer = NULL;

static int timerm_set_next()
{
    alarm_t *al = NULL;
    time_t now = time(0);

    if((al = TAILQ_FIRST(&timer->alist)) == NULL)
        alarm(0);   /* disable the alarm */
    else
        alarm(MAX(1, al->expire - now));

    return 0;
}

void timerm_sigalrm(int sigalrm)
{
    alarm_t *al = NULL, *next = NULL;
    int expire;

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

        /* free the alarm */
        u_free(al);

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

static int timerm_free(timerm_t *t)
{
    alarm_t *a = NULL;

    if(t)
    {
        while((a = TAILQ_FIRST(&t->alist)) != NULL)
            dbg_if(timerm_del(a));

        u_free(t);
    }

    return 0;
}

static int timerm_create(timerm_t **pt)
{
    timerm_t *t = NULL;

    t = u_calloc(sizeof(timerm_t));
    dbg_err_if(t == NULL);

    TAILQ_INIT(&t->alist);

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

    dbg_err_if(cb == NULL || pa == NULL);

    if(timer == NULL)
    {
        dbg_err_if(timerm_create(&timer));
        /* set the signal handler */
        dbg_err_if(u_signal(SIGALRM, timerm_sigalrm));
    }

    al = (alarm_t*)u_calloc(sizeof(alarm_t));
    dbg_err_if(al == NULL);

    al->timer = timer;
    al->cb = cb;
    al->arg = arg;
    al->expire = now + secs;

    dbg_err_if(u_sig_block(SIGALRM));

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

    dbg_if(u_sig_unblock(SIGALRM));

    *pa = al;

    return 0;
err:
    if(timer)
    {
        timerm_free(timer);
        timer = NULL;
    }
    if(al)
        u_free(al);

    u_sig_unblock(SIGALRM);

    return ~0;
}

int timerm_del(alarm_t *a)
{
    dbg_err_if(a == NULL);

    dbg_err_if(u_sig_block(SIGALRM));

    TAILQ_REMOVE(&timer->alist, a, np);

    /* set the timer for the earliest alarm */
    timerm_set_next();

    dbg_if(u_sig_unblock(SIGALRM));

    u_free(a);

    return 0;
err:
    u_sig_unblock(SIGALRM);
    return ~0;
}

