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
