#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <unistd.h>
#include <klone/klone.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/translat.h>
#include <klone/utils.h>
#include <klone/run.h>
#include <klone/mime_map.h>
#include <u/libu.h>
#include "conf.h"

int facility = LOG_LOCAL0;

/* command list enums */
enum command_e { CMD_UNKNOWN, CMD_TRANS, CMD_IMPORT };

/* runtime flags */
enum flags_e { FLAG_NONE, FLAG_VERBOSE };

typedef struct 
{
    char *file_in, *file_out;   /* [trans] input, output file   */
    char *uri;                  /* [trans] translated file uri  */
    int verbose;                /* >0 when verbose mode is on   */
    char **arg;                 /* argv                         */
    size_t narg;                /* argc                         */
    int cmd;                    /* command to run               */
    char *base_uri;             /* site base uri                */
    io_t *iom, *iod, *ior;      /* io makefile, io deps         */
    size_t ndir, nfile;         /* dir and file count           */
} context_t;

context_t *ctx;

#define KL1_FILE_FMT "pg_%s.c"

static void usage(void)
{
    static const char * us = 
"Usage: klone [-hvV] -c COMMAND OPTIONS ARGUMENTS                           \n"
"Version: %s - Copyright (c) 2005 KoanLogic s.r.l. - All rights reserved.   \n"
"\n"
"    -h          display this help                                          \n"
"    -v          verbose mode                                               \n"
"    -V          print klone version and exit                               \n"
"    -c          command                                                    \n"
"\n"
"    COMMAND LIST:                                                          \n"
"          import        import a directory tree in the embedded filesystem \n"
"          translate     convert a file or a dynamic web page to a C file   \n"
"\n"
"    COMMANDS SYNTAX:                                                       \n"
"\n"
"          import [-b URI] dir                                              \n"
"            -b URI      base URI                                           \n"
"            dir         directory tree path                                \n"
"\n"
"          translate -i in -o out -u URI                                    \n"
"            -u URI      URI of translated page                             \n"
"            -i file     input file                                         \n"
"            -o file     output file                                        \n"
"\n";
;


    fprintf(stderr, us, KLONE_VERSION);

    exit(EXIT_FAILURE);
}

static void version(void)
{
    static const char *vv = 
    "klone %s - Copyright (c) 2005 KoanLogic s.r.l. - All rights reserved. \n"
    "\n";

    fprintf(stderr, vv, KLONE_VERSION);

    exit(EXIT_FAILURE);
}

static void remove_trailing_slash(char *s)
{
    size_t len;
    len = strlen(s);
    if(len && s[len - 1] == '/')
        s[len - 1] = 0;
}

static int parse_opt(int argc, char **argv)
{
    int ret;

    if(argc == 1)
        usage();

    while((ret = getopt(argc, argv, "hvVb:i:o:u:c:")) != -1)
    {
        switch(ret)
        {
        case 'v': /* verbose on */
            ctx->verbose = 1;
            break;
        case 'V': /* verbose on */
            version();
            break;
        case 'c': /* command */
            if(!strcasecmp(optarg, "import"))
                ctx->cmd = CMD_IMPORT;
            else if(!strcasecmp(optarg, "translate"))
                ctx->cmd = CMD_TRANS;
            else
                con_err("unknown command: %s", optarg);
            break;
        case 'i': /* input file */
            ctx->file_in = u_strdup(optarg);
            warn_err_if(ctx->file_in == NULL);
            break;
        case 'b': /* base_uri */
            ctx->base_uri = u_strdup(optarg);
            warn_err_if(ctx->base_uri == NULL);

            if(ctx->base_uri[0] != '/')
                klone_die("base URI must be absolute "
                          "(i.e. must start with a '/')");

            remove_trailing_slash(ctx->base_uri);

            break;
        case 'o': /* output file */
            ctx->file_out = u_strdup(optarg);
            warn_err_if(ctx->file_out == NULL);
            break;
        case 'u': /* translated page uri */
            /* skip the first char to avoid MSYS path translation bug
             * (see klone-site.c) */
            ctx->uri = u_strdup(1+optarg);
            warn_err_if(ctx->uri == NULL);

            if(ctx->uri[0] != '/')
                klone_die("URI must be absolute (i.e. must start with a '/')");

            remove_trailing_slash(ctx->uri);

            break;
        default:
        case 'h': 
            usage();
        }
    }

    klone_die_if(ctx->cmd == 0, "missing command argument (-c)");
    ctx->narg = argc - optind;  /* # of args left */
    ctx->arg = argv + optind;   

    return 0;
err:
    return ~0;
}

static int command_trans(void)
{
    trans_info_t ti;
    const mime_map_t *mm;
    struct stat st;

    if(ctx->narg != 0)
        usage();    /* no argument allowed */

    klone_die_if(!ctx->file_in, "input file name required (-i file)");
    klone_die_if(!ctx->file_out, "output file name required (-o file)");
    klone_die_if(!ctx->uri, "translated page URI required (-u uri)");

    if(ctx->verbose)
        con("translating %s to %s (uri: %s)", ctx->file_in, ctx->file_out, 
            ctx->uri);

    /* input file */
    strncpy(ti.file_in, ctx->file_in, NAME_BUFSZ);

    /* output file */
    strncpy(ti.file_out, ctx->file_out, NAME_BUFSZ);

    /* uri */
    strncpy(ti.uri, ctx->uri, MIME_BUFSZ);

    /* mime_type */
    if((mm = u_get_mime_map(ctx->file_in)) != NULL)
    {
        strncpy(ti.mime_type, mm->mime_type, MIME_BUFSZ);
        ti.comp = mm->comp;
    } else {
        /* unable to guess mime type, use default */
        strncpy(ti.mime_type, "application/octect-stream", MIME_BUFSZ);
        ti.comp = 1; /* compress it, libz never enlarge uncompressable files  */
    }

    /* be sure that the input file exists */
    klone_die_if(stat(ctx->file_in, &st), "input file not found");

    ti.file_size = st.st_size;
    ti.mtime = st.st_mtime; 

    /* translate it */
    dbg_err_if(translate(&ti));

    return 0;
err:
    con("translate error");
    return ~0;
}

