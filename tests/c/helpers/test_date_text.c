/****************************************************************************
 *          test_date_text.c
 *
 *          Dates as text, read and written by kernel/c/gobj-c/src/helpers.c:
 *          approxidate_careful() / approxidate(), parse_date_basic() and
 *          show_date_relative().
 *
 *          In the time zone of Madrid, with the clock faked (the test binary
 *          is linked with --wrap=time, see CMakeLists.txt): NOW is
 *          Wednesday 2026-10-21 09:00:00 UTC, 11:00:00 CEST (+0200). A time
 *          read is checked as the local time it names, with its offset:
 *          "2026-10-20 11:00:00 +0200" is yesterday at the hour of now.
 *
 *          Cases:
 *          1.  approxidate: now, durations back ("2 hours ago", "1.day",
 *              number words, short units: 1s 1m 1h 1d 1w 1M 1y, where 'm'
 *              is a minute and 'M' a month, also with a minus sign: "-1d"),
 *              specials (noon, midnight, tea, never, am/pm),
 *              "last <weekday|unit>", clock times;
 *          2.  approxidate: absolute dates (ISO 8601, RFC 2822, the
 *              "Thu Oct 1 10:00:00 2026 +0200" form, month names, numeric
 *              dates M/D/Y and D.M.Y), time zones, epoch seconds
 *              ("@<seconds> +0000", with no year limit), a date with no
 *              year that is not more than 10 days ahead;
 *          3.  approxidate: a text it cannot read is NOW, with the error
 *              flag;
 *          4.  parse_date_basic(): only a whole date and time, or epoch
 *              seconds, and the offset it was given in (the local one when
 *              none);
 *          5.  show_date_relative() at the edge of each unit.
 *
 *          And what the rewrite of 2026 (the port of git's date.c,
 *          replaced) reads differently:
 *          6.  a text that is no date is refused, with the error flag,
 *              where the port read something else in silence: a time ahead
 *              ("in 3 days", "2 days from now" were days BACK), "1.5 hours
 *              ago" (the 1st of May), a date not in the calendar
 *              ("2026-02-30" was March 2), a lone sign or number ("+5" was
 *              the 5th; "-5" too), a plus sign before a duration ("+1d"
 *              was 1 day back); "today", "2 mins", "3 hrs" and "@0" are
 *              read;
 *          7.  the edges of the calendar: a month back from the 31st, a
 *              year back from Feb 29, a Y-M-D date far ahead of now;
 *          8.  a clock time on the day of a change of hour is the local
 *              one ("noon yesterday" was 13:00 on 2026-10-25);
 *          9.  parse_date_basic() refuses a word it does not know. And one
 *              unit is singular in show_date_relative() (5).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <yunetas.h>

#define APP     "test_date_text"

#define NOW     1792573200  // Wed 2026-10-21 09:00:00 UTC, 11:00:00 CEST

PRIVATE int global_result = 0;

/***************************************************************************
 *  The wall clock, faked: -Wl,--wrap=time (see CMakeLists.txt)
 ***************************************************************************/
PRIVATE time_t s_fake_now = NOW;

time_t __real_time(time_t *t);
time_t __wrap_time(time_t *t);

time_t __wrap_time(time_t *t)
{
    if(t) {
        *t = s_fake_now;
    }
    return s_fake_now;
}

/***************************************************************************
 *  A time as the local time it names, with its offset
 ***************************************************************************/
PRIVATE const char *local_text(timestamp_t t, char *bf, size_t bfsize)
{
    time_t tt = (time_t)t;
    struct tm tm;
    if(!localtime_r(&tt, &tm)) {
        snprintf(bf, bfsize, "(no local time of %llu)", (unsigned long long)t);
        return bf;
    }
    strftime(bf, bfsize, "%Y-%m-%d %H:%M:%S %z", &tm);
    return bf;
}

/***************************************************************************
 *  approxidate_careful(): the local time expected and the error flag
 ***************************************************************************/
