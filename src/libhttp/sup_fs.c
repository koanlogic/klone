#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <klone/supplier.h>
#include <klone/io.h>
#include <klone/utils.h>

static int fs_is_valid_uri(const char* uri, size_t len, time_t *mtime)
{
    struct stat st; 
    char fqn[1+U_FILENAME_MAX];

    if(len > U_FILENAME_MAX)
        return 0;

    memcpy(fqn, uri, len);
    fqn[len] = 0;
    
    if( stat(fqn, &st) == 0 && S_ISREG(st.st_mode))
    {
        *mtime = st.st_mtime;
        return 1;
    } else
        return 0;
}

static int fs_serve(request_t *rq, response_t *rs)
{
    enum { BUFSZ = 4096 };
    io_t *io = NULL;
    const char *mime_type, *fqn;
    size_t c;
    char buf[BUFSZ];
    struct stat st;

    fqn = request_get_resolved_filename(rq);

    /* we need file size */
    dbg_err_if(stat(fqn, &st));
    dbg_err_if(response_set_content_length(rs, st.st_size));

    /* guess the mime type append a Content-Type field to the response*/
    mime_type = u_guess_mime_type(fqn);
    dbg_err_if(response_set_content_type(rs, mime_type));

    /* add a Last-Modified field */
    dbg_err_if(response_set_last_modified(rs, st.st_mtime));

    /* open and write out the whole file */
    dbg_err_if(u_file_open(request_get_resolved_filename(rq), O_RDONLY, &io));

    while((c = io_read(io, buf, BUFSZ)) > 0)
        dbg_err_if(io_write(response_io(rs), buf, c) < 0);

    io_free(io);

    return 0;
err:
    if(io)
        io_free(io);
    return ~0;
}

static int fs_init()
{
    return 0;
}

static void fs_term()
{
    return;
}

supplier_t sup_fs = {
    "fs supplier",
    fs_init,
    fs_term,
    fs_is_valid_uri,
    fs_serve
};

