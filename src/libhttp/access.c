#include <time.h>
#include <u/libu.h>
#include <klone/klone.h>
#include <klone/context.h>
#include <klone/klog.h>
#include <klone/access.h>
#include <klone/server_ppc_cmd.h>

static inline const char *value_or_dash(const char *v)
{
    static const char dash[] = "-";

    return (v && v[0] != '\0') ? v : dash;
}

static long get_timezone(struct tm *tm)
{
#ifdef HAVE_STRUCT_TM_TM_GMTOFF
    long int h, m, sign;

    sign = (tm->tm_gmtoff > 0 ? 1 : -1);
    h = abs(tm->tm_gmtoff) / 3600;
    m = (h % 3600) / 60;

    return sign * (h * 100 + m);
#else
    return 0;
#endif
}

int access_log(http_t *h, u_config_t *config, request_t *rq, response_t *rs)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static const char default_prefix[] = "[access]";
    const char *fn, *value, *prefix, *addr;
    char buf[U_MAX_LOG_LENGTH], ip[128] = { '\0' };
    u_config_t *sub;
    vhost_t *vhost;
    struct timeval tv;
    struct tm tm;
    time_t now;
    int logrq, n;

    dbg_err_if(h == NULL);
    dbg_err_if(config == NULL);
    dbg_err_if(rq == NULL);
    dbg_err_if(rs == NULL);

    dbg_err_if((vhost = http_get_vhost(h, rq)) == NULL);

    /* exit if access logging is not configured */
    dbg_err_if(vhost->klog == NULL);

    fn = request_get_filename(rq);
    dbg_err_if(fn == NULL);

    logrq = 0;

    /* if the user specifies what to log */
    if(u_config_get_child_n(config, "log", 0))
    {
        for(n = 0; (sub = u_config_get_child_n(config, "log", n)) != NULL; ++n)
        {
            if((value = u_config_get_value(sub)) == NULL)
                continue;
            if(!fnmatch(value, fn, 0))
            {
                logrq++;
                break; /* log it */
            }
        }
    } else {
        /* no "log" key found, match all filenames */
        logrq++;
    }
    
    if(!logrq)
        return 0; /* don't log this request */

    /* the user specified what is NOT to be logged */
    for(n = 0; (sub = u_config_get_child_n(config, "dontlog", n)) != NULL; ++n)
    {
        if((value = u_config_get_value(sub)) == NULL)
            continue;
        if(fnmatch(value, fn, 0) == FNM_NOMATCH)
            continue;
        else 
            return 0; /* a "dontlog" item matches the filename, don't log */
    }

    gettimeofday(&tv, NULL);

    now = tv.tv_sec;
#ifdef HAVE_LOCALTIME_R
    localtime_r(&now, &tm);
#else
    tm = *localtime(&now);
#endif
    tm.tm_year += 1900;

    if((sub = u_config_get_child(config, "prefix")) == NULL || 
            (prefix = u_config_get_value(sub)) == NULL)
    {
        prefix = default_prefix;
    }

    addr = request_get_peer_addr(rq);

    /* build the log message */
    dbg_err_if(u_snprintf(buf, sizeof(buf),
            "%s %s - - [%02d/%s/%4d:%02d:%02d:%02d %ld]"
            " \"%s\" %d %s \"%s\" \"%s\" \"-\"", 
            prefix,
            value_or_dash(u_addr_get_ip(addr, ip, sizeof ip)),
            /* date */ 
            tm.tm_mday, months[tm.tm_mon], tm.tm_year,
            /* time */ 
            tm.tm_hour, tm.tm_min, tm.tm_sec, get_timezone(&tm),
            /* uri */
            value_or_dash(request_get_client_request(rq)),
            /* status */
            response_get_status(rs),
            /* bytes returned */
            value_or_dash(response_get_field_value(rs, "Content-Length")),
            /* referer and user-agent */
            value_or_dash(request_get_field_value(rq, "Referer")),
            value_or_dash(request_get_field_value(rq, "User-Agent"))
            ));

    /* syslog klog doesn't go through ppc */
    if(vhost->klog->type == KLOG_TYPE_SYSLOG || ctx->pipc == 0)
    {   /* syslog klog or parent context */
        if(vhost->klog)
            dbg_err_if(klog(vhost->klog, KLOG_INFO, "%s", buf));
    } else {
        /* children context */
        dbg_err_if(ctx->server == NULL);
        dbg_err_if(ctx->backend == NULL);
        dbg_err_if(server_ppc_cmd_access_log(ctx->server, ctx->backend->id, 
                    vhost->id, buf));
    }

    return 0;
err:
    return ~0;
}
