#include <stdlib.h>
#include <klone/cgi.h>
#include <klone/request.h>
#include <u/debug.h>

int cgi_set_request(request_t *rq)
{
    /* clear uri-related request fields */
    request_clear_uri(rq);

    dbg_err_if(request_set_filename(rq, getenv("SCRIPT_NAME")));
    dbg_err_if(request_set_path_info(rq, getenv("PATH_INFO")));
    dbg_err_if(request_set_query_string(rq, getenv("QUERY_STRING")));
    dbg_err_if(request_set_method(rq, getenv("REQUEST_METHOD")));

    if(getenv("CONTENT_TYPE"))
        dbg_err_if(request_set_field(rq, "Content-Type", 
                    getenv("CONTENT_TYPE")));

    if(getenv("CONTENT_LENGTH"))
        dbg_err_if(request_set_field(rq, "Content-Length", 
                    getenv("CONTENT_LENGTH")));

    return 0;
err:
    return ~0;
}