static int cb_file(struct dirent *de, const char *path , void *arg)
{
    char uri_md5[MD5_DIGEST_BUFSZ];
    char file_in[NAME_BUFSZ], uri[URI_BUFSZ], *base_uri = (char*)arg;

    ctx->nfile++;

    /* input file */
    u_snprintf(file_in, NAME_BUFSZ, "$(srcdir)/%s/%s", path , de->d_name);

    /* base uri */
    u_snprintf(uri, URI_BUFSZ, "%s/%s", base_uri, de->d_name);
    u_md5(uri, strlen(uri), uri_md5);

    if(ctx->verbose)
        con("%s -> %s", file_in + strlen("$(srcdir)"), uri);

    dbg_err_if(io_printf(ctx->iom, " \\\n" KL1_FILE_FMT, uri_md5) < 0);

    dbg_err_if(io_printf(ctx->ior, "KLONE_REGISTER(action,%s);\n", uri_md5) <0);

    /* we're adding a '/' before the uri (that will be removed by klone.exe) 
     * to avoid MSYS (win32) automatic path translation oddity */
    dbg_err_if(io_printf(ctx->iod, 
            "\n" KL1_FILE_FMT 
            ": %s\n\t$(KLONE) -c translate -i $< -o $@ -u /%s\n", 
            uri_md5, file_in, uri) < 0);

    return 0;
err:
    return ~0;
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

static int print_register_header(io_t *out)
{
    dbg_err_if(io_printf(out, "void do_register(int);\n") < 0);
    dbg_err_if(io_printf(out, 
        "void unregister_pages() { do_register(0); }\n") < 0);
    dbg_err_if(io_printf(out, 
        "void register_pages() { do_register(1); }\n") < 0);
    dbg_err_if(io_printf(out, 
        "static void do_register(int action) {\n") < 0);
    dbg_err_if(io_printf(out,
        "#define KLONE_REGISTER(a, md5)  \\\n"
        "    do {                        \\\n"
        "    void module_init_##md5();   \\\n"
        "    void module_term_##md5();   \\\n"
        "    if(a) module_init_##md5();  \\\n"
        "    else module_term_##md5();   \\\n"
        "    } while(0)                    \n") < 0);

    return 0;
err:
    return ~0;
}

static int print_register_footer(io_t *out)
{
    dbg_err_if(io_printf(out, "#undef KLONE_REGISTER\n") < 0);
    dbg_err_if(io_printf(out, "}\n") < 0);

    return 0;
err:
    return ~0;
}

static int trans_site(const char *root_dir, const char *base_uri)
{
    /* makefile */
    dbg_err_if(u_file_open("autogen.mk", O_CREAT | O_TRUNC | O_WRONLY, 
                &ctx->iom));
    dbg_err_if(u_file_open("autogen.dps", O_CREAT | O_TRUNC | O_WRONLY, 
                &ctx->iod));

    dbg_err_if(u_file_open("register.c", O_CREAT | O_TRUNC | O_WRONLY, 
                &ctx->ior));

    dbg_err_if(print_register_header(ctx->ior));

    dbg_err_if(io_printf(ctx->iom, "embfs_rootdir=%s\n", root_dir) < 0);
    dbg_err_if(io_printf(ctx->iom, "autogen_src= ") < 0);

    /* for each file call cb_file */
    u_foreach_dir_item(root_dir, S_IFREG, cb_file, base_uri);
    /* for each directory call cb_dir */
    u_foreach_dir_item(root_dir, S_IFDIR, cb_dir, base_uri);

    dbg_err_if(print_register_footer(ctx->ior));

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

static int command_import(void)
{
    const char *root_dir, *base_uri;

    if(ctx->narg != 1)
        usage();    /* just on directory expected */

    root_dir = ctx->arg[0];
    dbg_err_if(root_dir == NULL);

    if((base_uri = ctx->base_uri) == NULL)
        base_uri = "";

    dbg_err_if(trans_site(root_dir, base_uri));

    con("%lu dirs and %lu files imported", ctx->ndir, ctx->nfile);

    return 0;
err:
    con("import error");
    return ~0;
}

static int dispatch_command(void)
{
    switch(ctx->cmd)
    {
    case CMD_TRANS:
        dbg_err_if(command_trans());
        break;
    case CMD_IMPORT:
        dbg_err_if(command_import());
        break;
    default:
        con_err("unknown command");
    }

    return 0;
err:
    return ~0;
}

int main(int argc, char **argv)
{
    context_t context;

    ctx = &context;

    /* zero-out the context */
    memset(ctx, 0, sizeof(context_t));

    /* parse command line switches and set ctx->cmd and params */
    dbg_err_if(parse_opt(argc, argv));

    /* run the command */
    dbg_err_if(dispatch_command());

    return EXIT_SUCCESS;
err:
    return EXIT_FAILURE;
}
