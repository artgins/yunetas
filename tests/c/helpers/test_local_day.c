/****************************************************************************
 *          test_local_day.c
 *
 *          The day webstats reports: yunos/c/webstats/src/local_day.c,
 *          compiled into this test, in the time zone of Madrid.
 *
 *          The day before the day of `now`. Up to 7.25.20 it was the day of
 *          `now - 86400`, which is the day BEFORE yesterday from 00:00 to
 *          01:00 after the 23-hour day of the spring change: a report asked
 *          then (analyze-now, send-yesterday, a report_hour of 0) read and
 *          stored the wrong day.
 *
 *          Cases, as "local time of now -> day reported":
 *          1. an ordinary day, and the turn of a month and of a year;
 *          2. after the spring change (23 hours), at 00:30 and at 23:59;
 *          3. after the autumn change (25 hours), at 00:30 and at 23:59.
 *
 *          And the same count for N days, and for an age of N days, which
 *          the store (keep_days), the history of new visitors
 *          (new_visitor_days) and the whois cache (whois_cache_days) make.
 *          Up to 7.25.20 they took `t - N*86400`: an hour off across each
 *          change of hour, one day off near midnight.
 *          4. day_before_of(): N days back across a change of hour;
 *          5. moment_days_before(): the same time of day N days back.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <yunetas.h>
#include "local_day.h"

#define APP     "test_local_day"

PRIVATE int global_result = 0;

/***************************************************************************
 *  `now` given as local time, the day expected
 ***************************************************************************/
PRIVATE void check_yesterday(int year, int month, int day, int hour, int minute, const char *expected)
{
    struct tm tm = {0};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_isdst = -1;
    time_t now = mktime(&tm);

    char date[16] = {0};
    int ret = yesterday_of(0, now, date, sizeof(date));

    char label[64];
    snprintf(label, sizeof(label), "%04d-%02d-%02d %02d:%02d", year, month, day, hour, minute);
    if(ret == 0 && strcmp(date, expected) == 0) {
        printf("ok   %s -> %s\n", label, date);
    } else {
        printf("FAIL %s -> %s (expected %s)\n", label, date, expected);
        global_result += -1;
    }
}

/***************************************************************************
 *  A local time as time_t
 ***************************************************************************/
PRIVATE time_t local_time(int year, int month, int day, int hour, int minute)
{
    struct tm tm = {0};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

/***************************************************************************
 *  The day `days` days before a local time
 ***************************************************************************/
PRIVATE void check_days_before(int year, int month, int day, int hour, int minute,
    int days, const char *expected)
{
    char date[16] = {0};
    int ret = day_before_of(0, local_time(year, month, day, hour, minute), days, date, sizeof(date));

    char label[64];
    snprintf(label, sizeof(label), "%04d-%02d-%02d %02d:%02d - %d days", year, month, day, hour, minute, days);
    if(ret == 0 && strcmp(date, expected) == 0) {
        printf("ok   %s -> %s\n", label, date);
    } else {
        printf("FAIL %s -> %s (expected %s)\n", label, date, expected);
        global_result += -1;
    }
}

/***************************************************************************
 *  The same time of day `days` days before a local time
 ***************************************************************************/
PRIVATE void check_moment_before(int year, int month, int day, int hour, int minute,
    int days, const char *expected)
{
    time_t t = moment_days_before(0, local_time(year, month, day, hour, minute), days);
    struct tm tm;
    char got[32] = "?";
    if(t != (time_t)-1 && localtime_r(&t, &tm)) {
        strftime(got, sizeof(got), "%Y-%m-%d %H:%M", &tm);
    }

    char label[64];
    snprintf(label, sizeof(label), "%04d-%02d-%02d %02d:%02d - %d days", year, month, day, hour, minute, days);
    if(strcmp(got, expected) == 0) {
        printf("ok   %s -> %s\n", label, got);
    } else {
        printf("FAIL %s -> %s (expected %s)\n", label, got, expected);
        global_result += -1;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_days(void)
{
    /*
     *  1. Ordinary days
     */
    check_yesterday(2026, 9, 30, 6, 0, "2026-09-29");
    check_yesterday(2026, 3, 1, 0, 10, "2026-02-28");
    check_yesterday(2026, 1, 1, 0, 10, "2025-12-31");

    /*
     *  2. Sunday 2026-03-29 has 23 hours (02:00 -> 03:00)
     */
    check_yesterday(2026, 3, 30, 0, 30, "2026-03-29");
    check_yesterday(2026, 3, 30, 23, 59, "2026-03-29");
    check_yesterday(2026, 3, 29, 23, 59, "2026-03-28");

    /*
     *  3. Sunday 2026-10-25 has 25 hours (03:00 -> 02:00)
     */
    check_yesterday(2026, 10, 26, 0, 30, "2026-10-25");
    check_yesterday(2026, 10, 26, 23, 59, "2026-10-25");
    check_yesterday(2026, 10, 25, 23, 59, "2026-10-24");

    /*
     *  4. N days back (keep_days, new_visitor_days), across a change
     */
    check_days_before(2026, 9, 30, 6, 0, 400, "2025-08-26");
    check_days_before(2026, 4, 20, 0, 30, 30, "2026-03-21");     // spring in between
    check_days_before(2026, 4, 20, 12, 0, 30, "2026-03-21");
    check_days_before(2026, 11, 20, 23, 30, 30, "2026-10-21");   // autumn in between
    check_days_before(2026, 10, 25, 23, 30, 1, "2026-10-24");
    check_days_before(2026, 3, 30, 0, 30, 1, "2026-03-29");
    check_days_before(2026, 3, 30, 12, 0, 0, "2026-03-30");

    /*
     *  5. An age of N days (whois_cache_days) ends at the same time of day
     */
    check_moment_before(2026, 3, 30, 12, 0, 1, "2026-03-29 12:00");
    check_moment_before(2026, 10, 25, 12, 0, 1, "2026-10-24 12:00");
    check_moment_before(2026, 4, 20, 9, 15, 30, "2026-03-21 09:15");
    check_moment_before(2026, 11, 20, 9, 15, 30, "2026-10-21 09:15");
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    setenv("TZ", "Europe/Madrid", 1);
    tzset();

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authentication_parser
    );
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    test_days();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
