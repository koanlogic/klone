/*  
 * (c)opyright 2005 - KoanLogic S.r.l.
 */

#ifndef _KLONE_LOGPRV_H_
#define _KLONE_LOGPRV_H_

#include <klone/klog.h>

static const char *kloglev[] =
{
    "DEBUG", "INFO", "NOTICE", "WARNING", "ERR", "CRIT", "ALERT", "EMERG"
};

/* common */
int klog_new (int type, int threshold, klog_t **pkl);
const char *klog_to_str (int lev);

/* mem */
int klog_open_mem (klog_t *kl, const char *id, size_t ln_max);
int klog_mem (klog_mem_t *klm, int level, const char *fmt, va_list ap);
void klog_close_mem (klog_mem_t *klm);
int klog_getln_mem (klog_mem_t *klm, size_t nth, char ln[]);
ssize_t klog_countln_mem (klog_mem_t *klm);
int klog_clear_mem (klog_mem_t *klm);

/* file */
int klog_file (klog_file_t *klm, int level, const char *fmt, va_list ap);
int klog_open_file (klog_t *kl, char *basename, size_t npages, size_t nlines);
void klog_close_file (klog_file_t *klf);

/* syslog */
int klog_open_syslog (klog_t *kl, const char *ident, int fac, int logopt);
int klog_syslog (klog_syslog_t *klm, int level, const char *fmt, va_list ap);
void klog_close_syslog (klog_syslog_t *kls);

#endif /* _KLONE_LOGPRV_H_ */
