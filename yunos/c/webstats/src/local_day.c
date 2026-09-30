/****************************************************************************
 *          local_day.c
 *
 *          The local calendar day before a moment.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>

#include "local_day.h"

/***************************************************************************
 *  The local day before the day of `now`.
 *
 *  Counted on the calendar, not in seconds: `now - 86400` is the day
 *  BEFORE yesterday from 00:00 to 01:00 after the 23-hour day of the spring
 *  change, and the SAME day from 23:00 to 24:00 of the 25-hour day of the
 *  autumn one. mktime() normalises the day 0 of a month, and the noon of a
 *  day exists in every time zone whatever its change of hour.
 ***************************************************************************/
PUBLIC int yesterday_of(hgobj gobj, time_t now, char *bf, size_t bfsize)
{
    struct tm tm;

    if(!localtime_r(&now, &tm)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "localtime_r() FAILED",
            "t",            "%ld", (long)now,
            NULL
        );
        return -1;
    }

    tm.tm_mday--;
    tm.tm_hour = 12;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    if(mktime(&tm) == (time_t)-1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "mktime() FAILED",
            "t",            "%ld", (long)now,
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
