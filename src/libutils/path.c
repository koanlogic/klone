/*
 * Copyright (c) 2005, 2006, 2007 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: path.c,v 1.1 2007/10/17 22:58:35 tat Exp $
 */

#include "klone_conf.h"
#include <stdlib.h>
#include <string.h>
#include <u/libu.h>
#include <klone/utils.h>
#ifdef HAVE_STRINGS
#include <strings.h>
#endif

/* removes /./, /../, // (and windows backslash equivs) 
 * see test/path.c for examples */
int u_path_normalize(char *path)
{
    u_string_t *s = NULL;
    size_t len;
    char delim[2];
    char *pp, *tok, *src, *cs;
    int trsl = 0; /* trailing slash */

    dbg_err_if(path == NULL);

    /* must be an absolute path (i.e. must start with a slash) */
    dbg_err_if(path[0] != U_PATH_SEPARATOR); 

    if(path[strlen(path)-1] == U_PATH_SEPARATOR)
        trsl = 1;

    dbg_err_if(u_snprintf(delim, sizeof(delim), "%c", U_PATH_SEPARATOR));

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
            for(; len && *cs != U_PATH_SEPARATOR; --len, --cs)
                continue;
            /* crop */
            dbg_err_if(u_string_set_length(s, (len ? --len : 0) ));
        } else {
            dbg_err_if(u_string_aprintf(s, "%c%s", U_PATH_SEPARATOR, tok));
        }
    }

    if(!u_string_len(s) || trsl)
        dbg_err_if(u_string_aprintf(s, "%c", U_PATH_SEPARATOR));

    /* copy out */
    strcpy(path, u_string_c(s));

    u_string_free(s);

    return 0;
err:
    if(s)
        u_string_free(s);
    return ~0;
}
