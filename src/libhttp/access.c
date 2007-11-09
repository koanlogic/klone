#include <time.h>
#include <syslog.h>
#include <u/libu.h>
#include <klone/klone.h>
#include <klone/context.h>
#include <klone/klog.h>

static inline const char *value_or_dash(const char *v)
{
    static const char dash[] = "-";

    return (v ? v : dash);
}

static long get_timezone(struct tm *tm)
{
    long int h, m, sign;

    sign = (tm->tm_gmtoff > 0 ? 1 : -1);
    h = abs(tm->tm_gmtoff) / 3600;
    m = (h % 3600) / 60;

    return sign * (h * 100 + m);
}

int access_log(http_t *h, u_config_t *config, request_t *rq, response_t *rs)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static const char default_prefix[] = "[access]";
    static FILE *fp = NULL;
    u_config_t *sub;
    const char *ip, *p, *fn, *value, *prefix;
    addr_t *addr;
    struct timeval tv;
    struct tm tm;
    time_t now;
    int i, logrq, n;

    dbg_err_if(config == NULL);

    if((config = u_config_get_child(config, "access_log")) == NULL)
        return 0; /* access log not configured */

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
            if(!strcmp(value, "*") || !fnmatch(value, fn, 0))
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

    addr = request_get_peer_addr(rq);
    ip = inet_ntoa(addr->sa.sin.sin_addr);

    now = tv.tv_sec;
    localtime_r(&now, &tm);
    tm.tm_year += 1900;

    if((sub = u_config_get_child(config, "prefix")) == NULL || 
            (prefix = u_config_get_value(sub)) == NULL)
    {
        prefix = default_prefix;
    }

    info( "%s %s - - [%02d/%s/%4d:%02d:%02d:%02d %ld]"
            " \"%s\" %d %s \"%s\" \"%s\" \"-\"", 
            prefix,
            value_or_dash(ip),
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
            );

    return 0;
err:
    return ~0;
}
