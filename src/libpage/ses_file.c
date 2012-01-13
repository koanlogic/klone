/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: ses_file.c,v 1.21 2008/10/18 13:04:02 tat Exp $
 */

#include "klone_conf.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <u/libu.h>
#include <klone/session.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/vars.h>
#include <klone/utils.h>
#include <klone/ses_prv.h>

static int session_file_save(session_t *ss)
{
    io_t *io = NULL;

    dbg_return_if (ss == NULL, ~0);

    /* delete old file (if any), we'll rewrite it from scratch */
    (void) u_remove(ss->filename);

    if(vars_count(ss->vars) == 0)
        return 0; /* nothing to save */

    /* FIXME may be busy, must retry */
    dbg_err_if(u_file_open(ss->filename, O_WRONLY | O_CREAT, &io));

    dbg_err_if(session_prv_save_to_io(ss, io));

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int session_file_load(session_t *ss)
{
    io_t *io = NULL;

    dbg_err_if (ss == NULL);
    dbg_err_if (ss->filename == NULL || ss->filename[0] == '\0');

    /* FIXME may be busy, must retry */
    dbg_err_if(u_file_open(ss->filename, O_RDONLY | O_CREAT, &io));

    dbg_err_if(session_prv_load_from_io(ss, io));

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int session_file_term(session_t *ss)
{
    u_unused_args(ss);
    return 0;
}

static int session_file_remove(session_t *ss)
{
    dbg_return_if (ss == NULL, ~0);

    dbg_if(u_remove(ss->filename));

    return 0;
}

int session_file_create(session_opt_t *so, request_t *rq, response_t *rs, 
        session_t **pss)
{
    session_t *ss = NULL;
    time_t now;
    struct stat st;

    dbg_err_if (so == NULL);
    dbg_err_if (rq == NULL);
    dbg_err_if (rs == NULL);
    dbg_err_if (pss == NULL);

    ss = u_zalloc(sizeof(session_t));
    dbg_err_if(ss == NULL);

    ss->filename[0] = '\0';
    ss->load = session_file_load;
    ss->save = session_file_save;
    ss->remove = session_file_remove;
    ss->term = session_file_term;
    ss->so = so;

    dbg_err_if(session_prv_init(ss, rq, rs));

    if (ss->filename[0] != '\0' && stat(ss->filename, &st) == 0)
        ss->mtime = st.st_mtime;
    else
    {
        dbg_err_if ((now = time(NULL)) == (time_t) -1);
        ss->mtime = (int) now;
    }

    *pss = ss;

    return 0;
err:
    if(ss)
        session_free(ss);
    return ~0;
}

int session_file_module_term(session_opt_t *so)
{
    u_unused_args(so);
    return 0;
}

int session_file_module_init(u_config_t *config, session_opt_t *so)
{
    const char *v;

    /* config can't be NULL */
    dbg_return_if (so == NULL, ~0);
    
    if(config && (v = u_config_get_subkey_value(config, "file.path")) != NULL)
    {
        (void) u_strlcpy(so->path, v, sizeof so->path);
    } else {
        /* default */
        #ifdef OS_WIN
        GetTempPath(sizeof so->path, so->path);
        #else
        (void) u_strlcpy(so->path, "/tmp", sizeof so->path);
        #endif
    }

    return 0;
}

