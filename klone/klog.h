/*  
 * (c)opyright 2005 - KoanLogic S.r.l.
 */

#ifndef _KLONE_LOG_H_
#define _KLONE_LOG_H_

#include "conf.h"
#include <sys/types.h>
#include <stdarg.h>
#include <u/libu.h>
#include <klone/os.h>

/* log types */
enum { KLOG_TYPE_UNKNOWN, KLOG_TYPE_MEM, KLOG_TYPE_FILE, KLOG_TYPE_SYSLOG };

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

#define KLOG_LN_SZ          512 /* maximum log line size */
#define KLOG_MLIMIT_DFL     250 /* maximum number of log lines (mem) */
#define KLOG_FLIMIT_DFL     250 /* maximum number of log lines (file) */
#define KLOG_FSPLITS_DFL    4   /* maximum number of log files (file) */

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
    char *id;                               /* log sink id (owner ?) */
    size_t bound;                           /* FIFO buffer max size */
    size_t nmsgs;                           /* # of msgs in buffer */
#define KLOG_MEM_FULL(klm)  ((klm)->nmsgs >= (klm)->bound)
    TAILQ_HEAD(mh, klog_mem_msg_s) msgs;    /* the list of msgs */
};

typedef struct klog_mem_s klog_mem_t;

struct klog_file_split_s
{
    int id;                                 /* split id: fname is basename.id */
    FILE *fp;                               /* file pointer (when active) */
    size_t nlines;                          /* # of lines actually written */
    TAILQ_ENTRY(klog_file_split_s) next;    /* next in log files list */
};

typedef struct klog_file_split_s klog_file_split_t;

struct klog_file_s
{
    size_t nfiles;                              /* # of split files */
    char *basename;                             /* splits' prefix */
    size_t bound;                               /* max lines in file */
    struct klog_file_split_s *factive;          /* active log file */
#define KLOG_FILE_FULL(klf)  ((klf)->factive->nlines >= (klf)->bound)
    TAILQ_HEAD (fh, klog_file_split_s) files;   /* list of all log files */
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
    int threshold;          /* min unfiltered level */
    union {
        klog_mem_t *m;      /* in-memory FIFO buffer */
        klog_file_t *f;     /* disk FIFO buffer */
        klog_syslog_t *s;   /* syslog(3) wrap */ 
    } u;
};

typedef struct klog_s klog_t;

/* internal representation of a 'log' config section */
struct klog_args_s
{
    int type;           /* one of KLOG_TYPEs */
    char *ident;        /* string prepended to each log msg */
    int threshold;      /* filter log msgs lower than this level */
    size_t mlimit;      /* max number of log messages (memory) */
    char *fbasename;    /* basename of log files (postfix varies) */
    size_t fsplits;     /* number of split files (file) */
    size_t flimit;      /* number of log msgs per file (file) */
    int soptions;       /* log options (syslog) */
    int sfacility;      /* default facility (syslog's LOG_LOCAL[0-7]) */
#define KLOG_FACILITY_UNKNOWN   -1
};

typedef struct klog_args_s klog_args_t;

/* common */
int klog_args (u_config_t *logsect, klog_args_t **pka);
void klog_args_free (klog_args_t *ka);

int klog_open (klog_args_t *ka, klog_t **pkl);
int klog (klog_t *kl, int level, const char *msg, ...);
void klog_close (klog_t *kl);
int klog_open_from_config (u_config_t *logsect, klog_t **pkl);

/* mem and file specific */
int klog_getln (klog_t *kl, size_t nth, char ln[]);
ssize_t klog_countln (klog_t *kl);
int klog_clear (klog_t *kl);

#endif /* _KLONE_LOG_H_ */