typedef struct {
    const char *text;
    const char *expected;
    int error;
} approxidate_case_t;

PRIVATE void check_approxidate(const approxidate_case_t *cases, size_t n)
{
    for(size_t i = 0; i < n; i++) {
        const approxidate_case_t *c = &cases[i];
        int error = 0;
        timestamp_t t = approxidate_careful(c->text, &error);
        char got[64];
        local_text(t, got, sizeof(got));
        if(strcmp(got, c->expected) == 0 && error == c->error) {
            printf("ok   approxidate '%s' -> %s%s\n", c->text, got, error? " (error)" : "");
        } else {
            printf("FAIL approxidate '%s' -> %s error %d (expected %s error %d)\n",
                c->text, got, error, c->expected, c->error);
            global_result += -1;
        }
    }
}

/***************************************************************************
 *  A value, as a number
 ***************************************************************************/
PRIVATE void check_number(const char *label, unsigned long long got, unsigned long long expected)
{
    if(got == expected) {
        printf("ok   %s -> %llu\n", label, got);
    } else {
        printf("FAIL %s -> %llu (expected %llu)\n", label, got, expected);
        global_result += -1;
    }
}

/***************************************************************************
 *  A text
 ***************************************************************************/
PRIVATE void check_text(const char *label, const char *got, const char *expected)
{
    if(got && strcmp(got, expected) == 0) {
        printf("ok   %s -> '%s'\n", label, got);
    } else {
        printf("FAIL %s -> '%s' (expected '%s')\n", label, got? got : "(null)", expected);
        global_result += -1;
    }
}

/***************************************************************************
 *  1. Durations back from now, specials, clock times
 ***************************************************************************/
