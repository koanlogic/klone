#include <stdarg.h>
#include <stdio.h>
#include <klone/os.h>

const char *DEBUG_LABEL = "dbg";
const char *WARN_LABEL = "wrn";

int u_write_debug_message(const char *label, const char *file, int line, 
    const char *func, const char *fmt, ...)
{
    enum { BUFSZ = 1024, MSGSZ = 1200 };
    char buf[BUFSZ], msg[MSGSZ];
    va_list ap;

    /* build the message to send to the log system */
    va_start(ap, fmt); /* init variable list arguments */

    vsnprintf(buf, BUFSZ, fmt, ap);

    va_end(ap);

    syslog(LOG_LOCAL0 | LOG_DEBUG, "[%s][%u:%s:%d:%s] %s", label, getpid(),
        file, line, func, buf);

    return 0;
}

