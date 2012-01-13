/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: main.c,v 1.7 2006/01/09 12:38:38 tat Exp $
 */

#include <fcntl.h>
#include <err.h>
#include <syslog.h>
#include <u/libu.h>
#include <klone/klog.h>
#include <klone/io.h>


char *g_conf = "./log.conf";
int g_verbose = 0;
int g_ntimes = 1;

int facility = LOG_LOCAL0;

static void usage (void);
static int load_conf (const char *cf, klog_t **pkl);
static int stress_test (klog_t *kl, int ntimes);

int main (int argc, char *argv[])
{
    char c;
    klog_t *kl = NULL;

    while ((c = getopt(argc, argv, "f:n:v")) != -1)
        switch (c)
        {
            case 'f':
                g_conf = optarg; 
                break;
            case 'n':
                g_ntimes = atoi(optarg);
                break;
            case 'v':
                g_verbose++;
                break;
            default:
                usage();    
        }

    dbg_err_if (load_conf(g_conf, &kl));
    dbg_err_if (stress_test(kl, g_ntimes)); 
    klog_close(kl);

    return 0;
err:
    errx(1, "test failed");
    return 1;
}

static int stress_test (klog_t *kl, int ntimes)
{
    int i;
    ssize_t n;
    char ln[KLOG_LN_SZ + 1];

    dbg_return_if (ntimes <= 0, ~0);

    while (ntimes--)
    {
        for (i = 0; i < 5; i++)
        {
            dbg_if (klog(kl, KLOG_DEBUG, "this is debug message n %d", i)); 
            dbg_if (klog(kl, KLOG_INFO, "this is info message n %d", i)); 
            dbg_if (klog(kl, KLOG_NOTICE, "this is notice message n %d", i)); 
            dbg_if (klog(kl, KLOG_WARNING, "this is warning message n %d", i)); 
            dbg_if (klog(kl, KLOG_ERR, "this is err message n %d", i)); 
            dbg_if (klog(kl, KLOG_CRIT, "this is crit message n %d", i)); 
            dbg_if (klog(kl, KLOG_ALERT, "this is alert message n %d", i)); 
            dbg_if (klog(kl, KLOG_EMERG, "this is emerg message n %d", i)); 
        }

        if (g_verbose)
            u_con("number of msgs in memory log: %d", n = klog_countln(kl));

        for (i = 1; i <= n; i++)
        {
            int rc = klog_getln(kl, i, ln);
            if (rc == 0 && g_verbose)
                u_con("klog_getln(%d)\t\'%s\'", i, ln);
        }

        (void) klog_clear(kl);

        if (g_verbose)
            u_con("number of msgs in memory log: %d", n = klog_countln(kl));
    }

    return 0;
}

static int load_conf (const char *cf, klog_t **pkl)
{
    u_config_t *c = NULL, *l = NULL;
    int fd = -1;

    dbg_return_if (pkl == NULL, ~0);
    dbg_return_if (cf == NULL, ~0);
    
    fd = open(cf, O_RDONLY, 0600);
    dbg_err_if(fd < 0);

    dbg_err_if (u_config_create(&c));
    dbg_err_if (u_config_load(c, fd, 0));
    dbg_err_if ((l = u_config_get_child(c, "log")) == NULL);
    dbg_err_if (klog_open_from_config(l, pkl));

    close(fd);
    
    return 0;
err:
    if (c)
        u_config_free(c);
    if (fd)
        close(fd);
    return ~0;
}

static void usage (void)
{
    static const char *us =
        "test [-f cf_file] [-n ntimes] [-v]";

    fprintf(stderr, "%s\n", us);
    exit(1);
}
