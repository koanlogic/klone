/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: vhost.h,v 1.3 2007/11/09 22:06:26 tat Exp $
 */

#ifndef _KLONE_VHOST_H_
#define _KLONE_VHOST_H_

#include <u/libu.h>

#ifdef __cplusplus
extern "C" {
#endif 

/* define page list */
LIST_HEAD(vhost_list_s, vhost_s);

struct http_s;
struct klog_s;

typedef struct vhost_s
{
    LIST_ENTRY(vhost_s) np; /* next & prev pointers                          */
    struct http_s *http;    /* parent http object                            */
    struct klog_s *klog;    /* vhost logging facility                        */
    u_config_t *config;     /* vhost configuration                           */
    u_config_t *al_config;  /* cached access_log config ptr                  */
    const char *host;       /* hostname                                      */
    int id;                 /* position in the vhosts list                   */

    /* cached configuration options */
    const char *server_sig; /* server signature                              */
    const char *dir_root;   /* base html directory                           */
    const char *index;      /* user-provided index page(s)                   */
    int send_enc_deflate;   /* >0 if sending deflated content is not disabled*/
} vhost_t;

typedef struct vhost_list_s vhost_list_t;

int vhost_create(vhost_t **pv);
int vhost_free(vhost_t *v);

int vhost_list_create(vhost_list_t **pvs);
int vhost_list_free(vhost_list_t *vs);
int vhost_list_add(vhost_list_t *vs, vhost_t *vhost);
vhost_t* vhost_list_get_n(vhost_list_t *vs, int n);
vhost_t* vhost_list_get(vhost_list_t *vs, const char *host);

#ifdef __cplusplus
}
#endif 

#endif
