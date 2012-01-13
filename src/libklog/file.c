/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: file.c,v 1.20 2007/11/09 22:06:26 tat Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <u/libu.h>
#include <klone/klog.h>
#include <klone/klogprv.h>

static void klog_close_file (klog_t *klf);
static int klog_file (klog_t *klf, int level, const char *fmt, va_list ap);
static int klog_flush_file (klog_t *kl);

/* 
 * A 'file' log is physically subdivided in a certain number of files (pages)
 * named "<basename>.<page_id>" used as a sliding circular buffer.
 * A page must be thought as a fixed size array of log lines.  Each page 
 * in a 'file' log is of the same dimension so that each log line can be
 * referenced univocally.  Suppose a 'file' log made of n pages p_0, p_1, 
 * ..., p_n-1 of size m: the i-th line (0 <= i < n*m) will be found in page 
 * p_i%m at offset i%n.  We assume that at least 2 pages (n=2) exist.
 *
 * State informations are grouped into a 'head' file to be preserved between
 * one run and the subsequent.  Informations in 'head' (i.e. n, m, active page
 * id, offset in it) are used iff they correspond to actual config parameters.
 * Otherwise past log is discarded.
 *
 * (1) initialisation (where to start writing)
 * (2) append a log line
 * (3) termination (flush state to disk on exit)
 *
 * (1)
 * The 'file' log initialisation phase consists in the selection of an 
 * available page and an offset in it, where to start appending log messages.
 * The needed informations, if consistent with the supplied conf parameters, 
 * are gathered from the 'head' page.  If no 'head' is available (non-existent,
 * damaged or inconsistent with conf) the write pointer will be placed at page
 * zero, offset zero.
 */

static void klog_free_file (klog_file_t *klf);
static int klog_file_head_load (const char *base, klog_file_t **pklf);
static int klog_file_head_dump (klog_file_t *klf);
static int klog_file_head_new (const char *base, size_t npages, size_t nlines, 
        size_t wpageid, size_t offset, klog_file_t **pklf);
static void klog_file_head_free (klog_file_t *klf);
static int klog_file_append (klog_file_t *klf, const char *id, int level, 
        const char *ln);
static int klog_file_open_page (klog_file_t *klf);
static int klog_file_shift_page (klog_file_t *klf);

int klog_open_file (klog_t *kl, const char *base, size_t npages, size_t nlines)
{
    klog_file_t *klf = NULL;

    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (base == NULL, ~0);

    /* load an existing head or create a brand new if it doesn't exist */
    if (klog_file_head_load(base, &klf))
        dbg_err_if (klog_file_head_new(base, npages, nlines, 0, 0, &klf));
    else
    {
        /* check consistency with the supplied values, in case there is
         * a delta for npages and nlines, reset everything */
        dbg_ifb (klf->npages != npages || klf->nlines != nlines)
        {
            klf->npages = npages;
            klf->nlines = nlines;
            klf->wpageid = 0;
            klf->offset = 0;
        }
    }

    /* open the working log page for writing */
    dbg_err_if (klog_file_open_page(klf));

    /* set private methods */
    kl->cb_log = klog_file;
    kl->cb_close = klog_close_file;
    kl->cb_getln = NULL;
    kl->cb_countln = NULL;
    kl->cb_clear = NULL;
    kl->cb_flush = klog_flush_file;

    kl->u.f = klf, klf = NULL;

    return 0;
err:
    if (klf)
        klog_free_file(klf);
    return ~0;
}

static void klog_free_file (klog_file_t *klf)
{
    U_FREE(klf);
}

static int klog_flush_file (klog_t *kl)
{
    klog_file_t *klf;

    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (kl->type != KLOG_TYPE_FILE, ~0);
    dbg_return_if (kl->u.f == NULL, ~0);

    klf = kl->u.f;
     
    dbg_err_if (klf->wfp == NULL);
    dbg_err_sif (fflush(klf->wfp) != 0);

    return 0;
err:
    return ~0;
}

static int klog_file (klog_t *kl, int level, const char *fmt, va_list ap)
{
    klog_file_t *klf;
    char ln[KLOG_LN_SZ + 1];
    
    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (kl->type != KLOG_TYPE_FILE, ~0);
    dbg_return_if (kl->u.f == NULL, ~0);
    dbg_return_if (fmt == NULL, ~0);

    klf = kl->u.f;

    /* print log string */
    vsnprintf(ln, sizeof ln, fmt, ap);
    
    if (KLOG_PAGE_FULL(klf)) /* shift working page */
        dbg_err_if (klog_file_shift_page(klf));

    dbg_err_if (klog_file_append(klf, kl->ident, level, ln));

    return 0;
err:
    return ~0;
}

