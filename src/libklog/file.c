/*
 * (c)opyright 2005 - KoanLogic S.r.l.
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <u/libu.h>
#include <klone/klog.h>
#include <klone/klogprv.h>

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
 * (x) retrieve the nth log line
 * (y) clear all
 *
 * (1)
 * The 'file' log initialisation phase consists in the selection of an 
 * available page and an offset in it, where to start appending log messages.
 * The needed informations, if consistent with the supplied conf parameters, 
 * are gathered from the 'head' page.  If no 'head' is available (non-existent,
 * damaged or inconsistent with conf) the write pointer will be placed at page
 * zero, offset zero.
 */

static int klog_file_head_load (const char *bname, klog_file_t **pklf);
static int klog_file_head_dump (klog_file_t *klf);
static int klog_file_head_new (const char *bname, const char *id, size_t npages,
        size_t nlines, size_t wpageid, size_t offset, klog_file_t **pklf);
static void klog_file_head_free (klog_file_t *klf);
static int klog_file_append (klog_file_t *klf, int level, char *ln);

int klog_open_file (klog_t *kl, const char *bname, const char *id, 
        size_t npages, size_t nlines)
{
    klog_file_t *klf = NULL;

    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (bname == NULL, ~0);

    if (klog_file_head_load(bname, &klf))
        /* if there is no head, try to create a new one */
        dbg_err_if (klog_file_head_new(bname, id, npages, nlines, 0, 0, &klf));
    else
    {
        /* check consistency with the supplied values */
        dbg_ifb (klf->npages != npages)
            klf->npages = npages;
        dbg_ifb (klf->nlines != nlines)
            klf->nlines = nlines;
    }

    /* TODO open the working log page for writing */

    return 0;
err:
    return ~0;
}

int klog_file (klog_file_t *klf, int level, const char *fmt, va_list ap)
{
    char ln[KLOG_LN_SZ + 1];
    
    dbg_return_if (klf == NULL, ~0);
    dbg_return_if (fmt == NULL, ~0);

    /* print log string */
    vsnprintf(ln, sizeof ln, fmt, ap);
    
    if (KLOG_PAGE_FULL(klf))
    {
        /* TODO shift wrk page (i.e. close wrk, open next for writing) */
    }

    dbg_err_if (klog_file_append(klf, level, ln));

    return 0;
err:
    return ~0;
}

static int klog_file_append (klog_file_t *klf, int level, char *ln)
{
    time_t now;
    char ct[26];

    dbg_return_if (ln == NULL, ~0);
    dbg_return_if (klf == NULL, ~0);
    dbg_return_if (klf->wfp == NULL, ~0);

    ctime_r((const time_t *) &now, ct);
    ct[24] = '\0';

    /* append line to wrk page */
    fprintf(klf->wfp, "[%s] %s <%s>: %s", klog_to_str(level), ct, klf->id, ln);
    klf->offset += 1;

    return 0;
}

void klog_close_file (klog_file_t *klf)
{
    u_unused_args(klf);

    /* flush pending messages */
    fclose(klf->wfp);

    /* dump head info to disk */
    dbg_if (klog_file_head_dump(klf));
    
    return;
}

static int klog_file_head_load (const char *bname, klog_file_t **pklf)
{
    int fd = -1;
    struct stat sb;
    char hf[PATH_MAX + 1];
    char *mm = MAP_FAILED;
    klog_file_t *hfs, *klf = NULL;

    dbg_return_if (bname == NULL, ~0);
    dbg_return_if (pklf == NULL, ~0);
    
    dbg_err_if (u_path_snprintf(hf, PATH_MAX + 1, "%s%s", bname, ".head"));
    dbg_err_if ((fd = open(hf, O_RDONLY)) == -1);
    dbg_err_if (fstat(fd, &sb) == -1);
    dbg_err_if (sb.st_size != sizeof(klog_file_t));
    mm = mmap(0, sizeof(klog_file_t), PROT_READ, MAP_PRIVATE, fd, 0);
    dbg_err_if (mm == MAP_FAILED);
    hfs = (klog_file_t *) mm;

    dbg_err_if (klog_file_head_new(bname, hfs->id, hfs->npages, hfs->nlines, 
                                   hfs->wpageid, hfs->offset, &klf));

    *pklf = klf;

    munmap(mm, sizeof(klog_file_t));
    close(fd);

    return 0;
err:
    if (klf)
        klog_file_head_free(klf);
    if (mm != MAP_FAILED)
        munmap(mm, sizeof(klog_file_t));
    U_CLOSE(fd);
    return ~0;
}

static int klog_file_head_dump (klog_file_t *klf)
{
    FILE *hfp = NULL;
    char hf[PATH_MAX + 1];
    
    dbg_return_if (klf == NULL, ~0);

    dbg_err_if (u_path_snprintf(hf, PATH_MAX + 1, "%s%s", 
                                klf->basename, ".head"));

    dbg_err_if ((hfp = fopen(hf, "w")) == NULL);
    dbg_err_if (fwrite(klf, sizeof(klog_file_t), 1, hfp) != 1);
    dbg_if (fclose(hfp));

    return 0;
err:
    U_FCLOSE(hfp);
    return ~0;
}

static int klog_file_head_new (const char *bname, const char *id, size_t npages,
        size_t nlines, size_t wpageid, size_t offset, klog_file_t **pklf)
{
    klog_file_t *klf = NULL;

    klf = u_zalloc(sizeof(klog_file_t));
    dbg_err_if (klf == NULL);

    u_sstrncpy(klf->basename, bname, PATH_MAX);
    u_sstrncpy(klf->id, id, KLOG_ID_SZ);
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
    free(klf);
    
    return;
}
