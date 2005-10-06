/*  
 * (c)opyright 2005 - KoanLogic S.r.l.
 */

#ifndef _KLONE_LOG_H_
#define _KLONE_LOG_H_

#include "conf.h"
#include <sys/types.h>
#include <stdarg.h>
#include <klone/os.h>
#include <klone/queue.h>

/* log types */
enum { KLOG_TYPE_MEM, KLOG_TYPE_FILE, KLOG_TYPE_SYSLOG };

/* log levels, from low to high */
enum {
    KLOG_DEBUG,
    KLOG_INFO,
    KLOG_NOTICE,
    KLOG_WARNING,
    KLOG_ERR,
    KLOG_CRIT,
    KLOG_ALERT,
    KLOG_EMERG
};

#define KLOG_LN_SZ      512  /* maximum log line size */
#define KLOG_BOUND_DFL  250  /* maximum number of log lines (file and mem) */

/* a log line is at most KLOG_LN_SZ + 1 bytes long (including encoded
 * timestamp and severity) */
struct klog_mem_msg_s
{
    int level;          /* log severity */
    time_t timestamp;   /* message timestamp */
    char *line;         /* log line */
    TAILQ_ENTRY(klog_mem_msg_s) next;
};

typedef struct klog_mem_msg_s klog_mem_msg_t;

/* klog_mem_msg_s' organised in a fixed size buffer with FIFO discard policy */
struct klog_mem_s
{
    char *id;                           /* log sink id (owner ?) */
    size_t bound;                       /* FIFO buffer max size */
    size_t count;                       /* # of msgs in buffer */
#define KLOG_MEM_FULL(klm)  ((klm)->count >= (klm)->bound)
    TAILQ_HEAD(h, klog_mem_msg_s) msgs; /* the list of msgs */
};

typedef struct klog_mem_s klog_mem_t;

/* TODO */
struct klog_file_s
{
    int dummy;
};

typedef struct klog_file_s klog_file_t;

/* a syslog(3) wrapper  */
struct klog_syslog_s
{
    char *ident;    /* identifier prepended to each msg (optional) */
    int facility;   /* default syslog(3) facility */
    int logopt;     /* log options bit field */
};

typedef struct klog_syslog_s klog_syslog_t;

struct klog_s
{
    int type;               /* one of KLOG_TYPEs */
    union {
        klog_mem_t *m;      /* in-memory FIFO buffer */
        klog_file_t *f;     /* disk FIFO buffer */
        klog_syslog_t *s;   /* syslog(3) wrap */ 
    } u;
};

typedef struct klog_s klog_t;

/* exported prototypes */
int klog_open (int type, const char *id, int facility, int opt, size_t bound,
        klog_t **pkl);
int klog (klog_t *kl, int level, const char *msg, ...);
void klog_close (klog_t *kl);
int klog_getln (klog_t *kl, size_t nth, char ln[]);
ssize_t klog_countln (klog_t *kl);
int klog_clear (klog_t *kl);

#endif /* _KLONE_LOG_H_ */
