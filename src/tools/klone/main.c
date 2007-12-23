/*
 * Copyright (c) 2005, 2006 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: main.c,v 1.43 2007/12/23 10:28:45 tat Exp $
 */

#include "klone_conf.h"
#include <sys/stat.h>
#ifdef HAVE_SYS_DIR
#include <sys/dir.h>
#endif
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD
#include <unistd.h>
#endif
#ifdef HAVE_GETOPT
#include <getopt.h>
#endif
#include <u/libu.h>
#include <klone/os.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/translat.h>
#include <klone/utils.h>
#include <klone/run.h>
#include <klone/mime_map.h>
#include <klone/version.h>
#include "pm.h"

int facility = LOG_LOCAL0;

/* command list enums */
enum command_e { CMD_UNKNOWN, CMD_TRANS, CMD_IMPORT };

/* runtime flags */
enum flags_e { FLAG_NONE, FLAG_VERBOSE };

typedef struct 
{
    char *file_in, *file_out;   /* [trans] input, output file       */
    char *uri;                  /* [trans] translated file uri      */
    int verbose;                /* >0 when verbose mode is on       */
    char **arg;                 /* argv                             */
    size_t narg;                /* argc                             */
    int cmd;                    /* command to run                   */
    char *base_uri;             /* site base uri                    */
    int encrypt;                /* >0 when encryption is enabled    */
    int compress;               /* >0 when compress is enabled      */
    pm_t *comp_patt;            /* compress file pattern            */
    pm_t *enc_patt;             /* encrypt file pattern             */
    pm_t *excl_patt;            /* exclude file pattern             */
    char *key_file;             /* encryption key file name         */
    io_t *iom, *iod, *ior;      /* io makefile, io deps             */
    size_t ndir, nfile;         /* dir and file count               */
    size_t nexcl;               /* # of excluded files (-x)         */
} context_t;

context_t *ctx;

#define KL1_FILE_FMT "pg_%s.%s"

static void usage(void)
{
    static const char * us = 
"Usage: klone [-hvV] -c COMMAND OPTIONS ARGUMENTS\n"
"Version: %s - Copyright (c) 2005, 2006, 2007 KoanLogic s.r.l.\n"
"All rights reserved.\n"
"\n"
"       -h            display this help\n"
"       -v            verbose mode\n"
"       -V            print KLone version and exit\n"
"       -c command    command to execute (see COMMAND LIST)\n"
"\n"
"\n"
"    COMMAND LIST:\n"
"       import        import a directory tree in the embedded filesystem\n"
"       translate     convert a file or a dynamic web page to a C file\n"
"\n"
"\n"
"    COMMANDS SYNTAX:\n"
"\n"
"       import OPTIONS dir\n"
"         -b URI      base URI\n"
"         -x pattern  exclude all files whose URI match the given pattern (*)\n"
#ifdef HAVE_LIBOPENSSL
"         -e pattern  encrypt all files whose URI match the given pattern (*)\n"
"         -k key_file encryption key filename\n"
#endif
#ifdef HAVE_LIBZ
"         -z          compress all compressable content (based on MIME types)\n"
"         -Z pattern  compress all files whose URI match the given pattern (*)\n"
#endif
"         dir         directory tree path\n"
"\n"
"         (*) may be used more then once\n"
"\n"
"       translate OPTIONS\n"
#ifdef HAVE_LIBOPENSSL
"         -E          encrypt file content\n"
#endif
"         -i file     input file\n"
#ifdef HAVE_LIBOPENSSL
"         -k key_file encryption key filename\n"
#endif
"         -o file     output file\n"
"         -u URI      URI of translated page\n"
"                     (KLONE_CIPHER_KEY environ var is used if not provided)\n"
#ifdef HAVE_LIBZ
"         -z          compress file content\n"
#endif
"\n";

    fprintf(stderr, us, klone_version());

    exit(EXIT_FAILURE);
}

static void remove_trailing_slash(char *s)
{
    size_t len;
    
    dbg_ifb (s == NULL) return;
    
    len = strlen(s);
    if(len && s[len - 1] == '/')
        s[len - 1] = 0;
}

