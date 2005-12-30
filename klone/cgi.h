/*
 * Copyright (c) 2005 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: cgi.h,v 1.5 2005/12/30 17:21:53 tat Exp $
 */

#ifndef _KLONE_CGI_H_
#define _KLONE_CGI_H_

#include <klone/request.h>

#ifdef __cplusplus
extern "C" {
#endif 

int cgi_set_request(request_t *rq);

#ifdef __cplusplus
}
#endif 

#endif
