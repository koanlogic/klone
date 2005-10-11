/*
 * (c)opyright 2005 - KoanLogic S.r.l.
 */

#include "conf.h"
#include <time.h>
#include <u/libu.h>
#include <klone/klog.h>
#include <klone/klogprv.h>

static klog_file_split_t *klog_get_available (klog_file_t *klf);
static klog_file_split_t *klog_get_split (klog_file_t *klf);
static int klog_file_split_new (int id, klog_file_split_t **pfs);
static ssize_t klog_countln_split (klog_file_split_t *sf);

int klog_open_file (klog_t *kl, const char *basename, size_t nsplits, 
        size_t max_lines)
{
    size_t i;
    klog_file_t *klf = NULL;

    dbg_return_if (kl == NULL, ~0);
    dbg_return_if (basename == NULL, ~0);
    dbg_return_if (nsplits == 0, ~0);
    dbg_return_if (max_lines == 0, ~0);

    klf = u_zalloc(sizeof(klog_file_t));
    dbg_err_if (klf == NULL);

    /* init the klog_file_t obj */
    klf->nfiles = 0;
    klf->bound = max_lines;
    dbg_err_if ((klf->basename = u_strdup(basename)) == NULL);
    TAILQ_INIT(&klf->files); 

    /* for each split create the corresponding klog_file_split_t and 
     * push it onto the parent klog_file_t obj */
    for (i = 1; i <= nsplits; i++)
    {
        klog_file_split_t *fsplit;

        klog_file_split_new(i, &fsplit);
        TAILQ_INSERT_HEAD(&klf->files, fsplit, next);
        klf->nfiles++;
    }

    /* get the first available log file */
    dbg_err_if ((klf->factive = klog_get_split(klf)) == NULL);

    /* TODO */

    kl->u.f = klf, klf = NULL;

    return 0;
err:
    if (klf)
        klog_close_file(klf);
    return ~0;
}

/* TODO */
int klog_file (klog_file_t *klf, int level, const char *fmt, va_list ap)
{
    u_unused_args(klf, level, fmt, ap);
    return ~0;
}

/* TODO */
void klog_close_file (klog_file_t *klf)
{
    dbg_return_if (klf == NULL, );
    return;
}

/* return the first non-full file or the first in chain if all files are full 
 * - tongue twister comment :-) */
static klog_file_split_t *klog_get_split (klog_file_t *klf)
{
    klog_file_split_t *avail = klog_get_available(klf);

    return avail != NULL ? avail : TAILQ_FIRST(&klf->files);
}

/* if a suitable split is found, it is also set up (fopen'd and fseek'd) */
static klog_file_split_t *klog_get_available (klog_file_t *klf)
{
    char mode[3] = "r+";
    char splitpath[PATH_MAX];
    klog_file_split_t *cur;
    ssize_t nl;

    dbg_return_if (klf == NULL, NULL);
    
    /* try to get the first available (i.e. not full) */
    TAILQ_FOREACH (cur, &klf->files, next)
    {
        u_path_snprintf(splitpath, PATH_MAX, "%s.%d", klf->basename, cur->id);
again:
        cur->fp = fopen(splitpath, mode);
        if (cur->fp == NULL && errno == ENOENT)
        {
            mode[0] = 'a';
            goto again;
        } else {
            warn_strerror(errno);
            continue;
        }

        dbg_ifb ((nl = klog_countln_split(cur)) == -1)
        {
            warn("damaged split file \'%d\'", cur->id);
            continue;
        }

        /* if we've found it, also set its position at the end of file */
        if ((size_t) nl < klf->bound)
        {
            if (fseek(cur->fp, 0, SEEK_END))
            {
                U_FCLOSE(cur->fp);
                return NULL;
            }
            return cur;
        }

        U_FCLOSE(cur->fp);
    }

    return NULL;
}

ssize_t klog_countln_file (klog_file_t *klf)
{
    size_t i = 0;
    klog_file_split_t *cur;

    dbg_return_if (klf == NULL, -1);

    TAILQ_FOREACH (cur, &klf->files, next)
    {
        ssize_t tmp = klog_countln_split(cur);;

        dbg_ifb (tmp == -1)
            continue;
        
        i += (size_t) tmp;
    }

    return (size_t) i;
}

static ssize_t klog_countln_split (klog_file_split_t *sf)
{
    size_t len, i = 0;

    dbg_return_if (sf == NULL, -1); 

    if (sf->fp == NULL)
    {
        /* TODO should open for reading */
        return -1;
    }
    
    while (fgetln(sf->fp, &len))
        i++;

    dbg_err_if (ferror(sf->fp));

    return (ssize_t) i;
err:
    return -1;
}

static int klog_file_split_new (int id, klog_file_split_t **pfs)
{
    klog_file_split_t *fs;
    
    dbg_return_if (pfs == NULL, ~0);
    dbg_return_if (id < 0, ~0);

    fs = u_zalloc(sizeof(klog_file_split_t));
    dbg_return_if (fs == NULL, ~0);

    fs->id = id;
    fs->fp = NULL;
    fs->nlines = 0;
     
    *pfs = fs;

    return 0;
}