static int parse_opt(int argc, char **argv)
{
    int ret;
    char opts[512];

    if(argc == 1)
        usage();

    /* common switches */
    strcpy(opts, "hvVx:b:i:o:u:c:");

    /* encryption switches */
#ifdef HAVE_LIBOPENSSL
    strcat(opts, "k:e:E");
#endif

    /* compression switches */
#ifdef HAVE_LIBZ
    strcat(opts, "zZ:");
#endif

    while((ret = getopt(argc, argv, opts)) != -1)
    {
        switch(ret)
        {
        case 'v': /* verbose on */
            ctx->verbose++;
            break;
        case 'V': /* print name/version info and exit */
            u_print_version_and_exit();
            break;

#ifdef HAVE_LIBOPENSSL
        case 'E': /* encryption on */
            ctx->encrypt = 1;
            break;
        case 'e': /* encrypt file pattern */
            dbg_err_if(pm_add(ctx->enc_patt, u_strdup(optarg)));
            break;
        case 'k': /* encryption key filename */
            ctx->key_file = u_strdup(optarg);
            warn_err_if(ctx->key_file == NULL);
            break;
#endif

#ifdef HAVE_LIBZ
        case 'Z': /* compress file pattern */
            ctx->compress = 1;
            dbg_err_if(pm_add(ctx->comp_patt, u_strdup(optarg)));
            break;
        case 'z': /* compress */
            ctx->compress = 1;
            break;
#endif
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
        case 'x': /* exclude pattern */
            dbg_err_if(pm_add(ctx->excl_patt, u_strdup(optarg)));
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

static int set_key_from_file(trans_info_t *pti, const char *key_file)
{
    io_t *io = NULL;

    dbg_err_if (pti == NULL);
    dbg_err_if (key_file == NULL);
    
    dbg_err_if(u_file_open(key_file, O_RDONLY, &io));

    dbg_err_if(io_read(io, pti->key, CODEC_CIPHER_KEY_SIZE) <= 0);
    
    io_free(io);

    return 0;
err:
    return ~0;
}

static int command_trans(void)
{
    trans_info_t ti;
    const mime_map_t *mm;
    struct stat st;
    char *key_env;
    int key_found = 0;

    if(ctx->narg != 0)
        usage();    /* no argument allowed */

    memset(&ti, 0, sizeof(trans_info_t));

    klone_die_if(!ctx->file_in, "input file name required (-i file)");
    klone_die_if(!ctx->file_out, "output file name required (-o file)");
    klone_die_if(!ctx->uri, "translated page URI required (-u uri)");

    if(ctx->verbose)
        con("translating %s to %s (uri: %s)", ctx->file_in, ctx->file_out, 
            ctx->uri);

    /* input file */
    strncpy(ti.file_in, ctx->file_in, U_FILENAME_MAX);

    /* output file */
    strncpy(ti.file_out, ctx->file_out, U_FILENAME_MAX);

    /* uri */
    strncpy(ti.uri, ctx->uri, URI_BUFSZ);

    /* zero out the key (some byte could not be overwritten with small keys) */
    memset(ti.key, 0, CODEC_CIPHER_KEY_SIZE);

    /* sanity checks */
    con_err_ifm(ctx->key_file && !ctx->encrypt, "-k used but -E is missing");

    /* encryption key */
    key_env = getenv("KLONE_CIPHER_KEY");
    if(key_env && strlen(key_env))
    {
        key_found = 1;
        strncpy(ti.key, key_env, CODEC_CIPHER_KEY_SIZE);
    }

    /* if -k has been used the overwrite KLONE_CIPHER_KEY env var (if present)*/
    if(ctx->key_file)
    {
        key_found = 1;
        con_err_ifm(set_key_from_file(&ti, ctx->key_file), 
            "error reading key file [%s]", ctx->key_file);
    }

    if(ctx->encrypt)
    {
        if(!key_found)
            con_err("encryption key required (use -k or KLONE_CIPHER_KEY "
                    "environ variable)");
        ti.encrypt = 1;
    }

    /* set MIME type */
    if((mm = u_get_mime_map(ctx->file_in)) != NULL)
        strncpy(ti.mime_type, mm->mime_type, MIME_BUFSZ);
    else
        strncpy(ti.mime_type, "application/octet-stream", MIME_BUFSZ);

    /* compress if requested and the file is compressable (by MIME type) */
    if(ctx->compress)
        ti.comp = 1;

    /* be sure that the input file exists */
    klone_die_if(stat(ctx->file_in, &st), "input file not found");

    ti.file_size = st.st_size;
    ti.mtime = st.st_mtime; 

    /* translate it */
    dbg_err_if(translate(&ti));

    return 0;
err:
    /* delete output file on error */
    u_remove(ti.file_out);
    con(" ");
    return ~0;
}

static int is_cpp(const char *file_in)
{
    size_t l;

    dbg_err_if (file_in == NULL);

    l = strlen(file_in);
    if(l < 4)
        return 0;

    /* if the file name ends with "[Cc][Cc]" consider it a c++ file */
    if(tolower(file_in[--l]) == 'c' && tolower(file_in[--l]) == 'c')
        return 1; /* c++ */

err:
    return 0;
}

static int cb_file(struct dirent *de, const char *path , void *arg)
{
    static const char *prefix = "$(srcdir)";
    const mime_map_t *mm;
    char uri_md5[MD5_DIGEST_BUFSZ];
    char file_in[U_FILENAME_MAX], uri[URI_BUFSZ], *base_uri = (char*)arg;
    const char *ext;
    int enc = 0, zip = 0;

    dbg_err_if (de == NULL);
    dbg_err_if (path == NULL);
    dbg_err_if (arg == NULL);

    /* input file */
    if(path[0] == '/' || path[0] == '\\')
    {   /* absolute path */
        dbg_err_if(u_snprintf(file_in, U_FILENAME_MAX, "%s/%s", path, 
            de->d_name));
    } else if(isalpha(path[0]) && path[1] == ':') {
        /* absolute path Windows (X:/....) */
        dbg_err_if(u_snprintf(file_in, U_FILENAME_MAX, "%s/%s", path, 
            de->d_name));
    } else {
        /* relative path, use $(srcdir) */
        dbg_err_if(u_snprintf(file_in, U_FILENAME_MAX, "%s/%s/%s", prefix, 
            path, de->d_name));
    }

    /* base uri */
    dbg_err_if(u_snprintf(uri, URI_BUFSZ, "%s/%s", base_uri, de->d_name));
    dbg_err_if(u_md5(uri, strlen(uri), uri_md5));

    /* if the URI match the given exclude pattern then skip it */
    if(!pm_is_empty(ctx->excl_patt) && pm_match(ctx->excl_patt, uri))
    {
        if(ctx->verbose)
            con("%s skipped", uri);

        ctx->nexcl++;

        return 0; /* skip it */
    } 

    ctx->nfile++;

    /* if the URI match the given encrypt pattern then encrypt it */
    if(!pm_is_empty(ctx->enc_patt) && pm_match(ctx->enc_patt, uri))
        enc = 1;

    if(ctx->compress) /* -z or -Z have been used */
    {
        /* if the URI match the given compress pattern then compress it */
        if(!pm_is_empty(ctx->comp_patt))
        {   /* compression enabled basing on file URI pattern */
            if(pm_match(ctx->comp_patt, uri))
                zip = 1;
        } else {
            /* compression enabled basing on URI MIME types */
            if((mm = u_get_mime_map(uri)) != NULL)
                zip = mm->comp;
        }
    }

    /* print out some info */
    if(ctx->verbose == 1)
        con("%s (encrypted: %s, compressed: %s)", 
            uri, enc ? "yes" : "no", zip ? "yes" : "no");
    else if(ctx->verbose > 1)
        con("%s -> %s (encrypted: %s, compressed: %s)", 
            file_in + strlen(prefix), uri, 
            enc ? "yes" : "no", zip ? "yes" : "no");

    ext = u_match_ext(file_in, "klx") ? "cc" : "c";

    dbg_err_if(io_printf(ctx->iom, " \\\n" KL1_FILE_FMT, uri_md5, ext) < 0);

    dbg_err_if(io_printf(ctx->ior, "KLONE_REGISTER(action,%s);\n", uri_md5) <0);

    /* we're adding a '/' before the uri (that will be removed by klone.exe) 
     * to avoid MSYS (win32) automatic path translation oddity */
    dbg_err_if(io_printf(ctx->iod, 
            "\n" KL1_FILE_FMT 
            ": %s\n\t$(KLONE) -c translate -i $< -o $@ -u /%s %s %s %s %s\n", 
            uri_md5, ext, file_in, uri, 
            zip ? "-z" : "",
            enc ? "-E" : "", 
            enc && ctx->key_file ? "-k" : "", 
            enc && ctx->key_file ? ctx->key_file  : ""
            ) < 0);

    return 0;
err:
    return ~0;
}

static int cb_dir(struct dirent *de, const char *path , void *arg)
{
    char dir[U_FILENAME_MAX], base_uri[URI_BUFSZ], *cur_uri = (char*)arg;

    dbg_err_if (de == NULL);
    dbg_err_if (path == NULL);
    dbg_err_if (arg == NULL);
    
    ctx->ndir++;

    dbg_err_if(u_snprintf(dir, U_FILENAME_MAX, "%s/%s", path, de->d_name));

    dbg_err_if(u_snprintf(base_uri, URI_BUFSZ, "%s/%s", cur_uri, de->d_name));

    u_foreach_dir_item(dir, S_IFREG, cb_file, (void*)base_uri);

    u_foreach_dir_item(dir, S_IFDIR, cb_dir, (void*)base_uri);

    return 0;
err:
    return ~0;
}

static int print_register_header(io_t *out)
{
    dbg_err_if (out == NULL);
 
    dbg_err_if(io_printf(out, "#include <klone_conf.h>\n") < 0);
    dbg_err_if(io_printf(out, "#include <klone/hook.h>\n") < 0);
    dbg_err_if(io_printf(out, "static void do_register(int);\n") < 0);
    dbg_err_if(io_printf(out, "void unregister_pages(void);\n") < 0);
    dbg_err_if(io_printf(out, "void register_pages(void);\n") < 0);

    dbg_err_if(io_printf(out, 
        "void unregister_pages(void) { \n"
        "do_register(0); }\n"
        ) < 0);
    dbg_err_if(io_printf(out, 
        "void register_pages(void) { \n"
        "do_register(1);\n"
        "#ifdef ENABLE_HOOKS\n"
        "    hooks_setup(); \n"
        "#endif \n"
        "}\n") < 0);
    dbg_err_if(io_printf(out, 
        "static void do_register(int action) {\n") < 0);
    dbg_err_if(io_printf(out,
        "#define KLONE_REGISTER(a, md5)     \\\n"
        "    do {                           \\\n"
        "    void module_init_##md5(void);  \\\n"
        "    void module_term_##md5(void);  \\\n"
        "    if(a) module_init_##md5();     \\\n"
        "    else module_term_##md5();      \\\n"
        "    } while(0)                     \n") < 0);

    return 0;
err:
    return ~0;
}

static int print_register_footer(io_t *out)
{
    dbg_err_if (out == NULL);

    dbg_err_if(io_printf(out, "#undef KLONE_REGISTER\n") < 0);
    dbg_err_if(io_printf(out, "}\n") < 0);

    return 0;
err:
    return ~0;
}

static int trans_site(char *root_dir, char *base_uri)
{
    dbg_err_if (root_dir == NULL);
    dbg_err_if (base_uri == NULL);
    
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
    char *root_dir, *base_uri;

    if(ctx->narg != 1)
        usage();    /* just on directory expected */

    root_dir = ctx->arg[0];
    dbg_err_if(root_dir == NULL);

    if((base_uri = ctx->base_uri) == NULL)
    {
        base_uri = u_strdup("");
        dbg_err_if (base_uri == NULL);
    }

    dbg_err_if(trans_site(root_dir, base_uri));

    con("%lu dirs and %lu files imported, %lu files skipped", 
            ctx->ndir, ctx->nfile, ctx->nexcl);

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

    /* init pattern matching objects */
    dbg_err_if(pm_create(&ctx->comp_patt));
    dbg_err_if(pm_create(&ctx->enc_patt));
    dbg_err_if(pm_create(&ctx->excl_patt));

    /* parse command line switches and set ctx->cmd and params */
    dbg_err_if(parse_opt(argc, argv));

    /* run the command */
    dbg_err_if(dispatch_command());

    return EXIT_SUCCESS;
err:
    return EXIT_FAILURE;
}
