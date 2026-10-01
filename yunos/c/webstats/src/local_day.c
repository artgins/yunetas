/****************************************************************************
 *          local_day.c
 *
 *          Local calendar days before a moment, counted on the calendar.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>
#include <string.h>

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

/***************************************************************************
 *  The next daily slot (`hour`:`minute`, local) after `now` whose day --
 *  the day it reports, the one before it -- comes after `served_day`
 *  ("YYYY-MM-DD", or empty: none served yet).
 *
 *  Each candidate is built from its date and the hour, with tm_isdst -1:
 *  - on the day of the spring change an hour inside the gap is normalised
 *    by mktime() to the hour after it; a candidate built from the
 *    normalised tm (tm_mday + 1) kept that later hour for the next day too;
 *  - on the day of the autumn change an hour inside the repeated hour can
 *    resolve to its second occurrence (glibc does): after the run at the
 *    first one, that second one is still ahead, and it reports the same
 *    day -- which is why a candidate is judged by the DAY it reports, not
 *    by its time.
 *  -1 (logged) when none is found within a year and a month.
 ***************************************************************************/
PUBLIC time_t next_slot_after(hgobj gobj, time_t now, int hour, int minute, const char *served_day)
{
    struct tm today;

    if(!localtime_r(&now, &today)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "localtime_r() FAILED",
            "t",            "%ld", (long)now,
            NULL
        );
        return (time_t)-1;
    }

    for(int k = 0; k < 400; k++) {
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        tm.tm_year = today.tm_year;
        tm.tm_mon = today.tm_mon;
        tm.tm_mday = today.tm_mday + k;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_isdst = -1;
        time_t slot = mktime(&tm);
        if(slot == (time_t)-1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "mktime() FAILED",
                "hour",         "%d", hour,
                "minute",       "%d", minute,
                NULL
            );
            return (time_t)-1;
        }
        if(slot <= now) {
            continue;
        }
        if(served_day && served_day[0]) {
            char day[16];
            if(yesterday_of(gobj, slot, day, sizeof(day)) < 0) {
                return (time_t)-1;  // Error already logged
            }
            if(strcmp(day, served_day) <= 0) {
                continue;   // that day ran already
            }
        }
        return slot;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "No slot in 400 days after the day served",
        "served_day",   "%s", served_day? served_day : "",
        NULL
    );
    return (time_t)-1;
}