static int klog_file_append (klog_file_t *klf, const char *id, int level, 
        const char *ln)
{
    time_t now;
    char *ct;

    dbg_return_if (ln == NULL, ~0);
    dbg_return_if (id == NULL, ~0);
    dbg_return_if (klf == NULL, ~0);
    dbg_return_if (klf->wfp == NULL, ~0);

    dbg_err_if ((now = time(NULL)) == (time_t) -1);
    ct = ctime((const time_t *) &now);
    ct[24] = '\0';

    /* append line to wrk page */
    fprintf(klf->wfp, "[%s] %s <%s>: %s\n", 
            klog_to_str(level), ct, id, ln);
    klf->offset += 1;

    fflush(klf->wfp);

    return 0;
err:
    return ~0;
}

static void klog_close_file (klog_t *kl)
{
    klog_file_t *klf;
    
    dbg_ifb (kl == NULL) return;
    dbg_ifb (kl->type != KLOG_TYPE_FILE) return;
    dbg_ifb (kl->u.f == NULL) return;

    klf = kl->u.f;
    
    /* flush pending messages */
    U_FCLOSE(klf->wfp);

    /* dump head info to disk */
    dbg_if (klog_file_head_dump(klf));

    /* free resources */
    klog_free_file(klf), kl->u.f = NULL;
    
    return;
}

static int klog_file_head_load (const char *base, klog_file_t **pklf)
{
    FILE *hfp = NULL;
    char hf[U_FILENAME_MAX];
    klog_file_t hfs, *klf = NULL;

    dbg_return_if (base == NULL, ~0);
    dbg_return_if (pklf == NULL, ~0);
    
    dbg_err_if (u_path_snprintf(hf, U_FILENAME_MAX, U_PATH_SEPARATOR, "%s%s", 
        base, ".head"));
    dbg_err_if ((hfp = fopen(hf, "r")) == NULL);
    dbg_err_if (fread(&hfs, sizeof hfs, 1, hfp) != 1);
    U_FCLOSE(hfp);

    dbg_err_if (klog_file_head_new(base, hfs.npages, hfs.nlines, 
                                   hfs.wpageid, hfs.offset, &klf));
    *pklf = klf;

    return 0;
err:
    if (klf)
        klog_file_head_free(klf);
    U_FCLOSE(hfp);
    return ~0;
}

static int klog_file_head_dump (klog_file_t *klf)
{
    FILE *hfp = NULL;
    char hf[U_FILENAME_MAX];
    
    dbg_return_if (klf == NULL, ~0);

    dbg_err_if (u_path_snprintf(hf, U_FILENAME_MAX, U_PATH_SEPARATOR, "%s.%s", 
                                klf->basename, "head"));
    dbg_err_if ((hfp = fopen(hf, "w")) == NULL);
    dbg_err_if (fwrite(klf, sizeof(klog_file_t), 1, hfp) != 1);
    U_FCLOSE(hfp);

    return 0;
err:
    U_FCLOSE(hfp);
    return ~0;
}

static int klog_file_head_new (const char *base, size_t npages, size_t nlines, 
        size_t wpageid, size_t offset, klog_file_t **pklf)
{
    klog_file_t *klf = NULL;

    klf = u_zalloc(sizeof(klog_file_t));
    dbg_err_if (klf == NULL);

    u_strlcpy(klf->basename, base, sizeof klf->basename);
    klf->npages = npages;
    klf->nlines = nlines;
    klf->wpageid = wpageid;
    klf->offset = offset;
    klf->wfp = NULL;

    *pklf = klf;

    return 0;
err:
    klog_file_head_free(klf);
    return ~0;
}

static void klog_file_head_free (klog_file_t *klf)
{
    if (klf == NULL)
        return;

    U_FCLOSE(klf->wfp);
    U_FREE(klf);
    
    return;
}

static int klog_file_shift_page (klog_file_t *klf)
{
    char wf[U_FILENAME_MAX];
    
    dbg_return_if (klf == NULL, ~0);

    U_FCLOSE(klf->wfp);
    dbg_err_if (u_path_snprintf(wf, U_FILENAME_MAX, U_PATH_SEPARATOR, "%s.%d", 
        klf->basename, (klf->wpageid + 1)%klf->npages));
    dbg_err_if ((klf->wfp = fopen(wf, "w")) == NULL);

    klf->offset = 0;                                /* reset offset counter */
    klf->wpageid = ++(klf->wpageid)%klf->npages;    /* increment page id */
    
    return 0;
err:
    return ~0;
}

static int klog_file_open_page (klog_file_t *klf)
{
    char wf[U_FILENAME_MAX];

    dbg_return_if (klf == NULL, ~0);

    dbg_err_if (u_path_snprintf(wf, U_FILENAME_MAX, U_PATH_SEPARATOR, "%s.%d", 
                                klf->basename, klf->wpageid));
    dbg_err_if ((klf->wfp = fopen(wf, "a")) == NULL);
    
    return 0;
err:
    return ~0;
}
