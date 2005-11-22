#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <klone/klone.h>
#include <klone/os.h>
#include <klone/utils.h>

/**
 *  \addtogroup u_t u_t - Utility functions
 *  \{
 */

const char* days3[] = { 
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" 
};
const char* days[] = { 
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",  "Friday",
    "Saturday", "Sunday" 
};
const char* months[] = { 
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" 
};

static int month_idx(const char *mon)
{
    int i;

    for(i = 0; i < 12; ++i)
        if(strcasecmp(months[i], mon) == 0)
            return i;

    return -1;
}

/**
 * \brief   ...
 *
 * ...
 *
 * \param   str     ...
 * \param   tp      ...
 *
 * \return ...
 */
int u_asctime_to_tt(const char *str, time_t *tp)
{
    enum { BUFSZ = 64 };
    char wday[BUFSZ], mon[BUFSZ];
    unsigned int day, year, hour, min, sec;
    struct tm tm;
    int i;

    dbg_err_if(strlen(str) >= BUFSZ);

    dbg_err_if((i = sscanf(str, "%s %s %u %u:%u:%u %u", wday, 
        mon, &day, &hour, &min, &sec, &year)) != 7);

    memset(&tm, 0, sizeof(struct tm));

    /* time */
    tm.tm_sec = sec; tm.tm_min = min; tm.tm_hour = hour;

    /* date */
    tm.tm_mday = day; 
    tm.tm_mon = month_idx(mon);
    tm.tm_year = year - 1900;

    dbg_err_if(tm.tm_mon < 0);

    *tp = timegm(&tm);
    
    return 0;
err:
    return ~0;
}

int u_rfc850_to_tt(const char *str, time_t *tp)
{
    enum { BUFSZ = 64 };
    char wday[BUFSZ], mon[BUFSZ], tzone[BUFSZ];
    unsigned int day, year, hour, min, sec;
    struct tm tm;
    int i;
    char c;

    dbg_err_if(strlen(str) >= BUFSZ);

    dbg_err_if((i = sscanf(str, "%[^,], %u%c%[^-]%c%u %u:%u:%u %s", wday, 
        &day, &c, mon, &c, &year, &hour, &min, &sec, tzone)) != 10);

    memset(&tm, 0, sizeof(struct tm));

    /* time */
    tm.tm_sec = sec; tm.tm_min = min; tm.tm_hour = hour;

    /* date */
    tm.tm_mday = day; 
    tm.tm_mon = month_idx(mon);
    tm.tm_year = year - 1900;

    dbg_err_if(tm.tm_mon < 0);

    /* time zone */
    tm.tm_zone = tzone;

    *tp = timegm(&tm);

    return 0;
err:
    return ~0;
}

int u_rfc822_to_tt(const char *str, time_t *tp)
{
    enum { BUFSZ = 64 };
    char wday[BUFSZ], mon[BUFSZ], tzone[BUFSZ];
    unsigned int day, year, hour, min, sec;
    struct tm tm;

    dbg_err_if(strlen(str) >= BUFSZ);

    dbg_err_if(sscanf(str, "%[^,], %u %s %u %u:%u:%u %s", wday, 
        &day, mon, &year, &hour, &min, &sec, tzone) != 8);

    memset(&tm, 0, sizeof(struct tm));

    /* time */
    tm.tm_sec = sec; tm.tm_min = min; tm.tm_hour = hour;

    /* date */
    tm.tm_mday = day; 
    tm.tm_mon = month_idx(mon);
    tm.tm_year = year - 1900; 

    dbg_err_if(tm.tm_mon < 0);

    /* time zone */
    tm.tm_zone = tzone;

    *tp = timegm(&tm);

    return 0;
err:
    return ~0;
}

int u_httpdate_to_tt(const char *str, time_t *tp)
{
    dbg_err_if(str < 4);

    if(str[3] == ',')
        return u_rfc822_to_tt(str, tp);
    else if(str[3] == ' ')
        return u_asctime_to_tt(str, tp);

    return u_rfc850_to_tt(str, tp);
err:
    return ~0;
}


/* time_t to rfc822 Date convertion (Thu, 23 Jun 2005 13:06:16 GMT) */
int u_tt_to_rfc822(char *dst, time_t ts, size_t bufsz)
{
    enum { RFC822_DATE_BUFSZ = 32 };
    char buf[RFC822_DATE_BUFSZ];
    struct tm tm;

#ifdef OS_WIN
    memcpy(&tm, gmtime(&ts), sizeof(tm));
#else
    dbg_err_if(gmtime_r(&ts, &tm) == NULL);
#endif

    dbg_err_if(bufsz < RFC822_DATE_BUFSZ);
    dbg_err_if(tm.tm_wday > 6 || tm.tm_wday < 0);
    dbg_err_if(tm.tm_mon > 11 || tm.tm_mon < 0);

    dbg_err_if(u_snprintf(buf, RFC822_DATE_BUFSZ, 
                "%s, %02u %s %02u %02u:%02u:%02u GMT",
                days3[tm.tm_wday], 
                tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900, 
                tm.tm_hour, tm.tm_min, tm.tm_sec));

    /* copy out */
    strncpy(dst, buf, bufsz);

    return 0;
err:
    return ~0;
}

/**
 *  \}
 */
