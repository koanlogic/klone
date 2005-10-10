#include <fcntl.h>
#include <err.h>
#include <klone/debug.h>
#include <klone/config.h>
#include <klone/klog.h>
#include <klone/io.h>


char *g_conf = "./log.conf";
int g_verbose = 0;
int g_ntimes = 1;

static void usage (void);
static int load_conf (const char *cf, klog_args_t **ka);
static int stress_test (klog_t *kl, int ntimes);

int main (int argc, char *argv[])
{
    char c;
    klog_args_t *ka = NULL;
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

    dbg_err_if (load_conf(g_conf, &ka));
    dbg_err_if (klog_open(ka, &kl));
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

        if (g_verbose && kl->type != KLOG_TYPE_SYSLOG)
            cmsg("number of msgs in memory log: %d", n = klog_countln(kl));

        if (kl->type != KLOG_TYPE_SYSLOG)
        {
            for (i = 1; i <= n; i++)
            {
                klog_getln(kl, i, ln);
                if (g_verbose)
                cmsg("klog_getln(%d)\t\'%s\'", i, ln);
            }
        }

        klog_clear(kl);

        if (g_verbose && kl->type != KLOG_TYPE_SYSLOG)
            cmsg("number of msgs in memory log: %d", n = klog_countln(kl));
    }

    return 0;
}

static int load_conf (const char *cf, klog_args_t **ka)
{
    config_t *c = NULL, *l = NULL;
    io_t *io = NULL;

    dbg_return_if (ka == NULL, ~0);
    
    dbg_err_if (u_file_open(cf, O_RDONLY, &io));
    dbg_err_if (config_create(&c));
    dbg_err_if (config_load(c, io, 0));
    dbg_err_if ((l = config_get_child(c, "log")) == NULL);
    dbg_err_if (klog_args(l, ka));
    if (g_verbose)
        klog_args_print(stdout, *ka);
    
    return 0;
err:
    if (c)
        config_free(c);
    if (io)
        io_free(io);
    return ~0;
}

static void usage (void)
{
    static const char *us =
        "test [-f cf_file] [-n ntimes] [-v]";

    fprintf(stderr, "%s\n", us);
    exit(1);
}
