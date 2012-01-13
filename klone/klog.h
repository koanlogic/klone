/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: klog.h,v 1.19 2006/01/09 12:38:37 tat Exp $
 */

#ifndef _KLONE_LOG_H_
#define _KLONE_LOG_H_

#include <sys/types.h>
#include <stdarg.h>
#include <u/libu.h>

#ifdef __cplusplus
extern "C" {
#endif 

/* log levels, from low to high */
enum {
    KLOG_DEBUG,
    KLOG_INFO,
    KLOG_NOTICE,
    KLOG_WARNING,
    KLOG_ERR,
    KLOG_CRIT,
    KLOG_ALERT,
    KLOG_EMERG,
    KLOG_LEVEL_UNKNOWN    /* stopper */
};

/* internal representation of a 'log' config section */
struct klog_args_s
{
    int type;           /* one of KLOG_TYPEs */
    char *ident;        /* string prepended to each log msg */
    int threshold;      /* filter log msgs lower than this level */
    int mlimit;         /* max number of log messages (memory) */
    char *fbasename;    /* basename of log files (postfix varies) */
    int fsplits;        /* number of split files (file) */
    int flimit;         /* number of log msgs per file (file) */
    int soptions;       /* log options (syslog) */
    int sfacility;      /* default facility (syslog's LOG_LOCAL[0-7]) */
#define KLOG_FACILITY_UNKNOWN   -1
};  
    
typedef struct klog_args_s klog_args_t;

#define KLOG_LN_SZ          512 /* maximum log line size */
#define KLOG_ID_SZ          8   /* maximum log id size */
#define KLOG_MLIMIT_DFL     250 /* default number of log lines (mem) */
#define KLOG_FLIMIT_DFL     250 /* default number of log lines (file) */
#define KLOG_FSPLITS_DFL    4   /* default number of log files (file) */

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
    size_t bound;                           /* FIFO buffer max size */
    size_t nmsgs;                           /* # of msgs in buffer */
#define KLOG_MEM_FULL(klm)  ((klm)->nmsgs >= (klm)->bound)
    TAILQ_HEAD(mh, klog_mem_msg_s) msgs;    /* the list of msgs */
};

typedef struct klog_mem_s klog_mem_t;

struct klog_file_s
{
    size_t npages;  /* number of available log pages */
    size_t nlines;  /* number of available log lines per page */
    size_t wpageid; /* working page id */
    size_t offset;  /* write offset in working page */
    char basename[U_FILENAME_MAX];
#define KLOG_PAGE_FULL(klf)  ((klf)->offset >= (klf)->nlines)
    FILE *wfp;      /* working page file pointer */
};

typedef struct klog_file_s klog_file_t;

/* a syslog(3) wrapper  */
struct klog_syslog_s
{
    int facility;   /* default syslog(3) facility */
    int logopt;     /* log options bit field */
};

typedef struct klog_syslog_s klog_syslog_t;

struct klog_s
{
    enum { 
        KLOG_TYPE_UNKNOWN, 
        KLOG_TYPE_MEM, 
        KLOG_TYPE_FILE, 
        KLOG_TYPE_SYSLOG 
    } type;

#define IS_KLOG_TYPE(t) (t >= KLOG_TYPE_MEM && t <= KLOG_TYPE_SYSLOG)

    int threshold;                  /* min unfiltered level */
    char ident[KLOG_ID_SZ + 1];     /* id string prepended to each log msg */

    /* data private to each log type */
    union
    {
        klog_mem_t *m;
        klog_syslog_t *s;
        klog_file_t *f; 
    } u;

    /* availability of the following depends on klog_s type */
    int (*cb_log) (struct klog_s *, int, const char *, va_list);
    void (*cb_close) (struct klog_s *);
    int (*cb_getln) (struct klog_s *, size_t, char[]);
    ssize_t (*cb_countln) (struct klog_s *);
    int (*cb_clear) (struct klog_s *);
    int (*cb_flush) (struct klog_s *);
};

typedef struct klog_s klog_t;

int klog_open (klog_args_t *ka, klog_t **pkl);
int klog (klog_t *kl, int level, const char *msg, ...);
void klog_close (klog_t *kl);

/* file device specific */
int klog_flush (klog_t *kl);

/* mem device specific */
int klog_getln (klog_t *kl, size_t nth, char ln[]);
ssize_t klog_countln (klog_t *kl);
int klog_clear (klog_t *kl);

int klog_args (u_config_t *logsect, klog_args_t **pka);
void klog_args_free (klog_args_t *ka);
int klog_open_from_config (u_config_t *ls, klog_t **pkl);

#ifdef __cplusplus
}
#endif 

#endif  /* _KLONE_LOG_H_ */
