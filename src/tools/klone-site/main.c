#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <klone/klone.h>
#include <klone/debug.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/io.h>
#include <klone/translat.h>
#include <klone/utils.h>
#include <klone/str.h>
#include <klone/run.h>

#define err(args...) \
    do { dbg(args); fprintf(stderr, args); fprintf(stderr, "\n"); } while(0)

#define err_if(cond, args...) \
    do { if(cond) err(args);  } while(0)

#define die(args...)  \
    do { err(args); exit(EXIT_FAILURE); } while(0)

#define die_if(cond, args...)  \
    do { dbg_ifb(cond) die(args); } while(0)

typedef struct 
{
    char *root_dir;         /* path of of site root */
    char *base_uri;         /* base uri             */
    char **arg;             /* cmd line args array  */
    size_t narg;            /* # of cmd line args   */
    io_t *iom, *iod, *ior;  /* io makefile, io deps */
    int verbose;            /* verbose mode on/off  */
    size_t ndir, nfile;     /* dir and file count   */
} context_t;

/* global context */
static context_t g_ctx, *ctx = &g_ctx;

void usage(void)
{
    static const char * us = 
    "usage: klone-site [-b URI] -r root_dir                         \n"
    "             -h       display this help                      \n"
    "             -b       base URI                               \n"
    "             -r       root directory                         \n"
    ;

    fprintf(stderr, us);

    exit(EXIT_FAILURE);
}

void parse_opt(int argc, char **argv)
{
    int ret, len;

    while((ret = getopt(argc, argv, "hb:r:v")) != -1)
    {
        switch(ret)
        {
        case 'b': /* base URI */
            ctx->base_uri = u_strdup(optarg);
            break;
        case 'r': /* root directory */
            ctx->root_dir = u_strdup(optarg);
            break;
        case 'v': 
            ctx->verbose++;
            break;
        default:
        case 'h': 
            usage();
        }
    }
    /* sanity checks */
    die_if(!ctx->root_dir, "root directory (-r) is required");

    if(!ctx->base_uri)
        ctx->base_uri = u_strdup("");
    else  {
        if(ctx->base_uri[0] != '/')
            die("base uri must be absolute (i.e. must start with a '/')");
        len = strlen(ctx->base_uri);
        if(len && ctx->base_uri[len -1] == '/')
            ctx->base_uri[len - 1] = 0;
    }

    ctx->narg = argc - optind;
    ctx->arg = argv + optind;
}

static int cb_file(struct dirent *de, const char *path , void *arg)
{
    #define FILE_FMT "pg_%s.c"
    char *base_uri = (char*)arg;
    char uri_md5[MD5_DIGEST_BUFSZ];
    char file_in[NAME_BUFSZ], uri[URI_BUFSZ];

    ctx->nfile++;

    /* input file */
    u_snprintf(file_in, NAME_BUFSZ, "$(srcdir)/%s/%s", path , de->d_name);

    /* base uri */
    u_snprintf(uri, URI_BUFSZ, "%s/%s", base_uri, de->d_name);
    u_md5(uri, strlen(uri), uri_md5);

    if(ctx->verbose)
        printf("file: %s   uri: %s   md5: %s", de->d_name, uri, uri_md5);

    io_printf(ctx->iom, " \\\n" FILE_FMT, uri_md5);

    io_printf(ctx->ior, "KLONE_REGISTER(action,%s);\n", uri_md5);

    /* we're adding a '/' before the uri (that will be removed by klone.exe) 
     * to avoid MSYS (win32) automatic path translation oddity */
    io_printf(ctx->iod, 
            "\n" FILE_FMT ": %s\n\t$(KLONE) -t -i $< -o $@ -u /%s\n", 
            uri_md5, file_in, uri);

    return 0;
err:
    return ~0;
    #undef FILE_FMT
}

static int cb_dir(struct dirent *de, const char *path , void *arg)
{
    char dir[PATH_MAX], base_uri[URI_BUFSZ], *cur_uri = (char*)arg;

    ctx->ndir++;

    dbg_err_if(u_snprintf(dir, PATH_MAX, "%s/%s", path, de->d_name));

    dbg_err_if(u_snprintf(base_uri, URI_BUFSZ, "%s/%s", cur_uri, de->d_name));

    u_foreach_dir_item(dir, S_IFREG, cb_file, (void*)base_uri);

    u_foreach_dir_item(dir, S_IFDIR, cb_dir, (void*)base_uri);

    return 0;
err:
    return ~0;
}

static void print_register_header(io_t *out)
{
    io_printf(out, "void do_register(int);                      \n");
    io_printf(out, "void unregister_pages() { do_register(0); } \n");
    io_printf(out, "void register_pages() { do_register(1); }   \n");
    io_printf(out, "static void do_register(int action) {         \n");
    io_printf(out,
        "#define KLONE_REGISTER(a, md5)        \\\n"
        "    do {                            \\\n"
        "    void module_init_##md5();       \\\n"
        "    void module_term_##md5();       \\\n"
        "    if(a) module_init_##md5();      \\\n"
        "    else module_term_##md5();       \\\n"
        "    } while(0)                        \n");
}

static void print_register_footer(io_t *out)
{
    io_printf(out, "#undef KLONE_REGISTER\n");
    io_printf(out, "}\n");
}

static int trans_site()
{
    /* makefile */
    dbg_err_if(u_file_open("autogen.mk", O_CREAT | O_TRUNC | O_WRONLY, 
                &ctx->iom));
    dbg_err_if(u_file_open("autogen.dps", O_CREAT | O_TRUNC | O_WRONLY, 
                &ctx->iod));

    dbg_err_if(u_file_open("register.c", O_CREAT | O_TRUNC | O_WRONLY, 
                &ctx->ior));

    print_register_header(ctx->ior);

    io_printf(ctx->iom, "embfs_rootdir=%s\n", ctx->root_dir);
    io_printf(ctx->iom, "autogen_src= ");

    /* input files to C */
    u_foreach_dir_item(ctx->root_dir, S_IFREG, cb_file, (void*)ctx->base_uri);
    u_foreach_dir_item(ctx->root_dir, S_IFDIR, cb_dir, (void*)ctx->base_uri);

    print_register_footer(ctx->ior);

    io_free(ctx->ior);
    io_free(ctx->iod);
    io_free(ctx->iom);

    return 0;
err:
    if(ctx->ior)
        io_free(ctx->ior);
    if(ctx->iod)
        io_free(ctx->iod);
    if(ctx->iom)
        io_free(ctx->iom);
    return ~0;
}

int main(int argc, char **argv)
{
    memset(ctx, 0, sizeof(context_t));

    /* parse command line switches */
    parse_opt(argc, argv);

    dbg_err_if(trans_site());

    printf("%u dirs and %u files imported\n", ctx->ndir, ctx->nfile);

    return EXIT_SUCCESS;
err:
    return EXIT_FAILURE;
}
