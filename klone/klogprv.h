/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: klogprv.h,v 1.9 2006/01/09 12:38:37 tat Exp $
 */

#ifndef _KLONE_LOGPRV_H_
#define _KLONE_LOGPRV_H_

#include <klone/klog.h>

#ifdef __cplusplus
extern "C" {
#endif 

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

#ifdef __cplusplus
}
#endif 

#endif /* _KLONE_LOGPRV_H_ */
