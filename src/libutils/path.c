/*
 * Copyright (c) 2005, 2006, 2007 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: path.c,v 1.3 2007/10/26 10:01:09 tat Exp $
 */

#include "klone_conf.h"
#include <stdlib.h>
#include <string.h>
#include <u/libu.h>
#include <klone/utils.h>
#ifdef HAVE_STRINGS
#include <strings.h>
#endif

/**
 * \brief   Removes /./, /../ and // from the path
 * 
 * Clean ups a path removing /./, /../ and multiple consecutive
 * slashes from the given path.
 *
 * Note that the function modifies the 'path' buffer.
 *
 * \param   path        the path to normalize
 *
 * \return
 * - \c 0   successful
 * - \c ~0  error
 */
int u_uri_normalize(char *path)
{
    enum { SLASH = '/', BACKSLASH = '\\' };
    u_string_t *s = NULL;
    size_t len;
    char delim[2];
    char *pp, *tok, *src, *cs;
    int trsl = 0; /* trailing slash */

    dbg_err_if(path == NULL);

    /* convert backslashes to slashes */
    for(pp = path; *pp; ++pp)
        if(*pp == BACKSLASH)
            *pp = SLASH;

    /* must be an absolute path (i.e. must start with a slash) */
    dbg_err_if(path[0] != SLASH); 

    if(path[strlen(path)-1] == SLASH)
        trsl = 1;

    dbg_err_if(u_snprintf(delim, sizeof(delim), "%c", SLASH));

    dbg_err_if(u_string_create(NULL, 0, &s));

    /* alloc a reasonable buffer immediately */
    dbg_err_if(u_string_reserve(s, strlen(path) + 1));

    /* foreach name=value pair... */
    for(src = path; (tok = strtok_r(src, delim, &pp)) != NULL; src = NULL)
    {
        if(!strcmp(tok, ""))
            continue; /* double slash */
        else if(!strcmp(tok, "."))
            continue; /* /./ */
        else if(!strcmp(tok, "..")) {
            /* eat last dir */
            len = u_string_len(s);
            cs = u_string_c(s) + u_string_len(s) - 1;
            for(; len && *cs != SLASH; --len, --cs)
                continue;
            /* crop */
            dbg_err_if(u_string_set_length(s, (len ? --len : 0) ));
        } else {
            dbg_err_if(u_string_aprintf(s, "%c%s", SLASH, tok));
        }
    }

    if(!u_string_len(s) || trsl)
        dbg_err_if(u_string_aprintf(s, "%c", SLASH));

    /* copy out */
    strcpy(path, u_string_c(s));

    u_string_free(s);

    return 0;
err:
    if(s)
        u_string_free(s);
    return ~0;
}

