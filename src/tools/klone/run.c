#include <klone/klone.h>
#include <klone/run.h>
#include <klone/do.h>
#include <klone/request.h>
#include <klone/response.h>
#include <klone/page.h>

int run_page(const char *fqn, request_t *rq, response_t *rs)
{
    do_t *d = NULL;
    /* void (*entryp)(request_t*,response_t*) = NULL; */
    page_t *pg = NULL;

    dbg_err_if(do_load(fqn, &d));

    dbg_err_if(do_sym(d, "pg_handle", (void**)&pg));

    dbg_err_if(pg == NULL);

    /* run it */
    if(pg->type == PAGE_TYPE_DYNAMIC)
    {
        ((page_dynamic_t*)(pg->sd))->run(rq, rs);
    }

    do_free(d);

    return 0;
err:
    if(d)
        do_free(d);
    return 1;
}