PRIVATE void test_approxidate_durations_and_times(void)
{
    static const approxidate_case_t cases[] = {
        {"now",                     "2026-10-21 11:00:00 +0200", 0},
        {"Now",                     "2026-10-21 11:00:00 +0200", 0},
        {"yesterday",               "2026-10-20 11:00:00 +0200", 0},
        {"YESTERDAY",               "2026-10-20 11:00:00 +0200", 0},

        /*
         *  A count and a unit; "ago" is optional, the direction is back
         */
        {"2 hours ago",             "2026-10-21 09:00:00 +0200", 0},
        {"3 days ago",              "2026-10-18 11:00:00 +0200", 0},
        {"2 weeks ago",             "2026-10-07 11:00:00 +0200", 0},
        {"2 weeks",                 "2026-10-07 11:00:00 +0200", 0},
        {"1 month ago",             "2026-09-21 11:00:00 +0200", 0},
        {"2 years ago",             "2024-10-21 11:00:00 +0200", 0},
        {"1.day",                   "2026-10-20 11:00:00 +0200", 0},
        {"3.hours.ago",             "2026-10-21 08:00:00 +0200", 0},
        {"2 days 3 hours ago",      "2026-10-19 08:00:00 +0200", 0},
        {"1 year 2 months ago",     "2025-08-21 11:00:00 +0200", 0},
        {"one day ago",             "2026-10-20 11:00:00 +0200", 0},
        {"ten minutes ago",         "2026-10-21 10:50:00 +0200", 0},

        /*
         *  Short units: 'm' is a minute, 'M' a month, 'mon' is Monday
         */
        {"1s",                      "2026-10-21 10:59:59 +0200", 0},
        {"10s",                     "2026-10-21 10:59:50 +0200", 0},
        {"1m",                      "2026-10-21 10:59:00 +0200", 0},
        {"1h",                      "2026-10-21 10:00:00 +0200", 0},
        {"1d",                      "2026-10-20 11:00:00 +0200", 0},
        {"1w",                      "2026-10-14 11:00:00 +0200", 0},
        {"1M",                      "2026-09-21 11:00:00 +0200", 0},
        {"1y",                      "2025-10-21 11:00:00 +0200", 0},
        {"1sec",                    "2026-10-21 10:59:59 +0200", 0},
        {"1mi",                     "2026-10-21 10:59:00 +0200", 0},
        {"1min",                    "2026-10-21 10:59:00 +0200", 0},
        {"1MIN",                    "2026-10-21 10:59:00 +0200", 0},
        {"1mo",                     "2026-09-21 11:00:00 +0200", 0},
        {"1Mo",                     "2026-09-21 11:00:00 +0200", 0},
        {"1hr",                     "2026-10-21 10:00:00 +0200", 0},
        {"1wk",                     "2026-10-14 11:00:00 +0200", 0},
        {"1yr",                     "2025-10-21 11:00:00 +0200", 0},
        {"5m ago",                  "2026-10-21 10:55:00 +0200", 0},
        {"1d2h",                    "2026-10-20 09:00:00 +0200", 0},
        {"2h30m",                   "2026-10-21 08:30:00 +0200", 0},
        {"1y2M",                    "2025-08-21 11:00:00 +0200", 0},
        {"1mon",                    "2026-10-19 11:00:00 +0200", 0},

        /*
         *  A minus sign before a count and its unit: the port skipped it,
         *  and "-1d" was typed for years
         */
        {"-1d",                     "2026-10-20 11:00:00 +0200", 0},
        {"-10d",                    "2026-10-11 11:00:00 +0200", 0},
        {"-2h",                     "2026-10-21 09:00:00 +0200", 0},
        {"-2 hours",                "2026-10-21 09:00:00 +0200", 0},
        {"-1w ago",                 "2026-10-14 11:00:00 +0200", 0},
        {"-1M",                     "2026-09-21 11:00:00 +0200", 0},
        {"-1d2h",                   "2026-10-20 09:00:00 +0200", 0},

        /*
         *  noon, midnight and tea are the last ones that passed
         */
        {"noon",                    "2026-10-20 12:00:00 +0200", 0},
        {"midnight",                "2026-10-21 00:00:00 +0200", 0},
        {"tea",                     "2026-10-20 17:00:00 +0200", 0},
        {"noon yesterday",          "2026-10-19 12:00:00 +0200", 0},
        {"yesterday noon",          "2026-10-19 12:00:00 +0200", 0},
        {"never",                   "1970-01-01 01:00:00 +0100", 0},

        /*
         *  A clock time is of today, ahead of now or not
         */
        {"3pm",                     "2026-10-21 15:00:00 +0200", 0},
        {"3 pm",                    "2026-10-21 15:00:00 +0200", 0},
        {"10am",                    "2026-10-21 10:00:00 +0200", 0},
        {"12am",                    "2026-10-21 00:00:00 +0200", 0},
        {"12pm",                    "2026-10-21 12:00:00 +0200", 0},
        {"3:45 PM",                 "2026-10-21 15:45:00 +0200", 0},
        {"10:30",                   "2026-10-21 10:30:00 +0200", 0},
        {"10:30:15",                "2026-10-21 10:30:15 +0200", 0},
        {"yesterday 15:00",         "2026-10-20 15:00:00 +0200", 0},
        {"3 days 10:00",            "2026-10-18 10:00:00 +0200", 0},

        /*
         *  "last <weekday>" is the one before today, "N <weekday>" N of them
         */
        {"last tuesday",            "2026-10-20 11:00:00 +0200", 0},
        {"last wednesday",          "2026-10-14 11:00:00 +0200", 0},
        {"1 wednesday",             "2026-10-14 11:00:00 +0200", 0},
        {"2 wednesday",             "2026-10-07 11:00:00 +0200", 0},
        {"last week",               "2026-10-14 11:00:00 +0200", 0},
        {"last month",              "2026-09-21 11:00:00 +0200", 0},
        {"last year",               "2025-10-21 11:00:00 +0200", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));
}

/***************************************************************************
 *  2. Absolute dates
 ***************************************************************************/
