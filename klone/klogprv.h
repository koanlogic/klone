#ifndef _KLONE_LOGPRV_H_
#define _KLONE_LOGPRV_H_

#include <klone/klog.h>

static const char *kloglev[] =
{
    "DBG", "INF", "NTC", "WRN", "ERR", "CRT", "ALR", "EMR"
};

/* common */
int klog_new (int type, int threshold, const char *ident, klog_t **pkl);
int klog_open_mem (klog_t *kl, size_t ln_max);
int klog_open_syslog (klog_t *kl, int fac, int logopt);
int klog_open_file (klog_t *kl, const char *base, size_t npages, size_t nlines);
const char *klog_to_str (int lev);

#endif /* _KLONE_LOGPRV_H_ */
