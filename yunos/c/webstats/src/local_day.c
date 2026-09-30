/****************************************************************************
 *          local_day.c
 *
 *          Local calendar days before a moment, counted on the calendar.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>

#include "local_day.h"

/***************************************************************************
 *  The local day `days` calendar days before the day of `t`.
 *
 *  Counted on the calendar, not in seconds: `t - days*86400` is one hour
 *  off for every change of hour in between, which moves the day near
 *  midnight: from 00:00 to 01:00 after the 23-hour day of the spring
 *  change it is one day too far back, from 23:00 to 24:00 of the 25-hour
 *  day of the autumn one it is one day short. mktime() normalises a day 0
 *  (or less) of a month, and the noon of a day exists in every time zone
 *  whatever its change of hour.
 ***************************************************************************/
PUBLIC int day_before_of(hgobj gobj, time_t t, int days, char *bf, size_t bfsize)
{
    struct tm tm;

    if(!localtime_r(&t, &tm)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "localtime_r() FAILED",
            "t",            "%ld", (long)t,
            NULL
        );
        return -1;
    }

    tm.tm_mday -= days;
    tm.tm_hour = 12;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    if(mktime(&tm) == (time_t)-1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "mktime() FAILED",
            "t",            "%ld", (long)t,
            "days",         "%d", days,
            NULL
        );
        return -1;
    }

    if(strftime(bf, bfsize, "%Y-%m-%d", &tm) == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Date does not fit in the buffer",
            "bfsize",       "%d", (int)bfsize,
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  The local day before the day of `now`: the day a report run at `now` is
 *  about.
 ***************************************************************************/
PUBLIC int yesterday_of(hgobj gobj, time_t now, char *bf, size_t bfsize)
{
    return day_before_of(gobj, now, 1, bf, bfsize);
}

/***************************************************************************
 *  The same local time of day, `days` calendar days before `t`: the moment
 *  an age of `days` days ends. `t - days*86400` lands one hour off across
 *  a change of hour. A time that does not exist on that day (inside the
 *  spring gap) is normalised by mktime(), an hour later.
 ***************************************************************************/
PUBLIC time_t moment_days_before(hgobj gobj, time_t t, int days)
{
    struct tm tm;

    if(!localtime_r(&t, &tm)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "localtime_r() FAILED",
            "t",            "%ld", (long)t,
            NULL
        );
        return (time_t)-1;
    }

    tm.tm_mday -= days;
    tm.tm_isdst = -1;
    time_t before = mktime(&tm);
    if(before == (time_t)-1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "mktime() FAILED",
            "t",            "%ld", (long)t,
            "days",         "%d", days,
            NULL
        );
    }
    return before;
}