PRIVATE void test_approxidate_absolute_forms(void)
{
    static const approxidate_case_t cases[] = {
        /*
         *  A date with no clock time keeps the time of day of now, in the
         *  UTC offset of now: 09:00 UTC, which is 10:00 after the change of
         *  hour of 2026-10-25
         */
        {"2026-10-01",                      "2026-10-01 11:00:00 +0200", 0},
        {"2026/10/01",                      "2026-10-01 11:00:00 +0200", 0},
        {"2026.10.01",                      "2026-10-01 11:00:00 +0200", 0},
        {"Oct 1 2026",                      "2026-10-01 11:00:00 +0200", 0},
        {"1 Oct 2026",                      "2026-10-01 11:00:00 +0200", 0},
        {"October 1, 2026",                 "2026-10-01 11:00:00 +0200", 0},

        /*
         *  A date and a clock time, local unless a zone is given
         */
        {"2026-10-01 10:00",                "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00",             "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01T10:00:00",             "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01T10:00:00Z",            "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01T10:00:00+02:00",       "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01T10:00:00.123+02:00",   "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 +0200",       "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 -0500",       "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 UTC",         "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01 10:00:00 GMT",         "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01 10:00:00 CET",         "2026-10-01 11:00:00 +0200", 0},
        {"2026-10-01 10:00:00 EST",         "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 PST",         "2026-10-01 20:00:00 +0200", 0},
        {"20261001T100000",                 "2026-10-01 10:00:00 +0200", 0},
        {"Thu, 01 Oct 2026 10:00:00 +0000", "2026-10-01 12:00:00 +0200", 0},
        {"Thu Oct 1 10:00:00 2026 +0200",   "2026-10-01 10:00:00 +0200", 0},
        {"Thu Oct 1 10:00:00 2026",         "2026-10-01 10:00:00 +0200", 0},
        {"Oct 1, 2026 10:00",               "2026-10-01 10:00:00 +0200", 0},

        /*
         *  Numbers: M/D/Y first, D/M/Y when the month cannot be; D.M.Y
         */
        {"10/01/2026",                      "2026-10-01 11:00:00 +0200", 0},
        {"01/10/2026",                      "2026-01-10 10:00:00 +0100", 0},
        {"13/10/2026",                      "2026-10-13 11:00:00 +0200", 0},
        {"1.10.2026",                       "2026-10-01 11:00:00 +0200", 0},
        {"1/10/26",                         "2026-01-10 10:00:00 +0100", 0},
        {"Oct 1 99",                        "1999-10-01 11:00:00 +0200", 0},
        {"Oct 1 37",                        "2037-10-01 11:00:00 +0200", 0},

        /*
         *  What is not said is taken from now; a date with no year is of
         *  the year before when it is more than 10 days ahead
         */
        {"May 11",                          "2026-05-11 11:00:00 +0200", 0},
        {"May",                             "2026-05-21 11:00:00 +0200", 0},
        {"Sep 2024",                        "2024-09-21 11:00:00 +0200", 0},
        {"2024",                            "2024-10-21 11:00:00 +0200", 0},
        {"Oct 30",                          "2026-10-30 10:00:00 +0100", 0},
        {"Oct 31",                          "2026-10-31 10:00:00 +0100", 0},
        {"Nov 1",                           "2025-11-01 10:00:00 +0100", 0},

        /*
         *  Epoch seconds
         */
        {"@1700000000",                     "2023-11-14 23:13:20 +0100", 0},
        {"@1700000000 +0200",               "2023-11-14 23:13:20 +0100", 0},
        {"1700000000",                      "2023-11-14 23:13:20 +0100", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));

    /*
     *  Epoch seconds have no year limit: timeranger2 refuses what does
     *  not fit in a record (test_c_tranger)
     */
    int error = 0;
    check_number("approxidate '@99999999999999 +0000'",
        approxidate_careful("@99999999999999 +0000", &error), 99999999999999ULL);
    check_number("approxidate '@99999999999999 +0000' error", (unsigned long long)error, 0);

    check_number("approxidate() of '2 hours ago'", approxidate("2 hours ago"), NOW - 7200);
}

