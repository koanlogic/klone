/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: cgi.c,v 1.8 2006/01/09 12:38:38 tat Exp $
 */

#include "klone_conf.h"
#include <u/libu.h>
#include <stdlib.h>
#include <klone/cgi.h>
#include <klone/request.h>

int cgi_set_request(request_t *rq)
{
    char *ev;

    /* clear uri-related request fields */
    request_clear_uri(rq);

    dbg_err_if (rq == NULL);

    /* dbg_err_if(request_set_filename(rq, getenv("SCRIPT_NAME"))); */

    /* use PATH_INFO as script name i.e. 
        http://site/cgi-bin/kloned/path/to/script.kl1 */
    if((ev = getenv("PATH_INFO")) != NULL)
        dbg_err_if(request_set_filename(rq, ev));
    else
        dbg_err_if(request_set_filename(rq, "/"));

    if((ev = getenv("PATH_INFO")) != NULL)
        dbg_err_if(request_set_path_info(rq, ev));

    if((ev = getenv("QUERY_STRING")) != NULL)
        dbg_err_if(request_set_query_string(rq, ev));

    if((ev = getenv("REQUEST_METHOD")) != NULL)
        dbg_err_if(request_set_method(rq, ev));

    if((ev = getenv("CONTENT_TYPE")) != NULL)
        dbg_err_if(request_set_field(rq, "Content-Type", ev));

    if((ev = getenv("CONTENT_LENGTH")) != NULL)
        dbg_err_if(request_set_field(rq, "Content-Length", ev));

    return 0;
err:
    return ~0;
}
