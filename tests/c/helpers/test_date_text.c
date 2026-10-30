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
 *          10. forms the port read that 7.28.5 to 7.29.1 read otherwise or
 *              refused: M/D/Y (D.M.Y) with its year and a clock time after
 *              it, an offset after GMT/UTC ("GMT+0200"), ordinals ("5th"),
 *              the comma before a fraction of a second, a weekday in the
 *              plural ("2 fridays ago"), a month name between dashes or
 *              slashes ("01-Oct-2026", "01/Oct/2026:10:00:00 +0200");
 *              and epoch seconds no calendar holds are refused.
 *          11. more forms the port read that 7.28.5 to 7.29.2 read
 *              otherwise or refused: a zone name in a comment ("(UTC)"),
 *              D.M.Y far ahead in both orders ("5.4.2027" is April 5),
 *              Y-Mon-D ("2026-Oct-01"), epoch seconds past the year 9999
 *              (milliseconds) refused; and a zone offset after noon
 *              ("noon +0200"), a clock time of too many digits in the
 *              basic form of ISO 8601.
 *          12. an offset in a comment ("(UTC+02:00)", "(GMT-0500)",
 *              "(+0200)") is the zone when no other is given, before a
 *              zone name in a comment (7.28.5 to 7.29.3 took the name and
 *              lost the offset), but not one that is
 *              not whole ("(UTC-05:0)" was -05).
 *          13. a weekday after a date names it: "Oct 1 2026 Thu" is that
 *              day, not 2026 Thursdays back (1987-12-03, up to 7.29.3).
 *          14. any blank ("\v", "\r", "\n") parts a count from its unit,
 *              weekday or am/pm ("Oct 1 10\vpm" was 2010-10-01); a year
 *              before a weekday is a year, not a count ("2026 Thu" was
 *              1988-01-07, up to 7.29.3).
 *          15. a comment parts a count from its unit, weekday or am/pm as
 *              a blank does ("Oct 1 5 (CEST) pm" was 2005-10-01); a
 *              weekday names a day the text must say ("monday 10:00" was
 *              today at 10:00, "2026 Thu" today in 2026); a number alone
 *              after "at" is an hour with no half of the day, no date
 *              ("Oct 1 at 10" was 2010-10-01, "at 10" the 10th).
 *          16. after "at" no number alone is a date, of any value ("Oct 1
 *              at 24" was 2024-10-01), nor one after "at the" ("Oct 1 at
 *              the 10" was 2010-10-01), nor "at 10.30" (October 30), but a
 *              day before a month name is ("at 5 Oct 2026"); a count of
 *              zero and "now" name today, so a weekday after them is read
 *              ("0 days ago Wed"); "ago" with no count before it is no
 *              date ("5 (days) ago" was the 5th).
 *          17. a long text is read in time that grows with its length, not
 *              with its square: "at", 20000 comments, "on" and 20000 "the"
 *              took seconds (the place after "at" was looked for again from
 *              every piece).
 *          18. "ago" follows its count with nothing between them but blanks,
 *              dots, commas, underscores or comments ("1 day 5 ago" was
 *              October 4); right after "at"
 *              "15h" and "10h30" are clock times ("at 15h" was 15 hours
 *              back) and any other count with a unit is no date; "at 5 of
 *              October" is October 5; a weekday beside a day counted from
 *              today must be that day's, and a text says one ("now Mon" on
 *              a Wednesday, "Mon Thu now" were today); after "at" a date of
 *              numbers with dots needs a year of 4 digits ("at 10.30.00"
 *              was 2000-10-30).
 *          19. a weekday beside a day counted from today is checked against
 *              the day the text counted, not the local day of the time
 *              reached ("yesterday Sat" on a Sunday at 23:30 CET, the night
 *              of a change of hour, was refused, "yesterday Sun" read), in
 *              Madrid and Los Angeles, in autumn and in spring.
 *          20. noon, midnight and tea are of the day the text names
 *              ("today noon" before noon was yesterday's, "yesterday noon"
 *              the day before yesterday); "never" is a whole text ("now
 *              never Mon" was the epoch); no count back right after "at",
 *              with a minus sign or "last" either ("at -1d", "at last
 *              friday" were read); "last" goes with no am/pm ("last pm" was
 *              13:00); am/pm only with an hour of 12 ("at 15h am", "15:00
 *              am" were 15:00); a year and its month after "at" is a date
 *              ("at 2026.10", the day of today), "10.2026" none.
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
         *  noon, midnight and tea are the last ones that passed, or the
         *  ones of the day the text names
         */
        {"noon",                    "2026-10-20 12:00:00 +0200", 0},
        {"midnight",                "2026-10-21 00:00:00 +0200", 0},
        {"tea",                     "2026-10-20 17:00:00 +0200", 0},
        {"noon yesterday",          "2026-10-20 12:00:00 +0200", 0},
        {"yesterday noon",          "2026-10-20 12:00:00 +0200", 0},
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
 *  10. Forms the port read that 7.28.5 to 7.29.1 read otherwise, or not
 *  at all
 ***************************************************************************/
PRIVATE void test_forms_of_the_port(void)
{
    static const approxidate_case_t cases[] = {
        /*
         *  M/D/Y (D.M.Y) with its year and a clock time after it is read
         *  as written, even far ahead; without that clock time the other
         *  order is taken when it is far ahead (as the port did)
         */
        {"1.12.2026 10:00",                 "2026-12-01 10:00:00 +0100", 0},
        {"1.12.2026 10:00:00",              "2026-12-01 10:00:00 +0100", 0},
        {"01/02/2030 00:00:00",             "2030-01-02 00:00:00 +0100", 0},
        {"10/01/2027 10:00",                "2027-10-01 10:00:00 +0200", 0},
        {"3/4/27 10:00",                    "2027-03-04 10:00:00 +0100", 0},
        {"01/02/2030",                      "2030-02-01 10:00:00 +0100", 0},
        {"1.12.2026",                       "2026-01-12 10:00:00 +0100", 0},
        {"10:00 01/02/2030",                "2030-02-01 10:00:00 +0100", 0},
        {"01/02/2030 10am",                 "2030-02-01 10:00:00 +0100", 0},

        /*
         *  An offset after GMT or UTC, as Date.toString() of javascript
         *  writes it
         */
        {"Thu Oct 01 2026 10:00:00 GMT+0200 (CEST)",        "2026-10-01 10:00:00 +0200", 0},
        {"Thu Oct 01 2026 10:00:00 GMT-0500 (EST)",         "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 GMT+02:00",                   "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 GMT +0200",                   "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 UTC-0300",                    "2026-10-01 15:00:00 +0200", 0},
        {"2026-10-01 10:00:00 GMT+2",                       "2026-10-21 11:00:00 +0200", 1},

        /*
         *  Ordinals, "the" and "of"
         */
        {"July 5th",                        "2026-07-05 11:00:00 +0200", 0},
        {"1st Oct",                         "2026-10-01 11:00:00 +0200", 0},
        {"Oct 1st 2026 10:00",              "2026-10-01 10:00:00 +0200", 0},
        {"2nd March 2026",                  "2026-03-02 10:00:00 +0100", 0},
        {"3rd",                             "2026-10-03 11:00:00 +0200", 0},
        {"the 5th of July",                 "2026-07-05 11:00:00 +0200", 0},
        {"1st of October",                  "2026-10-01 11:00:00 +0200", 0},
        {"32nd",                            "2026-10-21 11:00:00 +0200", 1},
        {"0th",                             "2026-10-21 11:00:00 +0200", 1},

        /*
         *  The comma of ISO 8601 before the fraction of a second
         */
        {"2026-10-01 10:00:00,5",           "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01T10:00:00,123Z",        "2026-10-01 12:00:00 +0200", 0},

        /*
         *  A weekday in the plural after its count
         */
        {"2 fridays ago",                   "2026-10-09 11:00:00 +0200", 0},
        {"3 fridays",                       "2026-10-02 11:00:00 +0200", 0},

        /*
         *  A month name between dashes or slashes: D-Mon-Y, Mon-D-Y, and
         *  the common log format of web servers
         */
        {"01-Oct-2026",                     "2026-10-01 11:00:00 +0200", 0},
        {"1-oct-26",                        "2026-10-01 11:00:00 +0200", 0},
        {"01/Oct/2026",                     "2026-10-01 11:00:00 +0200", 0},
        {"Oct-01-2026",                     "2026-10-01 11:00:00 +0200", 0},
        {"01-Oct-2026 10:00",               "2026-10-01 10:00:00 +0200", 0},
        {"01/Oct/2026:10:00:00 +0200",      "2026-10-01 10:00:00 +0200", 0},
        {"21/Oct/2026:08:30:00 +0000",      "2026-10-21 10:30:00 +0200", 0},
        {"31-Feb-2026",                     "2026-10-21 11:00:00 +0200", 1},
        {"01-Foo-2026",                     "2026-10-21 11:00:00 +0200", 1},

        /*
         *  Epoch seconds that no calendar can hold are no date (it was
         *  read, with an internal error logged)
         */
        {"@9223372036854775807",            "2026-10-21 11:00:00 +0200", 1},
        {"@9223372036854775807 +0000",      "2026-10-21 11:00:00 +0200", 1},
        {"9223372036854775807",             "2026-10-21 11:00:00 +0200", 1},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));

    check_parse_date_basic("1.12.2026 10:00",                           0, "2026-12-01 10:00:00 +0100", 60);
    check_parse_date_basic("10/01/2027 10:00",                          0, "2027-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("Thu Oct 01 2026 10:00:00 GMT+0200 (CEST)",  0, "2026-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("Wed Oct 28 2026 18:00:00 GMT+0100 (hora estándar de Europa central)",
                                                                        0, "2026-10-28 18:00:00 +0100", 60);
    check_parse_date_basic("Oct 1st 2026 10:00",                        0, "2026-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("2026-10-01 10:00:00,5",                     0, "2026-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("01/Oct/2026:10:00:00 +0200",                0, "2026-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("@9223372036854775807",                      -1, "", 0);
    check_parse_date_basic("@9223372036854775807 +0000",                -1, "", 0);
}

/***************************************************************************
 *  11. More forms the port read that 7.28.5 to 7.29.2 read otherwise, or
 *  not at all
 ***************************************************************************/
PRIVATE void test_more_forms_of_the_port(void)
{
    static const approxidate_case_t cases[] = {
        /*
         *  A zone name in a comment is the zone when no other is given;
         *  a zone given outside the comment wins
         */
        {"2026-10-01 10:00:00 (UTC)",                       "2026-10-01 12:00:00 +0200", 0},
        {"Thu, 01 Oct 2026 10:00:00 (EST)",                 "2026-10-01 17:00:00 +0200", 0},
        {"(EST) 2026-10-01 10:00:00",                       "2026-10-01 17:00:00 +0200", 0},
        {"2026-01-15 10:00:00 (CEST)",                      "2026-01-15 09:00:00 +0100", 0},
        {"2026-10-01 10:00:00 (UTC EST)",                   "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01 10:00:00 +0000 (EST)",                 "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01 10:00:00 EST (UTC)",                   "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (Central European Time)",     "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-28 18:00:00 (hora estándar de Europa central)",
                                                            "2026-10-28 18:00:00 +0100", 0},
        {"(UTC)",                                           "2026-10-21 11:00:00 +0200", 1},

        /*
         *  D.M.Y (D.M) takes the other order only when that one is not
         *  far ahead too; M/D/Y takes it whenever the first is far ahead
         */
        {"5.4.2027",                        "2027-04-05 11:00:00 +0200", 0},
        {"4.5.2027",                        "2027-05-04 11:00:00 +0200", 0},
        {"11.12.2026",                      "2026-12-11 10:00:00 +0100", 0},
        {"12.11",                           "2025-11-12 10:00:00 +0100", 0},
        {"1.12.2026",                       "2026-01-12 10:00:00 +0100", 0},
        {"4/5/2027",                        "2027-05-04 11:00:00 +0200", 0},
        {"12/11",                           "2025-11-12 10:00:00 +0100", 0},

        /*
         *  Y-Mon-D: a month name between dashes or slashes, after the year
         */
        {"2026-Oct-01",                     "2026-10-01 11:00:00 +0200", 0},
        {"2026-oct-1",                      "2026-10-01 11:00:00 +0200", 0},
        {"2026/Oct/01",                     "2026-10-01 11:00:00 +0200", 0},
        {"2026-October-01",                 "2026-10-01 11:00:00 +0200", 0},
        {"2026-Oct-01 10:00",               "2026-10-01 10:00:00 +0200", 0},
        {"2026-Oct-01T10:00:00Z",           "2026-10-01 12:00:00 +0200", 0},
        {"2026-Feb-29",                     "2026-10-21 11:00:00 +0200", 1},
        {"2026-Oct-32",                     "2026-10-21 11:00:00 +0200", 1},
        {"2026-Oct",                        "2026-10-21 11:00:00 +0200", 1},
        {"2026-Oct-",                       "2026-10-21 11:00:00 +0200", 1},
        {"2026-Oct/01",                     "2026-10-21 11:00:00 +0200", 1},
        {"1969-Oct-01",                     "2026-10-21 11:00:00 +0200", 1},

        /*
         *  Epoch seconds past the year 9999 are no date, unless written
         *  as "@<seconds> <offset>": milliseconds are not seconds
         */
        {"1761000000000",                   "2026-10-21 11:00:00 +0200", 1},
        {"@1761000000000",                  "2026-10-21 11:00:00 +0200", 1},
        {"1761000000000 +0000",             "2026-10-21 11:00:00 +0200", 1},
        {"@1761000000000 UTC",              "2026-10-21 11:00:00 +0200", 1},
        {"253402300800",                    "2026-10-21 11:00:00 +0200", 1},
        {"4102444800",                      "2100-01-01 01:00:00 +0100", 0},

        /*
         *  A zone offset after noon, midnight or tea, as a zone name
         */
        {"noon +0200",                      "2026-10-20 12:00:00 +0200", 0},
        {"noon CEST",                       "2026-10-20 12:00:00 +0200", 0},
        {"noon -0500",                      "2026-10-20 19:00:00 +0200", 0},
        {"midnight +0000",                  "2026-10-21 02:00:00 +0200", 0},
        {"tea +02:00",                      "2026-10-20 17:00:00 +0200", 0},
        {"noon -1d",                        "2026-10-20 12:00:00 +0200", 0},

        /*
         *  The basic form of ISO 8601 with a clock time of too many digits
         *  (it overflowed a signed integer)
         */
        {"20261001T99999999999999999999",   "2026-10-21 11:00:00 +0200", 1},
        {"20261001T1000000",                "2026-10-21 11:00:00 +0200", 1},
        {"20261001T1000",                   "2026-10-01 10:00:00 +0200", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));

    int error = 0;
    check_number("approxidate '253402300799'", approxidate_careful("253402300799", &error), 253402300799ULL);
    check_number("approxidate '253402300799' error", (unsigned long long)error, 0);
    check_number("approxidate '@1761000000000 +0000'",
        approxidate_careful("@1761000000000 +0000", &error), 1761000000000ULL);
    check_number("approxidate '@1761000000000 +0000' error", (unsigned long long)error, 0);

    check_parse_date_basic("2026-10-01 10:00:00 (UTC)",                 0, "2026-10-01 12:00:00 +0200", 0);
    check_parse_date_basic("Thu, 01 Oct 2026 10:00:00 (EST)",           0, "2026-10-01 17:00:00 +0200", -300);
    check_parse_date_basic("2026-10-01 10:00:00 +0000 (EST)",           0, "2026-10-01 12:00:00 +0200", 0);
    check_parse_date_basic("2026-Oct-01 10:00:00",                      0, "2026-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("1761000000000",                             -1, "", 0);
    check_parse_date_basic("1761000000000 +0000",                       -1, "", 0);
    check_parse_date_basic("@1761000000000",                            -1, "", 0);
    check_parse_date_basic("noon +0200",                                -1, "", 0);
    check_parse_date_basic("20261001T99999999999999999999",             -1, "", 0);
}

/***************************************************************************
 *  12. An offset in a comment
 ***************************************************************************/
PRIVATE void test_offset_in_a_comment(void)
{
    static const approxidate_case_t cases[] = {
        /*
         *  The first offset of the comments is the zone when no other is
         *  given, before a zone name of the comments; one given outside
         *  the comment wins
         */
        {"2026-10-01 10:00:00 (UTC+02:00)",                 "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC-05:00)",                 "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (GMT+0200)",                  "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (GMT-0500)",                  "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC +0200)",                 "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (-0500)",                     "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (+02:00)",                    "2026-10-01 10:00:00 +0200", 0},
        {"2026-01-15 10:00:00 (+0200)",                     "2026-01-15 09:00:00 +0100", 0},
        {"Thu Oct 01 2026 10:00:00 (GMT-0500)",             "2026-10-01 17:00:00 +0200", 0},
        {"(UTC-05:00) 2026-10-01 10:00:00",                 "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (EST+0100)",                  "2026-10-01 11:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (CEST) (UTC-05:00)",          "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC-05:00) (CEST)",          "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC+02:00) (UTC-05:00)",     "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 +0000 (UTC-05:00)",           "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01 10:00:00 EST (UTC+02:00)",             "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC+02:00) +0000",           "2026-10-01 12:00:00 +0200", 0},

        /*
         *  What is no offset: a sign after a digit, a letter after the
         *  digits, a sign with one digit, an hour past 23, a minute past 59
         */
        {"2026-10-01 10:00:00 (2026-10-01)",                "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (12-05)",                     "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC-0500z)",                 "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC+2)",                     "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC+2400)",                  "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC+0260)",                  "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (+)",                         "2026-10-01 10:00:00 +0200", 0},

        /*
         *  Nor an offset that is not whole: a ':' after the hours needs
         *  both digits of the minutes, as outside a comment
         */
        {"2026-10-01 10:00:00 (-05:0)",                     "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (-05:)",                      "2026-10-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC-05:0)",                  "2026-10-01 12:00:00 +0200", 0},
        {"2026-10-01 10:00:00 (UTC-05:0) (UTC+03:00)",      "2026-10-01 09:00:00 +0200", 0},
        {"2026-10-01 10:00:00 -05:0",                       "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01 10:00:00 (UTC+02:00",                  "2026-10-21 11:00:00 +0200", 1},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));

    check_parse_date_basic("2026-10-01 10:00:00 (UTC+02:00)",           0, "2026-10-01 10:00:00 +0200", 120);
    check_parse_date_basic("2026-10-01 10:00:00 (UTC-05:00)",           0, "2026-10-01 17:00:00 +0200", -300);
    check_parse_date_basic("2026-10-01 10:00:00 (GMT+0530)",            0, "2026-10-01 06:30:00 +0200", 330);
    check_parse_date_basic("2026-10-01 10:00:00 +0000 (UTC-05:00)",     0, "2026-10-01 12:00:00 +0200", 0);
}

/***************************************************************************
 *  13. A weekday after a date names it: the number before it is that
 *  date's day or year, not a count of weekdays back. The port of git's
 *  date.c, and the rewrite up to 7.29.3, read "Oct 1 2026 Thu" as 2026
 *  Thursdays back (1987-12-03) and "Oct 5 friday" as 5 Fridays back.
 ***************************************************************************/
PRIVATE void test_weekday_after_a_date(void)
{
    static const approxidate_case_t cases[] = {
        {"Oct 1 2026 Thu",                  "2026-10-01 11:00:00 +0200", 0},
        {"1 Oct 2026 Thu",                  "2026-10-01 11:00:00 +0200", 0},
        {"Oct 1, 2026, Thursday 10:00",     "2026-10-01 10:00:00 +0200", 0},
        {"Oct 1 26 Thu",                    "2026-10-01 11:00:00 +0200", 0},
        {"Oct 5 friday",                    "2026-10-05 11:00:00 +0200", 0},
        {"2026-10-01 5 friday",             "2026-10-21 11:00:00 +0200", 1},

        /*
         *  With no date before it, or in the plural, it is a count
         */
        {"2 wednesday",                     "2026-10-07 11:00:00 +0200", 0},
        {"yesterday 1 wednesday",           "2026-10-14 11:00:00 +0200", 0},
        {"Oct 1 2 fridays",                 "2026-09-18 11:00:00 +0200", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));
}

/***************************************************************************
 *  14. Any blank parts a count from its unit, its weekday or its am/pm,
 *  as it parts any two pieces: up to 7.29.3 only a space or a tab did,
 *  so "Oct 1 10\vpm" was 2010-10-01 and "2\ndays ago" no date. And a
 *  year (four digits, 1970-9999) before a weekday is a year, never a
 *  count of weekdays back: "2026 Thu" was 2026 Thursdays back
 *  (1988-01-07), "Oct 1 2026 thursdays" 1987-12-03.
 ***************************************************************************/
PRIVATE void test_blanks_and_years_before_weekdays(void)
{
    static const approxidate_case_t cases[] = {
        {"Oct 1 10\vpm",                    "2026-10-01 22:00:00 +0200", 0},
        {"Oct 1 10\rpm",                    "2026-10-01 22:00:00 +0200", 0},
        {"Oct 1 10\npm",                    "2026-10-01 22:00:00 +0200", 0},
        {"Oct 1 10\fpm",                    "2026-10-01 22:00:00 +0200", 0},
        {"2\ndays ago",                     "2026-10-19 11:00:00 +0200", 0},
        {"1\r\nwednesday",                  "2026-10-14 11:00:00 +0200", 0},
        {"-1\vd",                           "2026-10-20 11:00:00 +0200", 0},
        {"Oct 1 2026\nThu",                 "2026-10-01 11:00:00 +0200", 0},

        {"2026 Thu",                        "2026-10-21 11:00:00 +0200", 1},     // no day (15)
        {"2026, thursday",                  "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 2026 thursdays",            "2026-10-21 11:00:00 +0200", 1},
        {"2026 fridays ago",                "2026-10-21 11:00:00 +0200", 1},

        /*
         *  Not a year: still a count
         */
        {"100 fridays ago",                 "2024-11-22 10:00:00 +0100", 0},
        {"2026 days ago",                   "2021-04-04 11:00:00 +0200", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));
}

/***************************************************************************
 *  15. A comment ("(CEST)") parts a count from its unit, its weekday or
 *  its am/pm as a blank does, as it parts any two pieces: up to 7.29.3
 *  "Oct 1 5 (CEST) pm" was 2005-10-01 and "2 (x) days ago" no date. A
 *  weekday names a day, and the text must say which, as a date or
 *  counted from today ("yesterday Tue", "2 days ago Mon"): "monday
 *  10:00" was today at 10:00, "2026 Thu" today in 2026. A number alone after "at"
 *  is an hour that says no half of the day, so no date: "Oct 1 at 10" was
 *  2010-10-01, "at 10" the 10th of this month.
 ***************************************************************************/
PRIVATE void test_comments_weekdays_and_at(void)
{
    static const approxidate_case_t cases[] = {
        {"Oct 1 5 (CEST) pm",               "2026-10-01 17:00:00 +0200", 0},
        {"Oct 29 3 (CEST) PM",              "2026-10-29 14:00:00 +0100", 0},
        {"1 Oct 5 (CEST) pm",               "2026-10-01 17:00:00 +0200", 0},
        {"Oct 1 2026 5 (UTC) pm",           "2026-10-01 19:00:00 +0200", 0},
        {"Oct 1 5 (x)(y) pm",               "2026-10-01 17:00:00 +0200", 0},
        {"2 (x) days ago",                  "2026-10-19 11:00:00 +0200", 0},
        {"-1 (x) d",                        "2026-10-20 11:00:00 +0200", 0},
        {"1 (x) wednesday",                 "2026-10-14 11:00:00 +0200", 0},
        {"Oct 1 2026 (x) Thu",              "2026-10-01 11:00:00 +0200", 0},
        {"Oct 1 5 (CEST pm",                "2026-10-21 11:00:00 +0200", 1},

        /*
         *  A weekday with no day
         */
        {"monday 10:00",                    "2026-10-21 11:00:00 +0200", 1},
        {"10:00 monday",                    "2026-10-21 11:00:00 +0200", 1},
        {"Thu noon",                        "2026-10-21 11:00:00 +0200", 1},
        {"Oct Thu",                         "2026-10-21 11:00:00 +0200", 1},
        {"Oct 2026 Thu",                    "2026-10-21 11:00:00 +0200", 1},
        {"yesterday Tue",                   "2026-10-20 11:00:00 +0200", 0},
        {"today Wed",                       "2026-10-21 11:00:00 +0200", 0},
        {"-1d Tue",                         "2026-10-20 11:00:00 +0200", 0},
        {"2 days ago Mon",                  "2026-10-19 11:00:00 +0200", 0},
        {"Thu, 01 Oct 2026",                "2026-10-01 11:00:00 +0200", 0},
        {"2026-10-01 Thu 10:00",            "2026-10-01 10:00:00 +0200", 0},
        {"last monday 10:00",               "2026-10-19 10:00:00 +0200", 0},

        /*
         *  A number alone after "at"
         */
        {"Oct 1 at 10",                     "2026-10-21 11:00:00 +0200", 1},
        {"at 10",                           "2026-10-21 11:00:00 +0200", 1},
        {"1 Oct at 3",                      "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at (CEST) 10",              "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at 10 pm",                  "2026-10-01 22:00:00 +0200", 0},
        {"Oct 1 at 10pm",                   "2026-10-01 22:00:00 +0200", 0},
        {"Oct 1 at 10:00",                  "2026-10-01 10:00:00 +0200", 0},
        {"at 2026-10-01 10:00",             "2026-10-01 10:00:00 +0200", 0},
        {"Oct 1 2026 at 10:00",             "2026-10-01 10:00:00 +0200", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));
}

/***************************************************************************
 *  16. After "at" a person means a time, so no number alone there is a
 *  date, whatever its value: "Oct 1 at 24" was 2024-10-01, "Oct 1 at 30"
 *  2030-10-01, "Oct at 26" October 26, "Oct 1 at the 10" 2010-10-01. A
 *  number before a month name is a day ("at 5 Oct 2026"; a development
 *  version refused it). "at 10.30" means 10:30, and was October 30: no
 *  date (write "at 10:30"). A count of zero and "now" name today, as
 *  "today" does: "0 days ago Wed" and "now Wed" are today (a development
 *  version refused them; a weekday that is not today's is refused, see
 *  18). "ago" says how long back a count is, and with
 *  no count it is no date: "5 (days) ago" and "5 ago" were the 5th.
 ***************************************************************************/
PRIVATE void test_at_zero_counts_and_ago(void)
{
    static const approxidate_case_t cases[] = {
        /*
         *  No number alone after "at"
         */
        {"Oct 1 at 24",                     "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at 30",                     "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at 99",                     "2026-10-21 11:00:00 +0200", 1},
        {"at 24",                           "2026-10-21 11:00:00 +0200", 1},
        {"Oct at 26",                       "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at 2026",                   "2026-10-21 11:00:00 +0200", 1},
        {"at 2026",                         "2026-10-21 11:00:00 +0200", 1},
        {"at 1761000000",                   "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at the 10",                 "2026-10-21 11:00:00 +0200", 1},
        {"at the 10",                       "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at (x) the 24",             "2026-10-21 11:00:00 +0200", 1},
        {"at 10.30",                        "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at 10.30",                  "2026-10-21 11:00:00 +0200", 1},

        /*
         *  A day before a month name, a date of numbers, an ordinal
         */
        {"at 5 Oct 2026",                   "2026-10-05 11:00:00 +0200", 0},
        {"at 26 Oct 2026",                  "2026-10-26 10:00:00 +0100", 0},
        {"at 5 October",                    "2026-10-05 11:00:00 +0200", 0},
        {"at 5, Oct 2026 10:00",            "2026-10-05 10:00:00 +0200", 0},
        {"at 1.10.2026",                    "2026-10-01 11:00:00 +0200", 0},
        {"at the 5th",                      "2026-10-05 11:00:00 +0200", 0},
        {"Oct 1 at 10:30",                  "2026-10-01 10:30:00 +0200", 0},
        {"Oct 1 at 10 pm",                  "2026-10-01 22:00:00 +0200", 0},

        /*
         *  A count of zero and "now" name today
         */
        {"0 hours ago Wed",                 "2026-10-21 11:00:00 +0200", 0},
        {"0 months ago Wed",                "2026-10-21 11:00:00 +0200", 0},
        {"zero days ago Wed",               "2026-10-21 11:00:00 +0200", 0},
        {"now Wed",                         "2026-10-21 11:00:00 +0200", 0},
        {"0 days ago",                      "2026-10-21 11:00:00 +0200", 0},

        /*
         *  "ago" needs a count
         */
        {"5 (days) ago",                    "2026-10-21 11:00:00 +0200", 1},
        {"5 ago",                           "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 ago",                       "2026-10-21 11:00:00 +0200", 1},
        {"ago",                             "2026-10-21 11:00:00 +0200", 1},
        {"5 days (x) ago",                  "2026-10-16 11:00:00 +0200", 0},
        {"last friday ago",                 "2026-10-16 11:00:00 +0200", 0},
        {"yesterday ago",                   "2026-10-20 11:00:00 +0200", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));
}

/***************************************************************************
 *  17. A long hostile text is read in linear time: "at", 20000 comments,
 *  "on" and 20000 "the" (140 KB) took about 2 s, because the piece right
 *  after "at" was looked for again, past every comment, from each "the".
 *  It is no date (no number in it), and must be refused well under a
 *  second, also under a sanitizer.
 ***************************************************************************/
PRIVATE void test_long_hostile_text(void)
{
    const int n = 20000;
    size_t size = 2 + (size_t)n*3 + 3 + (size_t)n*4 + 1;
    char *text = gbmem_malloc(size);
    if(!text) {
        printf("FAIL no memory for the long text\n");
        global_result += -1;
        return;
    }
    char *p = text;
    memcpy(p, "at", 2);
    p += 2;
    for(int i = 0; i < n; i++) {
        memcpy(p, "(x)", 3);
        p += 3;
    }
    memcpy(p, " on", 3);
    p += 3;
    for(int i = 0; i < n; i++) {
        memcpy(p, " the", 4);
        p += 4;
    }
    *p = 0;

    int error = 0;
    uint64_t timer = start_msectimer(500);
    approxidate_careful(text, &error);
    BOOL too_slow = test_msectimer(timer);
    if(error != 1 || too_slow) {
        printf("FAIL approxidate of a long text of %zu bytes: error %d (expected 1), %s\n",
            strlen(text), error, too_slow? "500 ms passed" : "in time");
        global_result += -1;
    }
    gbmem_free(text);
}

/***************************************************************************
 *  18. "ago" follows its count, with nothing but blanks, dots, commas,
 *  underscores or comments between them: "1 day 5 ago" and "yesterday 5 ago" were October 4 (the
 *  5 a day, the "ago" taken as the one of the count before). Right after
 *  "at" a number with a unit is no count back: "15h" and "10h30" are
 *  clock times there (15:00 and 10:30, as they are written in Spanish or
 *  French), "at 15h" was 15 hours back and "at 10h30" 10 hours back from
 *  the 30th; any other unit or a weekday there is no date ("at 5 days").
 *  "of" between a day and its month name after "at" is passed over, as
 *  anywhere else: "at 5 of October" is October 5 (a development version
 *  refused it). A weekday beside a day counted from today ("now",
 *  "today", a count back) must be that day's: "now Mon", "0 days ago
 *  Mon" were today on a Wednesday, "now 2026 Mon" too, and a text says
 *  one weekday ("Mon Thu now"). Beside a date of the calendar it is still
 *  not checked. After "at" a date of numbers with dots needs a year of
 *  4 digits: "at 10.30.00" is 10:30:00 for a person, and was 2000-10-30.
 ***************************************************************************/
PRIVATE void test_ago_at_counts_and_weekdays(void)
{
    static const approxidate_case_t cases[] = {
        /*
         *  "ago" right after its count
         */
        {"1 day 5 ago",                     "2026-10-21 11:00:00 +0200", 1},
        {"yesterday 5 ago",                 "2026-10-21 11:00:00 +0200", 1},
        {"2 days Oct ago",                  "2026-10-21 11:00:00 +0200", 1},
        {"1 day 10:00 ago",                 "2026-10-21 11:00:00 +0200", 1},
        {"2 days ago ago",                  "2026-10-21 11:00:00 +0200", 1},
        {"now ago",                         "2026-10-21 11:00:00 +0200", 1},
        {"5 days ago",                      "2026-10-16 11:00:00 +0200", 0},
        {"1.day.ago",                       "2026-10-20 11:00:00 +0200", 0},
        {"2 days 3 hours ago",              "2026-10-19 08:00:00 +0200", 0},
        {"2 days (x) (y) ago",              "2026-10-19 11:00:00 +0200", 0},
        {"-1w ago",                         "2026-10-14 11:00:00 +0200", 0},
        {"2 fridays ago",                   "2026-10-09 11:00:00 +0200", 0},
        {"last week ago",                   "2026-10-14 11:00:00 +0200", 0},
        {"3 days ago at 10:00",             "2026-10-18 10:00:00 +0200", 0},

        /*
         *  A count with a unit right after "at"
         */
        {"at 15h",                          "2026-10-21 15:00:00 +0200", 0},
        {"at 15H",                          "2026-10-21 15:00:00 +0200", 0},
        {"at 9h",                           "2026-10-21 09:00:00 +0200", 0},
        {"at 10h30",                        "2026-10-21 10:30:00 +0200", 0},
        {"Oct 1 at 15h",                    "2026-10-01 15:00:00 +0200", 0},
        {"Oct 1 at the 10h30",              "2026-10-01 10:30:00 +0200", 0},
        {"yesterday at 10h30",              "2026-10-20 10:30:00 +0200", 0},
        {"at 10h30 pm",                     "2026-10-21 11:00:00 +0200", 1},
        {"at 25h",                          "2026-10-21 11:00:00 +0200", 1},
        {"at 10h60",                        "2026-10-21 11:00:00 +0200", 1},
        {"at 10h3",                         "2026-10-21 11:00:00 +0200", 1},
        {"at 10h300",                       "2026-10-21 11:00:00 +0200", 1},
        {"at 100h",                         "2026-10-21 11:00:00 +0200", 1},
        {"at 15h ago",                      "2026-10-21 11:00:00 +0200", 1},
        {"at 15 h",                         "2026-10-21 11:00:00 +0200", 1},
        {"at 15hours",                      "2026-10-21 11:00:00 +0200", 1},
        {"at 5 days",                       "2026-10-21 11:00:00 +0200", 1},
        {"at 2 hours ago",                  "2026-10-21 11:00:00 +0200", 1},
        {"at 2 fridays ago",                "2026-10-21 11:00:00 +0200", 1},
        {"at 2 friday",                     "2026-10-21 11:00:00 +0200", 1},
        {"at five days ago",                "2026-10-21 11:00:00 +0200", 1},
        {"at five pm",                      "2026-10-21 17:00:00 +0200", 0},
        {"at 3pm",                          "2026-10-21 15:00:00 +0200", 0},
        {"15h",                             "2026-10-20 20:00:00 +0200", 0},
        {"15h ago",                         "2026-10-20 20:00:00 +0200", 0},

        /*
         *  "of" between a day and its month name after "at"
         */
        {"at 5 of October",                 "2026-10-05 11:00:00 +0200", 0},
        {"at 5 of Oct 2026",                "2026-10-05 11:00:00 +0200", 0},
        {"at 5 (x) of (y) October",         "2026-10-05 11:00:00 +0200", 0},
        {"at 5 of of October",              "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at 5 of",                   "2026-10-21 11:00:00 +0200", 1},
        {"at 5 of 2026",                    "2026-10-21 11:00:00 +0200", 1},

        /*
         *  A weekday beside a day counted from today is that day's
         */
        {"now Mon",                         "2026-10-21 11:00:00 +0200", 1},
        {"today Mon",                       "2026-10-21 11:00:00 +0200", 1},
        {"0 days ago Mon",                  "2026-10-21 11:00:00 +0200", 1},
        {"zero days ago Mon",               "2026-10-21 11:00:00 +0200", 1},
        {"now 2026 Mon",                    "2026-10-21 11:00:00 +0200", 1},
        {"now 2026 Wed",                    "2026-10-21 11:00:00 +0200", 0},
        {"yesterday Wed",                   "2026-10-21 11:00:00 +0200", 1},
        {"yesterday Tue",                   "2026-10-20 11:00:00 +0200", 0},
        {"2 days ago Tue",                  "2026-10-21 11:00:00 +0200", 1},
        {"2 days ago Mon",                  "2026-10-19 11:00:00 +0200", 0},
        {"12 hours ago Tue",                "2026-10-20 23:00:00 +0200", 0},
        {"12 hours ago Wed",                "2026-10-21 11:00:00 +0200", 1},
        {"last friday Fri",                 "2026-10-16 11:00:00 +0200", 0},
        {"last friday Thu",                 "2026-10-21 11:00:00 +0200", 1},
        {"Wed noon",                        "2026-10-21 11:00:00 +0200", 1},
        {"yesterday 23:30 Tue",             "2026-10-20 23:30:00 +0200", 0},
        {"Tue 23:30 UTC yesterday",         "2026-10-21 01:30:00 +0200", 0},

        /*
         *  One weekday in a text
         */
        {"Mon Thu now",                     "2026-10-21 11:00:00 +0200", 1},
        {"Wed Wed now",                     "2026-10-21 11:00:00 +0200", 1},
        {"Wed, Wed now",                    "2026-10-21 11:00:00 +0200", 1},
        {"Mon Thu, 01 Oct 2026",            "2026-10-21 11:00:00 +0200", 1},
        {"Thu, 01 Oct 2026",                "2026-10-01 11:00:00 +0200", 0},
        {"Oct 5 2026 friday",               "2026-10-05 11:00:00 +0200", 0},

        /*
         *  After "at" a date of numbers with dots needs its year of 4 digits
         */
        {"at 10.30.00",                     "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at 10.30.00",               "2026-10-21 11:00:00 +0200", 1},
        {"at 1.10.26",                      "2026-10-21 11:00:00 +0200", 1},
        {"at 10.30.00.000",                 "2026-10-21 11:00:00 +0200", 1},
        {"at 1.10.2026",                    "2026-10-01 11:00:00 +0200", 0},
        {"at 01.10.2026 10:00",             "2026-10-01 10:00:00 +0200", 0},
        {"at 2026.10.01",                   "2026-10-01 11:00:00 +0200", 0},
        {"1.10.26",                         "2026-10-01 11:00:00 +0200", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));
}

/***************************************************************************
 *  19. A weekday beside a day counted from today is checked against the
 *  day the text counted, in the UTC offset of now, not against the local
 *  day of the time reached: on the night of a change of hour that time is
 *  on the next (or the day before) local day. Sunday 2026-10-25 23:30
 *  CET in Madrid: "yesterday Sat" is Saturday at 23:30 +0100, which is
 *  Sunday 00:30 CEST, and was refused, while "yesterday Sun" was read.
 *  The same the night after the change of spring, and in Los Angeles.
 ***************************************************************************/
PRIVATE void test_weekdays_on_a_change_of_hour(void)
{
    /*
     *  Sun 2026-10-25 22:30:00 UTC, 23:30 CET (the day of 25 hours)
     */
    s_fake_now = 1792967400;
    static const approxidate_case_t autumn[] = {
        {"yesterday Sat",                   "2026-10-25 00:30:00 +0200", 0},
        {"yesterday Sun",                   "2026-10-25 23:30:00 +0100", 1},
        {"last friday Fri",                 "2026-10-24 00:30:00 +0200", 0},
        {"last friday Sat",                 "2026-10-25 23:30:00 +0100", 1},
        {"2 days ago Fri",                  "2026-10-24 00:30:00 +0200", 0},
        {"2 days ago Sat",                  "2026-10-25 23:30:00 +0100", 1},
        {"24 hours ago Sat",                "2026-10-25 00:30:00 +0200", 0},
        {"now Sun",                         "2026-10-25 23:30:00 +0100", 0},
        {"yesterday 23:30 Sat",             "2026-10-24 23:30:00 +0200", 0},
        {"yesterday 23:30 Sun",             "2026-10-25 23:30:00 +0100", 1},
    };
    check_approxidate(autumn, sizeof(autumn)/sizeof(autumn[0]));

    /*
     *  Sun 2026-03-29 22:30:00 UTC, Mon 2026-03-30 00:30 CEST (the night
     *  after the day of 23 hours)
     */
    s_fake_now = 1774823400;
    static const approxidate_case_t spring[] = {
        {"yesterday Sun",                   "2026-03-28 23:30:00 +0100", 0},
        {"yesterday Sat",                   "2026-03-30 00:30:00 +0200", 1},
        {"2 days ago Sat",                  "2026-03-27 23:30:00 +0100", 0},
        {"2 days ago Fri",                  "2026-03-30 00:30:00 +0200", 1},
        {"last friday Fri",                 "2026-03-26 23:30:00 +0100", 0},
        {"last friday Thu",                 "2026-03-30 00:30:00 +0200", 1},
        {"now Mon",                         "2026-03-30 00:30:00 +0200", 0},
    };
    check_approxidate(spring, sizeof(spring)/sizeof(spring[0]));

    setenv("TZ", "America/Los_Angeles", 1);
    tzset();

    /*
     *  Mon 2026-11-02 07:30:00 UTC, Sun 2026-11-01 23:30 PST
     */
    s_fake_now = 1793604600;
    static const approxidate_case_t la_autumn[] = {
        {"yesterday Sat",                   "2026-11-01 00:30:00 -0700", 0},
        {"yesterday Sun",                   "2026-11-01 23:30:00 -0800", 1},
        {"last friday Fri",                 "2026-10-31 00:30:00 -0700", 0},
        {"last friday Sat",                 "2026-11-01 23:30:00 -0800", 1},
    };
    check_approxidate(la_autumn, sizeof(la_autumn)/sizeof(la_autumn[0]));

    /*
     *  Mon 2026-03-09 07:30:00 UTC, 00:30 PDT
     */
    s_fake_now = 1773041400;
    static const approxidate_case_t la_spring[] = {
        {"yesterday Sun",                   "2026-03-07 23:30:00 -0800", 0},
        {"yesterday Sat",                   "2026-03-09 00:30:00 -0700", 1},
        {"2 days ago Sat",                  "2026-03-06 23:30:00 -0800", 0},
        {"2 days ago Fri",                  "2026-03-09 00:30:00 -0700", 1},
    };
    check_approxidate(la_spring, sizeof(la_spring)/sizeof(la_spring[0]));

    setenv("TZ", "Europe/Madrid", 1);
    tzset();
    s_fake_now = NOW;
}

/***************************************************************************
 *  20. noon, midnight and tea are of the day the text names ("today
 *  noon" before noon was yesterday's, and "today noon Wed" was refused
 *  on a Wednesday while "today noon Tue" was read; "yesterday noon" was
 *  the day before yesterday); alone they are still the last ones that
 *  passed. "never" is a whole text ("now never Mon" was the epoch). No
 *  count back right after "at", with a minus sign or "last" either ("at
 *  -5 days", "at last friday" were read, "at 5 days" refused); "last"
 *  takes a unit or a weekday, not am/pm ("last pm" was 13:00). am/pm is
 *  no half of the day of an hour with an 'h', of 24 hours ("at 15h am"
 *  was 15:00), nor of an hour past 12 ("15:00 am" was 15:00). A year and
 *  its month with dots is a date after "at" too ("at 2026.10", the day
 *  of today, as "2026.10" and "2026-10"); "10.2026" is no date anywhere.
 ***************************************************************************/
PRIVATE void test_noon_never_at_and_am_pm(void)
{
    static const approxidate_case_t cases[] = {
        /*
         *  noon, midnight and tea of the day named: Wed 11:00
         */
        {"today noon",                      "2026-10-21 12:00:00 +0200", 0},
        {"noon today",                      "2026-10-21 12:00:00 +0200", 0},
        {"today noon Wed",                  "2026-10-21 12:00:00 +0200", 0},
        {"today noon Tue",                  "2026-10-21 11:00:00 +0200", 1},
        {"now noon",                        "2026-10-21 12:00:00 +0200", 0},
        {"today tea",                       "2026-10-21 17:00:00 +0200", 0},
        {"today midnight",                  "2026-10-21 00:00:00 +0200", 0},
        {"0 days ago noon",                 "2026-10-21 12:00:00 +0200", 0},
        {"yesterday noon Tue",              "2026-10-20 12:00:00 +0200", 0},
        {"yesterday noon Mon",              "2026-10-21 11:00:00 +0200", 1},
        {"yesterday tea",                   "2026-10-20 17:00:00 +0200", 0},
        {"2 days ago noon",                 "2026-10-19 12:00:00 +0200", 0},
        {"last friday noon",                "2026-10-16 12:00:00 +0200", 0},
        {"1 week ago noon",                 "2026-10-14 12:00:00 +0200", 0},
        {"today noon -0500",                "2026-10-21 19:00:00 +0200", 0},
        {"noon",                            "2026-10-20 12:00:00 +0200", 0},
        {"noon 2 hours ago",                "2026-10-20 10:00:00 +0200", 0},
        {"Oct 1 noon",                      "2026-10-01 12:00:00 +0200", 0},

        /*
         *  "never" alone
         */
        {"never",                           "1970-01-01 01:00:00 +0100", 0},
        {"never (the epoch)",               "1970-01-01 01:00:00 +0100", 0},
        {"now never Mon",                   "2026-10-21 11:00:00 +0200", 1},
        {"now never",                       "2026-10-21 11:00:00 +0200", 1},
        {"never Thu",                       "2026-10-21 11:00:00 +0200", 1},
        {"never 10:00",                     "2026-10-21 11:00:00 +0200", 1},
        {"yesterday never",                 "2026-10-21 11:00:00 +0200", 1},
        {"never UTC",                       "2026-10-21 11:00:00 +0200", 1},
        {"never never",                     "2026-10-21 11:00:00 +0200", 1},
        {"never pm",                        "2026-10-21 11:00:00 +0200", 1},

        /*
         *  No count back right after "at"
         */
        {"at -5 days",                      "2026-10-21 11:00:00 +0200", 1},
        {"at -1d",                          "2026-10-21 11:00:00 +0200", 1},
        {"at -1w ago",                      "2026-10-21 11:00:00 +0200", 1},
        {"at last week",                    "2026-10-21 11:00:00 +0200", 1},
        {"at last friday",                  "2026-10-21 11:00:00 +0200", 1},
        {"at the last friday",              "2026-10-21 11:00:00 +0200", 1},
        {"at (x) -1d",                      "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at -2 days",                "2026-10-21 11:00:00 +0200", 1},
        {"-1d at 10:00",                    "2026-10-20 10:00:00 +0200", 0},
        {"last friday at 10:00",            "2026-10-16 10:00:00 +0200", 0},
        {"Oct 1 -2 days",                   "2026-09-29 11:00:00 +0200", 0},
        {"last pm",                         "2026-10-21 11:00:00 +0200", 1},
        {"last am",                         "2026-10-21 11:00:00 +0200", 1},
        {"at last pm",                      "2026-10-21 11:00:00 +0200", 1},

        /*
         *  am/pm: no half of the day of an hour with an 'h', nor of an
         *  hour past 12
         */
        {"at 15h am",                       "2026-10-21 11:00:00 +0200", 1},
        {"at 15h pm",                       "2026-10-21 11:00:00 +0200", 1},
        {"at 12h am",                       "2026-10-21 11:00:00 +0200", 1},
        {"at 9h (CEST) pm",                 "2026-10-21 11:00:00 +0200", 1},
        {"pm at 9h",                        "2026-10-21 11:00:00 +0200", 1},
        {"at 15h",                          "2026-10-21 15:00:00 +0200", 0},
        {"15:00 am",                        "2026-10-21 11:00:00 +0200", 1},
        {"15:00 pm",                        "2026-10-21 11:00:00 +0200", 1},
        {"13 pm",                           "2026-10-21 11:00:00 +0200", 1},
        {"tea pm",                          "2026-10-21 11:00:00 +0200", 1},
        {"24:00 am",                        "2026-10-21 11:00:00 +0200", 1},
        {"12:30 am",                        "2026-10-21 00:30:00 +0200", 0},
        {"12 pm",                           "2026-10-21 12:00:00 +0200", 0},
        {"0:30 am",                         "2026-10-21 00:30:00 +0200", 0},
        {"noon pm",                         "2026-10-21 11:00:00 +0200", 1},
        {"noon am",                         "2026-10-21 11:00:00 +0200", 1},
        {"midnight pm",                     "2026-10-21 11:00:00 +0200", 1},
        {"0:30 pm",                         "2026-10-21 11:00:00 +0200", 1},
        {"at 3 pm",                         "2026-10-21 15:00:00 +0200", 0},

        /*
         *  A year and its month: the day of today
         */
        {"at 2026.10",                      "2026-10-21 11:00:00 +0200", 0},
        {"2026.10",                         "2026-10-21 11:00:00 +0200", 0},
        {"at 2026-10",                      "2026-10-21 11:00:00 +0200", 0},
        {"2026/10",                         "2026-10-21 11:00:00 +0200", 0},
        {"at 1999.12",                      "1999-12-21 10:00:00 +0100", 0},
        {"at 2026.10 10:00",                "2026-10-21 10:00:00 +0200", 0},
        {"at 2026.13",                      "2026-10-21 11:00:00 +0200", 1},
        {"at 10.2026",                      "2026-10-21 11:00:00 +0200", 1},
        {"10.2026",                         "2026-10-21 11:00:00 +0200", 1},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));

    /*
     *  What follows "last" is looked at before it is read: a long text
     *  of comments between them is still read in time
     */
    const int n = 20000;
    size_t size = 4 + (size_t)n*3 + 5 + 1;
    char *text = gbmem_malloc(size);
    if(!text) {
        printf("FAIL no memory for the long text\n");
        global_result += -1;
        return;
    }
    char *p = text;
    memcpy(p, "last", 4);
    p += 4;
    for(int i = 0; i < n; i++) {
        memcpy(p, "(x)", 3);
        p += 3;
    }
    memcpy(p, " week", 5);
    p += 5;
    *p = 0;

    int error = 0;
    uint64_t timer = start_msectimer(500);
    timestamp_t t = approxidate_careful(text, &error);
    BOOL too_slow = test_msectimer(timer);
    char got[64];
    local_text(t, got, sizeof(got));
    if(error != 0 || too_slow || strcmp(got, "2026-10-14 11:00:00 +0200") != 0) {
        printf("FAIL approxidate of \"last\", %d comments, \"week\": %s error %d (expected "
            "2026-10-14 11:00:00 +0200 error 0), %s\n",
            n, got, error, too_slow? "500 ms passed" : "in time");
        global_result += -1;
    } else {
        printf("ok   approxidate of \"last\", %d comments, \"week\" -> %s, in time\n", n, got);
    }
    gbmem_free(text);
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
    test_forms_of_the_port();
    test_more_forms_of_the_port();
    test_offset_in_a_comment();
    test_weekday_after_a_date();
    test_blanks_and_years_before_weekdays();
    test_comments_weekdays_and_at();
    test_at_zero_counts_and_ago();
    test_long_hostile_text();
    test_ago_at_counts_and_weekdays();
    test_weekdays_on_a_change_of_hour();
    test_noon_never_at_and_am_pm();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