/***************************************************************************
 *  3. What approxidate cannot read
 ***************************************************************************/
PRIVATE void test_approxidate_unreadable(void)
{
    static const approxidate_case_t cases[] = {
        {"",                        "2026-10-21 11:00:00 +0200", 1},
        {"xyz",                     "2026-10-21 11:00:00 +0200", 1},
        {"hours",                   "2026-10-21 11:00:00 +0200", 1},
        {"ago",                     "2026-10-21 11:00:00 +0200", 1},
        {"friday",                  "2026-10-21 11:00:00 +0200", 1},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));

    check_number("approxidate() of 'xyz' is now", approxidate("xyz"), NOW);
}

/***************************************************************************
 *  4. parse_date_basic(): the time and the offset, in minutes
 ***************************************************************************/
PRIVATE void check_parse_date_basic(const char *text, int ret, const char *expected, int offset)
{
    timestamp_t t = 0;
    int off = 0;
    int r = parse_date_basic(text, &t, &off);
    char got[64] = "";
    if(r == 0) {
        local_text(t, got, sizeof(got));
    }
    if(r == ret && (ret != 0 || (strcmp(got, expected) == 0 && off == offset))) {
        printf("ok   parse_date_basic '%s' -> %d %s %d\n", text, r, got, r == 0? off : 0);
    } else {
        printf("FAIL parse_date_basic '%s' -> %d %s %d (expected %d %s %d)\n",
            text, r, got, off, ret, expected, offset);
        global_result += -1;
    }
}

