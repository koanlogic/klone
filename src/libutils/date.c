/*
 * Copyright (c) 2005-2012 by KoanLogic s.r.l. <http://www.koanlogic.com>
 * All rights reserved.
 *
 * This file is part of KLone, and as such it is subject to the license stated
 * in the LICENSE file which you have received as part of this distribution.
 *
 * $Id: date.c,v 1.12 2008/12/17 08:40:53 tho Exp $
 */

#include "klone_conf.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <u/libu.h>
#include <klone/os.h>
#include <klone/utils.h>

/**
 *  \addtogroup ut 
 *  \{
 */

static const char* days3[] = { 
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" 
};

static const char* months[] = { 
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" 
};

static int month_idx(const char *mon)
{
    int i;

    dbg_return_if (mon == NULL, -1);
    
    for(i = 0; i < 12; ++i)
        if(strcasecmp(months[i], mon) == 0)
            return i;

    return -1;
}

/**
 * \brief   Convert an asctime(3) string to \c time_t
 *
 * Convert the asctime(3) string \p str to its \c time_t representation \p tp.
 *
 * \param   str     the string to be converted
 * \param   tp      the \c time_t conversion of \p str as a value-result 
 *                  argument
 * \return
 * - \c 0   successful
 * - \c ~0  failure
 */
int u_asctime_to_tt(const char *str, time_t *tp)
{
    enum { BUFSZ = 64 };
    char wday[BUFSZ], mon[BUFSZ];
    unsigned int day, year, hour, min, sec;
    struct tm tm;
    int i;

    dbg_return_if (str == NULL, ~0);
    dbg_return_if (tp == NULL, ~0);
    dbg_return_if (strlen(str) >= BUFSZ, ~0);

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

/**
 * \brief   Convert an rfc850 time string to \c time_t
 *
 * Convert the rfc850 string \p str to its \c time_t representation \p tp.
 *
 * \param   str     the string to be converted
 * \param   tp      the \c time_t conversion of \p str as a value-result 
 *                  argument
 * \return
 * - \c 0   successful
 * - \c ~0  failure
 */
int u_rfc850_to_tt(const char *str, time_t *tp)
{
    enum { BUFSZ = 64 };
    char wday[BUFSZ], mon[BUFSZ], tzone[BUFSZ];
    unsigned int day, year, hour, min, sec;
    struct tm tm;
    int i;
    char c;

    dbg_return_if (str == NULL, ~0);
    dbg_return_if (tp == NULL, ~0);
    dbg_return_if (strlen(str) >= BUFSZ, ~0);

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

#ifdef HAVE_TMZONE
    /* time zone */
    tm.tm_zone = tzone;
#endif

    *tp = timegm(&tm);

    return 0;
err:
    return ~0;
}

/**
 * \brief   Convert an rfc822 time string to \c time_t
 *
 * Convert the rfc822 string \p str to its \c time_t representation \p tp.
 *
 * \param   str     the string to be converted
 * \param   tp      the \c time_t conversion of \p str as a value-result 
 *                  argument
 * \return
 * - \c 0   successful
 * - \c ~0  failure
 */
int u_rfc822_to_tt(const char *str, time_t *tp)
{
    enum { BUFSZ = 64 };
    char wday[BUFSZ], mon[BUFSZ], tzone[BUFSZ];
    unsigned int day, year, hour, min, sec;
    struct tm tm;

    dbg_return_if (str == NULL, ~0);
    dbg_return_if (tp == NULL, ~0);
    dbg_return_if (strlen(str) >= BUFSZ, ~0);

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

#ifdef HAVE_TMZONE
    /* time zone */
    tm.tm_zone = tzone;
#endif

    *tp = timegm(&tm);

    return 0;
err:
    return ~0;
}

/**
 * \brief   Convert an HTTP time string to \c time_t
 *
 * Convert the HTTP time string \p str to its \c time_t representation \p tp.
 *
 * \param   str     the string to be converted
 * \param   tp      the \c time_t conversion of \p str as a value-result 
 *                  argument
 * \return
 * - \c 0   successful
 * - \c ~0  failure
 */
int u_httpdate_to_tt(const char *str, time_t *tp)
{
    dbg_return_if (str == NULL, ~0);
    dbg_return_if (tp == NULL, ~0);
    dbg_return_if (strlen(str) < 4, ~0);

    if(str[3] == ',')
        return u_rfc822_to_tt(str, tp);
    else if(str[3] == ' ')
        return u_asctime_to_tt(str, tp);

    return u_rfc850_to_tt(str, tp);
}

/**
 * \brief   Convert a \c time_t value to a rfc822 time string
 *
 * Convert the \c time_t value \p ts to a rfc822 time string
 *
 * \param   dst     placeholder for the rfc822 time string.  The buffer,
 *                  of at least RFC822_DATE_BUFSZ bytes, must be preallocated
 *                  by the caller.
 * \param   ts      the \c time_t value to be converted
 *
 * \return
 * - \c 0   successful
 * - \c ~0  failure
 */
int u_tt_to_rfc822(char dst[RFC822_DATE_BUFSZ], time_t ts)
{
    char buf[RFC822_DATE_BUFSZ];
    struct tm tm;

    dbg_return_if (dst == NULL, ~0);

#ifdef OS_WIN
    memcpy(&tm, gmtime(&ts), sizeof(tm));
#else
    dbg_err_if(gmtime_r(&ts, &tm) == NULL);
#endif

    dbg_err_if(tm.tm_wday > 6 || tm.tm_wday < 0);
    dbg_err_if(tm.tm_mon > 11 || tm.tm_mon < 0);

    dbg_err_if(u_snprintf(buf, sizeof buf, 
                "%s, %02u %s %02u %02u:%02u:%02u GMT",
                days3[tm.tm_wday], 
                tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900, 
                tm.tm_hour, tm.tm_min, tm.tm_sec));

    /* copy out */
    u_strlcpy(dst, buf, RFC822_DATE_BUFSZ);

    return 0;
err:
    return ~0;
}

/**
 *  \}
 */