PRIVATE void test_parse_date_basic(void)
{
    check_parse_date_basic("2026-10-01 10:00:00",               0, "2026-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("2026-10-01 10:00",                  0, "2026-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("2026-01-15 10:00:00",               0, "2026-01-15 10:00:00 +0100", 60);
    check_parse_date_basic("2026-10-01T10:00:00Z",              0, "2026-10-01 12:00:00 +0200", 0);
    check_parse_date_basic("2026-10-01T10:00:00+02:00",         0, "2026-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("2026-10-01 10:00:00 -0500",         0, "2026-10-01 17:00:00 +0200", -300);
    check_parse_date_basic("2026-10-01 10:00:00 +02:30",        0, "2026-10-01 09:30:00 +0200", 150);
    check_parse_date_basic("2026-10-01 10:00:00 UTC",           0, "2026-10-01 12:00:00 +0200", 0);
    check_parse_date_basic("2026-10-01 10:00:00 PM",            0, "2026-10-01 22:00:00 +0200", 120);
    check_parse_date_basic("Thu, 01 Oct 2026 10:00:00 +0000",   0, "2026-10-01 12:00:00 +0200", 0);
    check_parse_date_basic("Thu, 1 Oct 2026 10:00:00 GMT",      0, "2026-10-01 12:00:00 +0200", 0);
    check_parse_date_basic("Thu Oct 1 10:00:00 2026 +0200",     0, "2026-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("@1700000000",                       0, "2023-11-14 23:13:20 +0100", 60);
    check_parse_date_basic("@1700000000 +0200",                 0, "2023-11-14 23:13:20 +0100", 120);
    check_parse_date_basic("1700000000 -0130",                  0, "2023-11-14 23:13:20 +0100", -90);

    /*
     *  Not a whole date and time
     */
    check_parse_date_basic("2026-10-01",                        -1, "", 0);
    check_parse_date_basic("10:00:00",                          -1, "", 0);
    check_parse_date_basic("2026-10-01 10",                     -1, "", 0);
    check_parse_date_basic("2026-10-01 10:61:00",               -1, "", 0);
    check_parse_date_basic("2 days ago",                        -1, "", 0);
    check_parse_date_basic("yesterday 10:00",                   -1, "", 0);
    check_parse_date_basic("",                                  -1, "", 0);
    check_parse_date_basic("xyz",                               -1, "", 0);

    timestamp_t t = 0;
    int off = -1;
    int r = parse_date_basic("@99999999999999 +0000", &t, &off);
    check_number("parse_date_basic '@99999999999999 +0000' ret", (unsigned long long)(r + 1), 1);
    check_number("parse_date_basic '@99999999999999 +0000' time", t, 99999999999999ULL);
}

/***************************************************************************
 *  5. show_date_relative(): `seconds` before now
 ***************************************************************************/
PRIVATE void check_relative(long seconds, const char *expected)
{
    char bf[128] = "";
    show_date_relative((timestamp_t)(NOW - seconds), bf, sizeof(bf));
    char label[64];
    snprintf(label, sizeof(label), "show_date_relative %ld s back", seconds);
    check_text(label, bf, expected);
}

PRIVATE void test_show_date_relative(void)
{
    check_relative(0,           "0 seconds ago");
    check_relative(89,          "89 seconds ago");
    check_relative(90,          "2 minutes ago");
    check_relative(149,         "2 minutes ago");
    check_relative(150,         "3 minutes ago");
    check_relative(5369,        "89 minutes ago");
    check_relative(5370,        "2 hours ago");
    check_relative(127769,      "35 hours ago");
    check_relative(127770,      "2 days ago");
    check_relative(1164569,     "13 days ago");
    check_relative(1164570,     "2 weeks ago");
    check_relative(6002969,     "10 weeks ago");
    check_relative(6002970,     "2 months ago");
    check_relative(31490969,    "12 months ago");
    check_relative(61730970,    "2 years ago");
    check_relative(64409370,    "2 years, 1 month ago");
    check_relative(1,           "1 second ago");
    check_relative(31490970,    "1 year ago");
    check_relative(32873370,    "1 year, 1 month ago");
    check_relative(156338969,   "4 years, 11 months ago");
    check_relative(156338970,   "5 years ago");
    check_relative(315360000,   "10 years ago");
    check_relative(-1,          "in the future");
}

/***************************************************************************
 *  6. Texts that are no date are refused; some more that are read
 ***************************************************************************/
PRIVATE void test_approxidate_refused(void)
{
    static const approxidate_case_t cases[] = {
        {"tomorrow",                    "2026-10-21 11:00:00 +0200", 1},
        {"next friday",                 "2026-10-21 11:00:00 +0200", 1},
        {"in 3 days",                   "2026-10-21 11:00:00 +0200", 1},
        {"2 days from now",             "2026-10-21 11:00:00 +0200", 1},
        {"the day before yesterday",    "2026-10-21 11:00:00 +0200", 1},
        {"1.5 hours ago",               "2026-10-21 11:00:00 +0200", 1},
        {"1 decade ago",                "2026-10-21 11:00:00 +0200", 1},
        {"a day ago",                   "2026-10-21 11:00:00 +0200", 1},
        {"2daysago",                    "2026-10-21 11:00:00 +0200", 1},
        {"2026-02-30",                  "2026-10-21 11:00:00 +0200", 1},
        {"2026-13-45",                  "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-32",                  "2026-10-21 11:00:00 +0200", 1},
        {"+5",                          "2026-10-21 11:00:00 +0200", 1},
        {"-5",                          "2026-10-21 11:00:00 +0200", 1},
        {"+1d",                         "2026-10-21 11:00:00 +0200", 1},
        {"-2 friday",                   "2026-10-21 11:00:00 +0200", 1},
        {"-d",                          "2026-10-21 11:00:00 +0200", 1},
        {"10:00 -1d",                   "2026-10-21 11:00:00 +0200", 1},
        {"-86400",                      "2026-10-21 11:00:00 +0200", 1},
        {"0",                           "2026-10-21 11:00:00 +0200", 1},
        {"100",                         "2026-10-21 11:00:00 +0200", 1},
        {"1969",                        "2026-10-21 11:00:00 +0200", 1},
        {"20261001",                    "2026-10-21 11:00:00 +0200", 1},

        {"today",                       "2026-10-21 11:00:00 +0200", 0},
        {"zero days ago",               "2026-10-21 11:00:00 +0200", 0},
        {"2 secs",                      "2026-10-21 10:59:58 +0200", 0},
        {"2 mins",                      "2026-10-21 10:58:00 +0200", 0},
        {"3 hrs",                       "2026-10-21 08:00:00 +0200", 0},
        {"2 wks",                       "2026-10-07 11:00:00 +0200", 0},
        {"2 yrs",                       "2024-10-21 11:00:00 +0200", 0},
        {"10:30 yesterday",             "2026-10-20 10:30:00 +0200", 0},
        {"@0",                          "1970-01-01 01:00:00 +0100", 0},
        {"2100-01-01",                  "2100-01-01 10:00:00 +0100", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));
}

/***************************************************************************
 *  7. The edges of the calendar
 ***************************************************************************/
PRIVATE void test_calendar_edges(void)
{
    /*
     *  Tue 2026-03-31 10:00:00 UTC, 12:00 CEST (two days after the change)
     */
    s_fake_now = 1774951200;
    static const approxidate_case_t march_31[] = {
        {"1 month ago",                 "2026-02-28 11:00:00 +0100", 0},
        {"2026-10-01",                  "2026-10-01 12:00:00 +0200", 0},
        {"last friday noon",            "2026-03-27 12:00:00 +0100", 0},
    };
    check_approxidate(march_31, sizeof(march_31)/sizeof(march_31[0]));

    /*
     *  Tue 2028-02-29 10:00:00 UTC, 11:00 CET
     */
    s_fake_now = 1835431200;
    static const approxidate_case_t february_29[] = {
        {"1 year ago",                  "2027-02-28 11:00:00 +0100", 0},
        {"4 years ago",                 "2024-02-29 11:00:00 +0100", 0},
        {"31",                          "2028-02-29 11:00:00 +0100", 1},
    };
    check_approxidate(february_29, sizeof(february_29)/sizeof(february_29[0]));

    s_fake_now = NOW;
}

/***************************************************************************
 *  8. The day of a change of hour: Sun 2026-10-25 12:34:56 UTC, 13:34:56
 *  CET, a day of 25 hours (03:00 CEST was 02:00 CET)
 ***************************************************************************/
PRIVATE void test_change_of_hour(void)
{
    s_fake_now = 1792931696;
    static const approxidate_case_t cases[] = {
        {"noon yesterday",              "2026-10-24 12:00:00 +0200", 0},
        {"yesterday noon",              "2026-10-24 12:00:00 +0200", 0},
        {"midnight",                    "2026-10-25 00:00:00 +0200", 0},
        {"10:30 yesterday",             "2026-10-24 10:30:00 +0200", 0},
        {"yesterday",                   "2026-10-24 14:34:56 +0200", 0},

        /*
         *  With no clock time, the time of day and the UTC offset of now
         */
        {"2026-10-01",                  "2026-10-01 14:34:56 +0200", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));
    s_fake_now = NOW;
}

/***************************************************************************
 *  9. parse_date_basic() and a word it does not know
 ***************************************************************************/
PRIVATE void test_words(void)
{
    check_parse_date_basic("2026-10-01 10:00:00 junk",  -1, "", 0);
    check_parse_date_basic("@0",                        0, "1970-01-01 01:00:00 +0100", 60);
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;
    gbmem_get_json_allocators(&malloc_func, &free_func);
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

    test_approxidate_durations_and_times();
    test_approxidate_absolute_forms();
    test_approxidate_unreadable();
    test_parse_date_basic();
    test_show_date_relative();
    test_approxidate_refused();
    test_calendar_edges();
    test_change_of_hour();
    test_words();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
