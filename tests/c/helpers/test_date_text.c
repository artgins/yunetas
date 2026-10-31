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
 *          21. a local time that a change of hour repeats is the first
 *              one, and one that it skips is read with the offset before
 *              the change, whatever text was read before it (in Madrid
 *              "2026-10-25T02:30:00" was 02:30 +0100 after a text of the
 *              26th, and 02:30 +0200 after one of 01:30 that day).
 *          22. am/pm needs an hour ("pm" was 23:00 at 11:00); after "at" a
 *              time of day or a date ("at yesterday" was read); "never"
 *              with a zone is no date ("never (+0200)" was the epoch); "12
 *              noon" is noon ("12 noon" was the 12th); an offset after a
 *              date or a day is a zone ("yesterday +0000" was refused).
 *          23. a sign glued to the digits of a date, with only a day
 *              before it, is no zone ("2026-10-01-10:00" was Oct 2 at the
 *              time of now); right after a clock time ("T10:00-05:00"), or
 *              after a clock time or epoch seconds said before it
 *              ("10:00 2026-10-01-0500", "@1790000000-0500",
 *              "1790000000-0500", which was refused), it is, and a minus
 *              sign, a count and its unit is a duration there ("10:00
 *              2026-10-01-1d", "10:00 -1d" were refused); with
 *              no clock time an offset outside -12:00..+14:00 is no date
 *              ("now-2026" was read in a zone of -20:26), in a comment too
 *              ("now (+1500)"), while after epoch seconds any offset is
 *              read; a count right after a month name with no day is no
 *              date ("Jan 1 pm" was January 21 at 13:00, the day of
 *              today); a clock time with a zone and no date is of the day
 *              of now in that zone.
 *          24. a day that the zone skips whole is no day counted back, and
 *              the last noon that passed is the one before it (in Apia
 *              and Fakaofo on 2011-12-31, "noon" and "yesterday noon" were
 *              that day's noon, ahead of now); a count of hours, minutes or
 *              seconds counts no day ("noon -2 hours" is the 29th at
 *              10:00); a TZ changed with no tzset() is seen; and the limit
 *              of a zone that changes its offset twice in less than 48
 *              hours.
 *          25. the reader calls tzset() before the first look of a text
 *              at the local zone, TZ set or unset, and a text that says its
 *              zone or counts back from now costs none; a TZ set or unset
 *              with no tzset() is seen, and so is one put back with no
 *              tzset() after other code loaded another zone in libc (a
 *              line of log, a mktime()), also one with the names and
 *              offsets of the local zone and other rules; a month or a
 *              weekday back moves the date in the frame of now.
 *          26. gmtime2timezone() gives the offset at the time it converts
 *              (it gave the offset of now), keeps the TZ of the process
 *              when it cannot copy it (a failed copy unset TZ) and loads it
 *              again; a zone libc does not know is UTC with an ERROR (it
 *              was UTC in silence), a time no calendar holds one ERROR.
 *          27. get_hours_range() and the rest give the bounds of the zone
 *              asked, or of UTC (they were built in the zone of the
 *              process, with tm_isdst 0); an hour is one of the zone, also
 *              across a change of 30 minutes, at 02:45, or of 28 seconds;
 *              negative months, several years; hours of UTC take one step,
 *              an end no calendar holds is the range 0 with an ERROR (the
 *              year of 2147483647 years wrapped, some two billion years
 *              back), and a range back past the year 1 is proleptic.
 *          28. a run of 12 digits is no date ("202610011000" was the year
 *              8390), and after a clock time a minus sign and four digits
 *              is its offset ("10:00 -0500 hours" is no date).
 *          29. the same after anything else an offset can follow: noon, a
 *              day, a date ("noon -0500 hours", "yesterday -0500 hours",
 *              "2026-10-01 -0500 hours" were 500 hours back).
 *          30. the zones libc knows: a file under TZDIR or a path is a zone
 *              when it is a TZif file (zone.tab, /etc/passwd were UTC in
 *              silence), a POSIX TZ is one when all of it can be read
 *              ("ABC0DEF,M13.1.0" was UTC in silence, "ABC25" a day), and
 *              with no tzdata UTC and GMT are no error; a FIFO in place of
 *              a zone file does not block the process.
 *          31. a time no calendar holds: gmtime2timezone() leaves *ltm
 *              zeroed (it was written by halves), t2timestamp() and
 *              formatdate() give an empty text and one ERROR (a NULL went
 *              to strftime()).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <yunetas.h>

#define APP     "test_date_text"

#define NOW     1792573200  // Wed 2026-10-21 09:00:00 UTC, 11:00:00 CEST

#define ZONES_DIR   "/tmp/test_date_text_zones"

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
 *  tzset(), counted: -Wl,--wrap=tzset (see CMakeLists.txt)
 ***************************************************************************/
PRIVATE int s_tzset_calls = 0;

void __real_tzset(void);
void __wrap_tzset(void);

void __wrap_tzset(void)
{
    s_tzset_calls++;
    __real_tzset();
}

/***************************************************************************
 *  localtime_r() and gmtime_r(), counted: the cost of a text and of a range
 *  (see CMakeLists.txt)
 ***************************************************************************/
PRIVATE unsigned long long s_localtime_r_calls = 0;
PRIVATE unsigned long long s_gmtime_r_calls = 0;

struct tm *__real_localtime_r(const time_t *t, struct tm *tm);
struct tm *__wrap_localtime_r(const time_t *t, struct tm *tm);
struct tm *__real_gmtime_r(const time_t *t, struct tm *tm);
struct tm *__wrap_gmtime_r(const time_t *t, struct tm *tm);

struct tm *__wrap_localtime_r(const time_t *t, struct tm *tm)
{
    s_localtime_r_calls++;
    return __real_localtime_r(t, tm);
}

struct tm *__wrap_gmtime_r(const time_t *t, struct tm *tm)
{
    s_gmtime_r_calls++;
    return __real_gmtime_r(t, tm);
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
        {"10:00 -1",                    "2026-10-21 11:00:00 +0200", 1},
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
        {"10:00 -1d",                   "2026-10-20 10:00:00 +0200", 0},
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
    check_number("approxidate '@253402300799'", approxidate_careful("@253402300799", &error), 253402300799ULL);
    check_number("approxidate '@253402300799' error", (unsigned long long)error, 0);
    approxidate_careful("253402300799", &error);
    check_number("approxidate '253402300799' (12 digits, see 28) error", (unsigned long long)error, 1);
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
 *  21. A local time that a change of hour repeats is the first one, with
 *  the offset in force before the change, and one that a change of hour
 *  skips is read with that offset too (the time that many minutes after
 *  the change), whatever text was read before it: mktime() resolved the
 *  repeated hour with what its last call had cached, so in Madrid
 *  "2026-10-25T02:30:00" was 02:30 +0200 after a text of 01:30 that day
 *  and 02:30 +0100 after one of the 26th. A zone in the text says which.
 *  In Madrid, Los Angeles and Lord Howe (a change of 30 minutes).
 ***************************************************************************/
PRIVATE void test_repeated_and_skipped_hours(void)
{
    static const approxidate_case_t madrid[] = {
        /*
         *  Autumn: 02:00 to 02:59:59 on 2026-10-25 come twice
         */
        {"2026-10-25 01:30",                "2026-10-25 01:30:00 +0200", 0},
        {"2026-10-25T02:30:00",             "2026-10-25 02:30:00 +0200", 0},
        {"2026-10-26 10:00",                "2026-10-26 10:00:00 +0100", 0},
        {"2026-10-25T02:30:00",             "2026-10-25 02:30:00 +0200", 0},
        {"2026-10-25 02:00",                "2026-10-25 02:00:00 +0200", 0},
        {"2026-10-25 02:59:59",             "2026-10-25 02:59:59 +0200", 0},
        {"2026-10-25 03:00",                "2026-10-25 03:00:00 +0100", 0},
        {"2026-10-25 01:59:59",             "2026-10-25 01:59:59 +0200", 0},
        {"2026-10-25 02:30 +0100",          "2026-10-25 02:30:00 +0100", 0},
        {"2026-10-25 02:30 CET",            "2026-10-25 02:30:00 +0100", 0},
        {"2026-10-25 02:30 +0200",          "2026-10-25 02:30:00 +0200", 0},

        /*
         *  Spring: 02:00 to 02:59:59 on 2026-03-29 never come
         */
        {"2026-03-28 10:00",                "2026-03-28 10:00:00 +0100", 0},
        {"2026-03-29 02:30",                "2026-03-29 03:30:00 +0200", 0},
        {"2026-03-30 10:00",                "2026-03-30 10:00:00 +0200", 0},
        {"2026-03-29 02:30",                "2026-03-29 03:30:00 +0200", 0},
        {"2026-03-29 02:00",                "2026-03-29 03:00:00 +0200", 0},
        {"2026-03-29 01:59:59",             "2026-03-29 01:59:59 +0100", 0},
        {"2026-03-29 03:00",                "2026-03-29 03:00:00 +0200", 0},
        {"2026-03-29 02:30 +0200",          "2026-03-29 01:30:00 +0100", 0},
    };
    check_approxidate(madrid, sizeof(madrid)/sizeof(madrid[0]));

    /*
     *  A clock time of today, of yesterday: Sun 2026-10-25 13:34:56 CET
     */
    s_fake_now = 1792931696;
    static const approxidate_case_t madrid_today[] = {
        {"2026-10-26 10:00",                "2026-10-26 10:00:00 +0100", 0},
        {"2am",                             "2026-10-25 02:00:00 +0200", 0},
        {"2:30 am",                         "2026-10-25 02:30:00 +0200", 0},
        {"2026-10-24 10:00",                "2026-10-24 10:00:00 +0200", 0},
        {"2am",                             "2026-10-25 02:00:00 +0200", 0},
        {"02:30 CET",                       "2026-10-25 02:30:00 +0100", 0},
    };
    check_approxidate(madrid_today, sizeof(madrid_today)/sizeof(madrid_today[0]));

    /*
     *  Mon 2026-10-26 10:00 CET
     */
    s_fake_now = 1793005200;
    static const approxidate_case_t madrid_yesterday[] = {
        {"yesterday 02:30",                 "2026-10-25 02:30:00 +0200", 0},
        {"2026-10-25 01:30",                "2026-10-25 01:30:00 +0200", 0},
        {"yesterday 02:30",                 "2026-10-25 02:30:00 +0200", 0},
    };
    check_approxidate(madrid_yesterday, sizeof(madrid_yesterday)/sizeof(madrid_yesterday[0]));
    s_fake_now = NOW;

    setenv("TZ", "America/Los_Angeles", 1);
    tzset();
    static const approxidate_case_t los_angeles[] = {
        {"2026-11-02 10:00",                "2026-11-02 10:00:00 -0800", 0},
        {"2026-11-01 01:30",                "2026-11-01 01:30:00 -0700", 0},
        {"2026-10-31 10:00",                "2026-10-31 10:00:00 -0700", 0},
        {"2026-11-01 01:30",                "2026-11-01 01:30:00 -0700", 0},
        {"2026-11-01 01:30 PST",            "2026-11-01 01:30:00 -0800", 0},
        {"2026-03-09 10:00",                "2026-03-09 10:00:00 -0700", 0},
        {"2026-03-08 02:30",                "2026-03-08 03:30:00 -0700", 0},
        {"2026-03-07 10:00",                "2026-03-07 10:00:00 -0800", 0},
        {"2026-03-08 02:30",                "2026-03-08 03:30:00 -0700", 0},
    };
    check_approxidate(los_angeles, sizeof(los_angeles)/sizeof(los_angeles[0]));

    setenv("TZ", "Australia/Lord_Howe", 1);
    tzset();
    static const approxidate_case_t lord_howe[] = {
        /*
         *  01:30 to 01:59:59 on 2026-04-05 come twice; 02:00 to
         *  02:29:59 on 2026-10-04 never come
         */
        {"2026-04-06 10:00",                "2026-04-06 10:00:00 +1030", 0},
        {"2026-04-05 01:45",                "2026-04-05 01:45:00 +1100", 0},
        {"2026-04-04 10:00",                "2026-04-04 10:00:00 +1100", 0},
        {"2026-04-05 01:45",                "2026-04-05 01:45:00 +1100", 0},
        {"2026-04-05 02:00",                "2026-04-05 02:00:00 +1030", 0},
        {"2026-10-05 10:00",                "2026-10-05 10:00:00 +1100", 0},
        {"2026-10-04 02:15",                "2026-10-04 02:45:00 +1100", 0},
        {"2026-10-03 10:00",                "2026-10-03 10:00:00 +1030", 0},
        {"2026-10-04 02:15",                "2026-10-04 02:45:00 +1100", 0},
    };
    check_approxidate(lord_howe, sizeof(lord_howe)/sizeof(lord_howe[0]));

    setenv("TZ", "Europe/Madrid", 1);
    tzset();
}

/***************************************************************************
 *  A text of `prefix`, `n` times `piece` and `suffix`, read in time: the
 *  local time expected and the error flag
 ***************************************************************************/
PRIVATE void check_long_text(const char *prefix, const char *piece, int n, const char *suffix,
    const char *expected, int expected_error)
{
    size_t size = strlen(prefix) + (size_t)n*strlen(piece) + strlen(suffix) + 1;
    char *text = gbmem_malloc(size);
    if(!text) {
        printf("FAIL no memory for the long text\n");
        global_result += -1;
        return;
    }
    char *p = text;
    memcpy(p, prefix, strlen(prefix));
    p += strlen(prefix);
    for(int i = 0; i < n; i++) {
        memcpy(p, piece, strlen(piece));
        p += strlen(piece);
    }
    memcpy(p, suffix, strlen(suffix));
    p += strlen(suffix);
    *p = 0;

    int error = 0;
    uint64_t timer = start_msectimer(500);
    timestamp_t t = approxidate_careful(text, &error);
    BOOL too_slow = test_msectimer(timer);
    char got[64];
    local_text(t, got, sizeof(got));
    if(error != expected_error || too_slow || strcmp(got, expected) != 0) {
        printf("FAIL approxidate of \"%s\", %d \"%s\", \"%s\": %s error %d (expected %s error %d), %s\n",
            prefix, n, piece, suffix, got, error, expected, expected_error,
            too_slow? "500 ms passed" : "in time");
        global_result += -1;
    } else {
        printf("ok   approxidate of \"%s\", %d \"%s\", \"%s\" -> %s%s, in time\n",
            prefix, n, piece, suffix, got, error? " (error)" : "");
    }
    gbmem_free(text);
}

/***************************************************************************
 *  22. am/pm needs an hour ("pm" and "today pm" at 11:00 were today at
 *  23:00, "pm 3" the 3rd at 23:00). Right after "at" comes a time of day
 *  or a date: "at yesterday", "at now", "at today", "at never" were read.
 *  "never" says no zone either, in a comment neither ("never (+0200)" was
 *  the epoch). "12 noon" and "12 midnight" are noon and midnight when no
 *  date is said before the 12 ("12 noon" was the 12th at noon,
 *  "yesterday 12 noon" the 11th). An offset is read after a date or a day
 *  as a zone name is ("yesterday UTC" was read, "yesterday +0000"
 *  refused): the date is of that zone.
 ***************************************************************************/
PRIVATE void test_am_pm_at_never_noon_and_zones(void)
{
    static const approxidate_case_t cases[] = {
        /*
         *  am/pm needs an hour: Wed 11:00
         */
        {"pm",                              "2026-10-21 11:00:00 +0200", 1},
        {"am",                              "2026-10-21 11:00:00 +0200", 1},
        {"PM",                              "2026-10-21 11:00:00 +0200", 1},
        {"today pm",                        "2026-10-21 11:00:00 +0200", 1},
        {"now am",                          "2026-10-21 11:00:00 +0200", 1},
        {"pm 3",                            "2026-10-21 11:00:00 +0200", 1},
        {"yesterday am",                    "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1st pm",                      "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01 am",                   "2026-10-21 11:00:00 +0200", 1},
        {"2 hours ago pm",                  "2026-10-21 11:00:00 +0200", 1},
        {"pm (CEST)",                       "2026-10-21 11:00:00 +0200", 1},
        {"pm 3:00",                         "2026-10-21 15:00:00 +0200", 0},
        {"3 pm",                            "2026-10-21 15:00:00 +0200", 0},
        {"yesterday 3 pm",                  "2026-10-20 15:00:00 +0200", 0},

        /*
         *  A count of days moves the day, one of hours the time: Wed 11:00
         */
        {"1 day ago",                       "2026-10-20 11:00:00 +0200", 0},
        {"24 hours ago",                    "2026-10-20 11:00:00 +0200", 0},
        {"noon 1 day ago",                  "2026-10-20 12:00:00 +0200", 0},
        {"noon 24 hours ago",               "2026-10-19 12:00:00 +0200", 0},
        {"tea 1 day ago",                   "2026-10-20 17:00:00 +0200", 0},
        {"tea 24 hours ago",                "2026-10-19 17:00:00 +0200", 0},

        /*
         *  After "at" a time of day or a date
         */
        {"at yesterday",                    "2026-10-21 11:00:00 +0200", 1},
        {"at now",                          "2026-10-21 11:00:00 +0200", 1},
        {"at today",                        "2026-10-21 11:00:00 +0200", 1},
        {"at never",                        "2026-10-21 11:00:00 +0200", 1},
        {"at the yesterday",                "2026-10-21 11:00:00 +0200", 1},
        {"at (x) now",                      "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 at yesterday",              "2026-10-21 11:00:00 +0200", 1},
        {"yesterday at 10:00",              "2026-10-20 10:00:00 +0200", 0},
        {"now at 10:00",                    "2026-10-21 10:00:00 +0200", 0},
        {"at noon",                         "2026-10-20 12:00:00 +0200", 0},
        {"at 10:00 yesterday",              "2026-10-20 10:00:00 +0200", 0},

        /*
         *  "never" with no zone
         */
        {"never (+0200)",                   "2026-10-21 11:00:00 +0200", 1},
        {"never (UTC+02:00)",               "2026-10-21 11:00:00 +0200", 1},
        {"never (CEST)",                    "2026-10-21 11:00:00 +0200", 1},
        {"(GMT) never",                     "2026-10-21 11:00:00 +0200", 1},
        {"never +0000",                     "2026-10-21 11:00:00 +0200", 1},
        {"never (the epoch)",               "1970-01-01 01:00:00 +0100", 0},

        /*
         *  "12 noon", "12 midnight"
         */
        {"12 noon",                         "2026-10-20 12:00:00 +0200", 0},
        {"12 midnight",                     "2026-10-21 00:00:00 +0200", 0},
        {"at 12 noon",                      "2026-10-20 12:00:00 +0200", 0},
        {"today 12 noon",                   "2026-10-21 12:00:00 +0200", 0},
        {"12 noon today",                   "2026-10-21 12:00:00 +0200", 0},
        {"yesterday 12 noon",               "2026-10-20 12:00:00 +0200", 0},
        {"12 (x) noon",                     "2026-10-20 12:00:00 +0200", 0},
        {"12 noon Oct 1",                   "2026-10-01 12:00:00 +0200", 0},
        {"Oct 12 noon",                     "2026-10-12 12:00:00 +0200", 0},
        {"12 Oct noon",                     "2026-10-12 12:00:00 +0200", 0},
        {"12 tea",                          "2026-10-12 17:00:00 +0200", 0},
        {"5 noon",                          "2026-10-05 12:00:00 +0200", 0},
        {"12 noon 12 noon",                 "2026-10-21 11:00:00 +0200", 1},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));

    /*
     *  An offset after a date or a day: Wed 2026-10-21 01:00 CEST, Tue
     *  2026-10-20 23:00 UTC, so the date of now in UTC is the day before
     */
    s_fake_now = 1792537200;
    static const approxidate_case_t zones[] = {
        {"2026-10-01",                      "2026-10-01 01:00:00 +0200", 0},
        {"2026-10-01 UTC",                  "2026-10-02 01:00:00 +0200", 0},
        {"2026-10-01 +0000",                "2026-10-02 01:00:00 +0200", 0},
        {"2026-10-01 +00:00",               "2026-10-02 01:00:00 +0200", 0},
        {"2026-10-01 -05",                  "2026-10-02 01:00:00 +0200", 0},
        {"Oct 1 +0000",                     "2026-10-02 01:00:00 +0200", 0},
        {"yesterday +0000",                 "2026-10-20 01:00:00 +0200", 0},
        {"yesterday UTC",                   "2026-10-20 01:00:00 +0200", 0},
        {"yesterday -0500",                 "2026-10-20 01:00:00 +0200", 0},
        {"now +0000",                       "2026-10-21 01:00:00 +0200", 0},
        {"2 hours ago -0500",               "2026-10-20 23:00:00 +0200", 0},
        {"now +0000 Tue",                   "2026-10-21 01:00:00 +0200", 0},
        {"now UTC Tue",                     "2026-10-21 01:00:00 +0200", 0},
        {"now Tue",                         "2026-10-21 01:00:00 +0200", 1},
        {"Oct 1 -2 days",                   "2026-09-29 01:00:00 +0200", 0},
        {"Oct 1 -5",                        "2026-10-21 01:00:00 +0200", 1},
        {"+0200",                           "2026-10-21 01:00:00 +0200", 1},
        {"-0500",                           "2026-10-21 01:00:00 +0200", 1},
        {"+0200 yesterday",                 "2026-10-21 01:00:00 +0200", 1},
        {"yesterday +0000 UTC",             "2026-10-21 01:00:00 +0200", 1},
        {"yesterday +0000 +0000",           "2026-10-21 01:00:00 +0200", 1},
    };
    check_approxidate(zones, sizeof(zones)/sizeof(zones[0]));
    s_fake_now = NOW;

    /*
     *  What is looked at after a 12 and a minus sign is passed in time
     */
    check_long_text("12", "(x)", 20000, " noon", "2026-10-20 12:00:00 +0200", 0);
    check_long_text("yesterday -1", "(x)", 20000, " d", "2026-10-19 11:00:00 +0200", 0);
    check_long_text("yesterday", " -1s", 20000, "", "2026-10-20 05:26:40 +0200", 0);
    check_long_text("Oct 1", " -0500", 20000, "", "2026-10-21 11:00:00 +0200", 1);
}

/***************************************************************************
 *  23. A sign glued to the digits of a date with only a day before it is
 *  no zone: "2026-10-01-10:00" (Wed 21 at 11:00) was Fri 2 at 11:00, the
 *  date of October 1 at -10:00. An offset may touch digits right after a
 *  clock time, as ISO 8601 writes it ("2026-10-01T10:00-05:00"), and after
 *  a clock time (noon) or epoch seconds said before it, where no clock
 *  time can follow ("10:00 2026-10-01-0500", "@1790000000-0500",
 *  "1790000000-0500", refused while "1790000000+0200" was read). A minus
 *  sign, a count and its unit is a duration there too, as without the
 *  sign ("10:00 2026-10-01-1d" and "10:00 -1d" were refused). With no
 *  clock time an offset moves the date, and one outside -12:00..+14:00,
 *  the zones there are, is no date ("now-2026" was now, in a zone of
 *  -20:26), in a comment too ("now (+1500)"); after epoch seconds it moves
 *  nothing, only says the offset to show, and any offset is read. A count
 *  right after a month name, when the text says no day, is no date ("Jan 1
 *  pm" was January 21 at 13:00, the day of today). A clock time with a
 *  zone and no date is of the day of now in that zone.
 ***************************************************************************/
PRIVATE void test_glued_signs_far_offsets_and_counts_after_a_month(void)
{
    static const approxidate_case_t cases[] = {
        /*
         *  A sign glued to a date: Wed 2026-10-21 11:00 CEST
         */
        {"2026-10-01-10:00",                "2026-10-21 11:00:00 +0200", 1},
        {"10/01/2026-10:00",                "2026-10-21 11:00:00 +0200", 1},
        {"1.10.2026-23",                    "2026-10-21 11:00:00 +0200", 1},
        {"2026.10-2026.10",                 "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01-05",                   "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01+0200",                 "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 2026+0000",                 "2026-10-21 11:00:00 +0200", 1},
        {"@1790000000 -0500",               "2026-09-21 16:13:20 +0200", 0},
        {"2026-10-01 -10:00",               "2026-10-02 11:00:00 +0200", 0},
        {"2026-10-01-1d",                   "2026-09-30 11:00:00 +0200", 0},
        {"2026-10-01T10:00-05:00",          "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01T10:00:00.5-05:00",     "2026-10-01 17:00:00 +0200", 0},
        {"2026-10-01 10:00:00+05:00",       "2026-10-01 07:00:00 +0200", 0},
        {"20261001T100000-0500",            "2026-10-01 17:00:00 +0200", 0},
        {"01/Oct/2026:10:00:00+0200",       "2026-10-01 10:00:00 +0200", 0},
        {"10:00-0500",                      "2026-10-21 17:00:00 +0200", 0},
        {"now-0500",                        "2026-10-21 11:00:00 +0200", 0},

        /*
         *  After a clock time (noon) or epoch seconds said before it, a
         *  sign glued to a date or to the seconds is a zone: no clock time
         *  can follow
         */
        {"10:00 2026-10-01-0500",           "2026-10-01 17:00:00 +0200", 0},
        {"10:00 1.10.2026-23",              "2026-10-02 11:00:00 +0200", 0},
        {"noon 2026-10-01-0500",            "2026-10-01 19:00:00 +0200", 0},
        {"3pm 10/01/2026-10:00",            "2026-10-02 03:00:00 +0200", 0},
        {"@1790000000-0500",                "2026-09-21 16:13:20 +0200", 0},
        {"@1790000000+1500",                "2026-09-21 16:13:20 +0200", 0},
        {"1790000000+0200",                 "2026-09-21 16:13:20 +0200", 0},

        /*
         *  Epoch seconds start no date of numbers: their '-' is a sign, as
         *  their '+' (it was refused, read as the '-' of a date)
         */
        {"1790000000-0500",                 "2026-09-21 16:13:20 +0200", 0},
        {"1790000000-05",                   "2026-09-21 16:13:20 +0200", 0},
        {"1790000000-05:00",                "2026-09-21 16:13:20 +0200", 0},
        {"1790000000/05",                   "2026-10-21 11:00:00 +0200", 1},
        {"1790000000-1d",                   "2026-10-21 11:00:00 +0200", 1},

        /*
         *  A minus sign, a count and its unit is a duration after a clock
         *  time too, as without the sign (it was refused, the -1 read as a
         *  zone); with no unit it is a zone there
         */
        {"10:00 2026-10-01-1d",             "2026-09-30 10:00:00 +0200", 0},
        {"10:00 2026-10-01 1d",             "2026-09-30 10:00:00 +0200", 0},
        {"2026-10-01 10:00 -1d",            "2026-09-30 10:00:00 +0200", 0},
        {"10:00 -1d",                       "2026-10-20 10:00:00 +0200", 0},
        {"10:00-1d",                        "2026-10-20 10:00:00 +0200", 0},
        {"10:00 -1 day",                    "2026-10-20 10:00:00 +0200", 0},
        {"10:00 -05 hours",                 "2026-10-21 05:00:00 +0200", 0},
        {"10:00 -05",                       "2026-10-21 17:00:00 +0200", 0},
        {"10:00 -5",                        "2026-10-21 11:00:00 +0200", 1},

        /*
         *  With no clock time, an offset of a zone there is
         */
        {"now-2026",                        "2026-10-21 11:00:00 +0200", 1},
        {"now +1500",                       "2026-10-21 11:00:00 +0200", 1},
        {"yesterday -2300",                 "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01 +1401",                "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01 -1201",                "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01 GMT+1500",             "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01 (UTC+15:00)",          "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01 (+1500)",              "2026-10-21 11:00:00 +0200", 1},
        {"now (+1500)",                     "2026-10-21 11:00:00 +0200", 1},
        {"yesterday (UTC-13:00)",           "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01 (UTC+14:00)",          "2026-10-01 11:00:00 +0200", 0},
        {"2026-10-01 +1400",                "2026-10-01 11:00:00 +0200", 0},
        {"2026-10-01 -1200",                "2026-10-02 11:00:00 +0200", 0},
        {"noon -2300",                      "2026-10-20 13:00:00 +0200", 0},
        {"2026-10-01 10:00 +2300",          "2026-09-30 13:00:00 +0200", 0},
        {"@1761000000 +1500",               "2025-10-21 00:40:00 +0200", 0},
        {"@1761000000 -2359",               "2025-10-21 00:40:00 +0200", 0},

        /*
         *  A count right after a month name, and no day
         */
        {"Jan 1 pm",                        "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 pm",                        "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 am",                        "2026-10-21 11:00:00 +0200", 1},
        {"October 1 pm",                    "2026-10-21 11:00:00 +0200", 1},
        {"Oct, 1 pm",                       "2026-10-21 11:00:00 +0200", 1},
        {"Oct (x) 1 pm",                    "2026-10-21 11:00:00 +0200", 1},
        {"Oct 3pm",                         "2026-10-21 11:00:00 +0200", 1},
        {"2026 Oct 1 pm",                   "2026-10-21 11:00:00 +0200", 1},
        {"Oct 5 days ago",                  "2026-10-21 11:00:00 +0200", 1},
        {"Oct five days ago",               "2026-10-21 11:00:00 +0200", 1},
        {"Oct 2 fridays",                   "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 3 pm",                      "2026-10-01 15:00:00 +0200", 0},
        {"Jan 1 3 pm",                      "2026-01-01 15:00:00 +0100", 0},
        {"1 Oct 3 pm",                      "2026-10-01 15:00:00 +0200", 0},
        {"Oct 1st 3 pm",                    "2026-10-01 15:00:00 +0200", 0},
        {"the 5th of October 3 pm",         "2026-10-05 15:00:00 +0200", 0},
        {"Oct 1 2 fridays",                 "2026-09-18 11:00:00 +0200", 0},
        {"Oct 5 friday",                    "2026-10-05 11:00:00 +0200", 0},
        {"Oct 2026 1 pm",                   "2026-10-21 13:00:00 +0200", 0},
        {"3 pm Oct",                        "2026-10-21 15:00:00 +0200", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));

    /*
     *  A clock time with a zone and no date is of the day of now in that
     *  zone: Sun 2026-10-25 00:30 CEST is Sat 2026-10-24 23:30 CET
     */
    s_fake_now = 1792881000;
    static const approxidate_case_t day_of_the_zone[] = {
        {"02:30 CET",                       "2026-10-24 03:30:00 +0200", 0},
        {"02:30 +0100",                     "2026-10-24 03:30:00 +0200", 0},
        {"02:30",                           "2026-10-25 02:30:00 +0200", 0},
        {"today 02:30 CET",                 "2026-10-24 03:30:00 +0200", 0},
    };
    check_approxidate(day_of_the_zone, sizeof(day_of_the_zone)/sizeof(day_of_the_zone[0]));
    s_fake_now = NOW;

    /*
     *  What is looked at after a month name is passed in time
     */
    check_long_text("Oct", "(x)", 20000, " 1 pm", "2026-10-21 11:00:00 +0200", 1);
    check_long_text("Oct 1", "(x)", 20000, " 3 pm", "2026-10-01 15:00:00 +0200", 0);
    check_long_text("Oct 1 10:00", "(x)", 20000, "-0500", "2026-10-01 17:00:00 +0200", 0);
}

/***************************************************************************
 *  24. A day that a change of the zone skips whole is no day counted
 *  back from today, and the last noon (midnight, tea) that passed is the
 *  one of the day before it: in Apia 2011-12-30 was skipped (from
 *  2011-12-29 23:59:59 -1000 to 2011-12-31 00:00:00 +1400), and on the
 *  31st "noon", "yesterday noon" and "yesterday 12:30" were the 31st at
 *  12:00 and 12:30, ahead of now at 00:00. A count of hours, minutes or
 *  seconds counts no day: "noon -2 hours" is 2 hours before the last noon
 *  that passed, the 29th's. Fakaofo skipped the same day. A date said is
 *  read as any time a change skips ("2011-12-30 12:00" is 24 hours later).
 *
 *  A TZ changed with no tzset() is seen: the reader calls it (see 25).
 *
 *  The offsets before and after a local time are taken a day away from
 *  it, so the reading is exact for a zone that never changes its offset
 *  twice in less than 48 hours: tzdata has no such zone since 1970 (its
 *  shortest interval is 6.96 days). A POSIX TZ can have it: with a summer
 *  time of one day a time of that day is read with the offset of the days
 *  around it.
 ***************************************************************************/
PRIVATE void test_whole_day_skipped_tzset_and_close_changes(void)
{
    setenv("TZ", "Pacific/Apia", 1);
    tzset();

    /*
     *  Sat 2011-12-31 00:00 +1400, the first moment after the day skipped
     */
    s_fake_now = 1325239200;
    static const approxidate_case_t first_moment[] = {
        {"noon",                            "2011-12-29 12:00:00 -1000", 0},
        {"tea",                             "2011-12-29 17:00:00 -1000", 0},
        {"yesterday noon",                  "2011-12-31 00:00:00 +1400", 1},
        {"yesterday 12:30",                 "2011-12-31 00:00:00 +1400", 1},
        {"1 day ago 12:30",                 "2011-12-31 00:00:00 +1400", 1},
        {"last friday noon",                "2011-12-31 00:00:00 +1400", 1},
        {"2 days ago 12:30",                "2011-12-29 12:30:00 -1000", 0},
        {"midnight",                        "2011-12-31 00:00:00 +1400", 0},
        {"yesterday",                       "2011-12-29 00:00:00 -1000", 0},
        {"2011-12-30 12:00",                "2011-12-31 12:00:00 +1400", 0},
        {"2011-12-29 12:00",                "2011-12-29 12:00:00 -1000", 0},
        {"12:30",                           "2011-12-31 12:30:00 +1400", 0},

        /*
         *  A count of hours, minutes or seconds counts no day: it is taken
         *  back from the last noon (tea) that passed
         */
        {"noon -2 hours",                   "2011-12-29 10:00:00 -1000", 0},
        {"noon 2 hours ago",                "2011-12-29 10:00:00 -1000", 0},
        {"tea -30 minutes",                 "2011-12-29 16:30:00 -1000", 0},
        {"noon -1 second",                  "2011-12-29 11:59:59 -1000", 0},
        {"noon 24 hours ago",               "2011-12-28 12:00:00 -1000", 0},
        {"2011-12-30 noon -2 hours",        "2011-12-31 10:00:00 +1400", 0},
        {"noon -1 day",                     "2011-12-31 00:00:00 +1400", 1},
        {"noon 1 day ago",                  "2011-12-31 00:00:00 +1400", 1},
        {"yesterday noon -2 hours",         "2011-12-31 00:00:00 +1400", 1},
    };
    check_approxidate(first_moment, sizeof(first_moment)/sizeof(first_moment[0]));

    /*
     *  Sat 2011-12-31 13:00 +1400
     */
    s_fake_now = 1325286000;
    static const approxidate_case_t afternoon[] = {
        {"noon",                            "2011-12-31 12:00:00 +1400", 0},
        {"today noon",                      "2011-12-31 12:00:00 +1400", 0},
        {"yesterday noon",                  "2011-12-31 13:00:00 +1400", 1},
        {"yesterday 12:30",                 "2011-12-31 13:00:00 +1400", 1},
        {"2 days ago noon",                 "2011-12-29 12:00:00 -1000", 0},
        {"tea 24 hours ago",                "2011-12-28 17:00:00 -1000", 0},
    };
    check_approxidate(afternoon, sizeof(afternoon)/sizeof(afternoon[0]));

    /*
     *  Fakaofo skipped the same day, from -1100 to +1300: Sat 2011-12-31
     *  00:00 +1300
     */
    setenv("TZ", "Pacific/Fakaofo", 1);
    tzset();
    s_fake_now = 1325242800;
    static const approxidate_case_t fakaofo[] = {
        {"noon",                            "2011-12-29 12:00:00 -1100", 0},
        {"noon -2 hours",                   "2011-12-29 10:00:00 -1100", 0},
        {"noon 2 hours ago",                "2011-12-29 10:00:00 -1100", 0},
        {"tea -30 minutes",                 "2011-12-29 16:30:00 -1100", 0},
        {"noon -1 second",                  "2011-12-29 11:59:59 -1100", 0},
        {"noon -1 day",                     "2011-12-31 00:00:00 +1300", 1},
        {"yesterday noon",                  "2011-12-31 00:00:00 +1300", 1},
        {"yesterday",                       "2011-12-29 00:00:00 -1100", 0},
    };
    check_approxidate(fakaofo, sizeof(fakaofo)/sizeof(fakaofo[0]));
    s_fake_now = NOW;

    /*
     *  A TZ changed with no tzset()
     */
    setenv("TZ", "America/Los_Angeles", 1);
    static const approxidate_case_t no_tzset[] = {
        {"2026-10-01 10:00",                "2026-10-01 10:00:00 -0700", 0},
    };
    check_approxidate(no_tzset, sizeof(no_tzset)/sizeof(no_tzset[0]));

    /*
     *  Two changes 48 hours apart: exact. Summer time from 2027-03-01
     *  00:00 to 2027-03-03 01:00, local.
     */
    setenv("TZ", "AAA0BBB-1,J60/0,J62/1", 1);
    tzset();
    static const approxidate_case_t two_days[] = {
        {"2027-03-01 00:30",                "2027-03-01 01:30:00 +0100", 0},
        {"2027-03-01 12:00",                "2027-03-01 12:00:00 +0100", 0},
        {"2027-03-03 00:30",                "2027-03-03 00:30:00 +0100", 0},
        {"2027-03-03 01:30",                "2027-03-03 01:30:00 +0000", 0},
    };
    check_approxidate(two_days, sizeof(two_days)/sizeof(two_days[0]));

    /*
     *  Two changes 24 hours apart, the limit: summer time from 2027-03-01
     *  00:00 to 2027-03-02 00:00, local. The offsets a day away are those
     *  of winter, and a time of that day is read with them, an hour later.
     */
    setenv("TZ", "AAA0BBB-1,J60/0,J61/0", 1);
    tzset();
    static const approxidate_case_t one_day[] = {
        {"2027-03-01 12:00",                "2027-03-01 13:00:00 +0100", 0},
        {"2027-03-02 12:00",                "2027-03-02 12:00:00 +0000", 0},
    };
    check_approxidate(one_day, sizeof(one_day)/sizeof(one_day[0]));

    setenv("TZ", "Europe/Madrid", 1);
    tzset();
}

/***************************************************************************
 *  A text read now, as a number to compare
 ***************************************************************************/
PRIVATE unsigned long long read_now(const char *text)
{
    int error = 0;
    timestamp_t t = approxidate_careful(text, &error);
    return error? 0 : (unsigned long long)t;
}

/***************************************************************************
 *  A text of the local date of `t` at 12:00
 ***************************************************************************/
PRIVATE void noon_of_day(time_t t, char *text, size_t size)
{
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(text, size, "%04d-%02d-%02d 12:00", (tm.tm_year + 1900)%10000, (tm.tm_mon + 1)%100, tm.tm_mday%100);
}

/***************************************************************************
 *  25. The zone of a text is the TZ of now, whatever other code did before
 *  it. The reader calls tzset() before the first look of a text at the
 *  local zone, TZ set or unset: glibc compares TZ with the text of its last
 *  load, or with TZ unset looks at /etc/localtime, so a TZ set, unset or
 *  put back with no tzset() is seen after a localtime() of other code in
 *  another zone (a line of log: current_timestamp()), or a mktime(), which
 *  call tzset() inside libc. A snapshot of the zone libc holds (tzname[],
 *  timezone, daylight) could not tell such a zone from the local one when
 *  both have the same names and offsets (Africa/Cairo and Europe/Athens):
 *  with TZ unset a text was read with the other one's rules. A text that
 *  says its zone, or counts back from now, looks at no zone and costs no
 *  tzset().
 ***************************************************************************/
PRIVATE void test_tzset_only_when_tz_changes(void)
{
    char bf[90];
    struct tm tm;

    setenv("TZ", "America/Los_Angeles", 1);
    tzset();
    unsigned long long t_los_angeles = read_now("2026-07-01 10:00");
    s_tzset_calls = 0;
    for(int i = 0; i < 1000; i++) {
        read_now("2026-07-01 10:00");
    }
    check_number("tzset() calls for 1000 texts, TZ set: one per text", (unsigned long long)s_tzset_calls, 1000);

    /*
     *  The offset of now is asked of libc once per text, however many
     *  pieces of the text need it
     */
    s_localtime_r_calls = 0;
    read_now("Oct 1");
    check_number("localtime_r() calls for 'Oct 1', a date with no year: the offset of now once",
        s_localtime_r_calls, 1);

    static const char *zone_free_texts[] = {
        "2026-07-01T10:00:00Z", "Thu, 01 Oct 2026 10:00:00 +0000", "3 hours ago", "now", "@1790000000",
    };
    for(size_t i = 0; i < sizeof(zone_free_texts)/sizeof(zone_free_texts[0]); i++) {
        s_tzset_calls = 0;
        for(int j = 0; j < 1000; j++) {
            read_now(zone_free_texts[i]);
        }
        snprintf(bf, sizeof(bf), "tzset() calls for 1000 texts '%s', no zone looked at", zone_free_texts[i]);
        check_number(bf, (unsigned long long)s_tzset_calls, 0);
    }

    /*
     *  A month or a weekday back moves the date in the frame of now, the
     *  UTC offset of now: on 2026-03-30 at 23:30 UTC, 01:30 CEST of the
     *  31st, a month back is February 28 (the 31st has none), and on
     *  Wednesday 2026-10-21 at 23:30 UTC, 01:30 CEST of Thursday, the last
     *  Friday is October 16
     */
    setenv("TZ", "Europe/Madrid", 1);
    s_fake_now = 1774913400;
    static const approxidate_case_t frame_of_now_march[] = {
        {"1 month ago",                     "2026-02-28 00:30:00 +0100", 0},
        {"2 days ago",                      "2026-03-29 00:30:00 +0100", 0},
    };
    check_approxidate(frame_of_now_march, sizeof(frame_of_now_march)/sizeof(frame_of_now_march[0]));
    s_fake_now = 1792625400;
    static const approxidate_case_t frame_of_now_october[] = {
        {"last friday",                     "2026-10-16 01:30:00 +0200", 0},
        {"yesterday Wed",                   "2026-10-21 01:30:00 +0200", 0},
    };
    check_approxidate(frame_of_now_october, sizeof(frame_of_now_october)/sizeof(frame_of_now_october[0]));
    s_fake_now = NOW;

    /*
     *  TZ unset: a tzset() at every text too
     */
    unsetenv("TZ");
    s_tzset_calls = 0;
    unsigned long long t_unset = read_now("2026-07-01 10:00");
    for(int i = 0; i < 999; i++) {
        read_now("2026-07-01 10:00");
    }
    check_number("tzset() calls for 1000 texts, TZ unset: one per text", (unsigned long long)s_tzset_calls, 1000);
    tzset();
    unsigned long long t_after_tzset = read_now("2026-07-01 10:00");
    check_number("TZ unset with no tzset(): the time of /etc/localtime", t_unset, t_after_tzset);
    printf("     (Los Angeles %llu, /etc/localtime %llu)\n", t_los_angeles, t_after_tzset);

    /*
     *  Other code loads a zone with the names and offsets of the local one
     *  and other rules (a POSIX TZ whose summer time is one hour of December
     *  31), and unsets TZ with no tzset(): a day of local summer time is
     *  read with the local rules
     */
    unsetenv("TZ");
    tzset();
    if(!daylight) {
        printf("     (the local zone has no summer time: a zone with its names and offsets"
            " and other rules is not built)\n");
    } else {
        time_t day = 1767268800;    // 2026-01-01 12:00 UTC
        struct tm lt;
        for(int i = 0; i < 360; i++, day += 86400) {
            localtime_r(&day, &lt);
            if(lt.tm_isdst > 0) {
                break;
            }
        }
        char text[32];
        noon_of_day(day, text, sizeof(text));
        unsigned long long t_local = read_now(text);
        char twin[128];
        snprintf(twin, sizeof(twin), "<%s>%ld:%02ld<%s>,J365/0,J365/1",
            tzname[0], timezone/3600, (labs(timezone)%3600)/60, tzname[1]);
        setenv("TZ", twin, 1);
        current_timestamp(bf, sizeof(bf));
        unsetenv("TZ");
        snprintf(bf, sizeof(bf), "'%s', TZ unset after a log line in a twin of the local zone", text);
        check_number(bf, read_now(text), t_local);
        printf("     (twin %s)\n", twin);
    }

    /*
     *  And with TZ set: two zones with the same names and offsets, the
     *  rules of Europe/Athens and of Africa/Cairo in 2026
     */
    static const char *athens = "EET-2EEST,M3.5.0/3,M10.5.0/4";
    static const char *cairo = "EET-2EEST,M4.5.5/0,M10.5.4/24";
    setenv("TZ", athens, 1);
    unsigned long long t_athens = read_now("2026-04-10 12:00");
    check_number("2026-04-10 12:00 in Athens", t_athens, 1775811600ULL);
    setenv("TZ", cairo, 1);
    check_number("2026-04-10 12:00 in Cairo", read_now("2026-04-10 12:00"), 1775815200ULL);
    current_timestamp(bf, sizeof(bf));
    setenv("TZ", athens, 1);
    check_number("Athens after a log line in Cairo, TZ put back with no tzset()",
        read_now("2026-04-10 12:00"), t_athens);

    setenv("TZ", "Europe/Madrid", 1);
    s_tzset_calls = 0;
    static const approxidate_case_t madrid_again[] = {
        {"2026-07-01 10:00",                "2026-07-01 10:00:00 +0200", 0},
        {"2026-10-01 10:00",                "2026-10-01 10:00:00 +0200", 0},
    };
    check_approxidate(madrid_again, sizeof(madrid_again)/sizeof(madrid_again[0]));
    check_number("tzset() calls for 2 texts after a setenv(), TZ set", (unsigned long long)s_tzset_calls, 2);

    /*
     *  Set, unset and set again, with no tzset() between them
     */
    setenv("TZ", "Asia/Tokyo", 1);
    unsigned long long t_tokyo = read_now("2026-07-01 10:00");
    check_number("TZ set with no tzset(): Tokyo", t_tokyo, 1782867600ULL);
    unsetenv("TZ");
    check_number("then TZ unset with no tzset(): /etc/localtime", read_now("2026-07-01 10:00"), t_after_tzset);
    setenv("TZ", "Asia/Tokyo", 1);
    check_number("then TZ set again with no tzset(): Tokyo", read_now("2026-07-01 10:00"), t_tokyo);
    unsetenv("TZ");
    check_number("then TZ unset again with no tzset(): /etc/localtime", read_now("2026-07-01 10:00"), t_after_tzset);

    /*
     *  Other code loads another zone in libc (a line of log calls
     *  localtime(), mktime() calls tzset()) and puts TZ back with no
     *  tzset(): the text is read in the TZ of now
     */
    setenv("TZ", "Europe/Madrid", 1);
    unsigned long long t_madrid = read_now("2026-07-01 10:00");
    check_number("Madrid", t_madrid, 1782892800ULL);
    setenv("TZ", "Asia/Tokyo", 1);
    current_timestamp(bf, sizeof(bf));
    setenv("TZ", "Europe/Madrid", 1);
    check_number("Madrid after a log line in Tokyo, TZ put back with no tzset()",
        read_now("2026-07-01 10:00"), t_madrid);

    setenv("TZ", "Asia/Tokyo", 1);
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 126;
    tm.tm_mday = 1;
    tm.tm_isdst = -1;
    mktime(&tm);
    setenv("TZ", "Europe/Madrid", 1);
    check_number("Madrid after a mktime() in Tokyo, TZ put back with no tzset()",
        read_now("2026-07-01 10:00"), t_madrid);

    unsetenv("TZ");
    tzset();
    check_number("/etc/localtime", read_now("2026-07-01 10:00"), t_after_tzset);
    setenv("TZ", "Asia/Tokyo", 1);
    current_timestamp(bf, sizeof(bf));
    unsetenv("TZ");
    check_number("/etc/localtime after a log line in Tokyo, TZ unset again with no tzset()",
        read_now("2026-07-01 10:00"), t_after_tzset);

    setenv("TZ", "Asia/Tokyo", 1);
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 126;
    tm.tm_mday = 1;
    tm.tm_isdst = -1;
    mktime(&tm);
    unsetenv("TZ");
    check_number("/etc/localtime after a mktime() in Tokyo, TZ unset again with no tzset()",
        read_now("2026-07-01 10:00"), t_after_tzset);

    setenv("TZ", "Europe/Madrid", 1);
    tzset();
}

/***************************************************************************
 *  The errors logged, counted, and the last one kept
 ***************************************************************************/
PRIVATE int s_errors_logged = 0;
PRIVATE char s_last_error[512] = "";

PRIVATE int count_errors(void *h, int priority, const char *bf, size_t len)
{
    if(priority <= LOG_ERR) {
        s_errors_logged++;
        snprintf(s_last_error, sizeof(s_last_error), "%.*s", (int)len, bf);
    }
    return 0;
}

PRIVATE void check_errors(const char *label, int expected, const char *msg)
{
    char bf[256];
    snprintf(bf, sizeof(bf), "%s: errors logged", label);
    check_number(bf, (unsigned long long)s_errors_logged, (unsigned long long)expected);
    if(msg) {
        snprintf(bf, sizeof(bf), "%s: the error says '%s'", label, msg);
        check_number(bf, (unsigned long long)(strstr(s_last_error, msg) != NULL), 1);
    }
    s_errors_logged = 0;
    s_last_error[0] = 0;
}

/***************************************************************************
 *  An allocator that fails once, on demand
 ***************************************************************************/
PRIVATE sys_malloc_fn_t s_real_malloc = 0;
PRIVATE BOOL s_fail_next_malloc = FALSE;

PRIVATE void *malloc_that_fails_once(size_t size)
{
    if(s_fail_next_malloc) {
        s_fail_next_malloc = FALSE;
        return NULL;
    }
    return s_real_malloc(size);
}

/***************************************************************************
 *  26. gmtime2timezone() gives the offset of the zone at the time it
 *  converts, not now (in Madrid a time of January got +7200 in October),
 *  and leaves the TZ of the process as it found it, loaded in libc, also
 *  when it cannot keep a copy of it (a failed copy unset TZ, in silence).
 *  A zone libc does not know is taken as UTC with an ERROR (it was UTC in
 *  silence), and a time no calendar holds logs one ERROR (it logged two).
 ***************************************************************************/
PRIVATE void test_gmtime2timezone(void)
{
    struct tm tm;
    time_t offset = -1;

    setenv("TZ", "Asia/Tokyo", 1);
    tzset();
    time_t local = gmtime2timezone(1768816800, "Europe/Madrid", &tm, &offset);
    check_number("gmtime2timezone() 2026-01-19 10:00 UTC in Madrid: offset at that time",
        (unsigned long long)offset, 3600);
    check_number("gmtime2timezone() 2026-01-19 10:00 UTC in Madrid: return",
        (unsigned long long)local, 1768816800ULL + 3600);
    check_number("gmtime2timezone() 2026-01-19 10:00 UTC in Madrid: hour", (unsigned long long)tm.tm_hour, 11);
    local = gmtime2timezone(1784455200, "Europe/Madrid", &tm, &offset);
    check_number("gmtime2timezone() 2026-07-19 10:00 UTC in Madrid: offset at that time",
        (unsigned long long)offset, 7200);
    check_number("gmtime2timezone() 2026-07-19 10:00 UTC in Madrid: return",
        (unsigned long long)local, 1784455200ULL + 7200);
    check_number("gmtime2timezone() 2026-07-19 10:00 UTC in Madrid: hour", (unsigned long long)tm.tm_hour, 12);
    const char *tz = getenv("TZ");
    check_number("gmtime2timezone(): the TZ of the process put back",
        (unsigned long long)(tz && strcmp(tz, "Asia/Tokyo") == 0), 1);
    time_t t_july = 1784455200;
    localtime_r(&t_july, &tm);
    check_number("gmtime2timezone(): the zone of the process loaded again, a localtime_r() after it in Tokyo",
        (unsigned long long)tm.tm_hour, 19);

    /*
     *  A zone libc does not know: UTC, with an ERROR
     */
    static const char *unknown_zones[] = {"Nowhere/Zone", "Europe", "Foo", ":Nowhere", "<XA>0", "<XAAA0"};
    for(size_t i = 0; i < sizeof(unknown_zones)/sizeof(unknown_zones[0]); i++) {
        char label[128];
        s_errors_logged = 0;
        offset = -1;
        local = gmtime2timezone(1768816800, unknown_zones[i], &tm, &offset);
        snprintf(label, sizeof(label), "gmtime2timezone() in '%s': offset 0", unknown_zones[i]);
        check_number(label, (unsigned long long)offset, 0);
        snprintf(label, sizeof(label), "gmtime2timezone() in '%s'", unknown_zones[i]);
        check_errors(label, 1, "Unknown time zone");
    }
    tz = getenv("TZ");
    check_number("gmtime2timezone() in an unknown zone: TZ of the process put back",
        (unsigned long long)(tz && strcmp(tz, "Asia/Tokyo") == 0), 1);
    static const char *utc_zones[] = {"UTC", "Etc/UTC", "Africa/Abidjan", "UTC0", "<+00>0", ""};
    for(size_t i = 0; i < sizeof(utc_zones)/sizeof(utc_zones[0]); i++) {
        char label[128];
        s_errors_logged = 0;
        offset = -1;
        gmtime2timezone(1768816800, utc_zones[i], &tm, &offset);
        snprintf(label, sizeof(label), "gmtime2timezone() in '%s'", utc_zones[i]);
        check_errors(label, 0, NULL);
    }

    /*
     *  A time no calendar holds: one ERROR
     */
    s_errors_logged = 0;
    gmtime2timezone((time_t)0x7fffffffffffffffLL, "Europe/Madrid", &tm, &offset);
    check_errors("gmtime2timezone() of a time no calendar holds", 1, "no calendar holds the time");

    unsetenv("TZ");
    gmtime2timezone(1768816800, "Europe/Madrid", &tm, &offset);
    check_number("gmtime2timezone(): TZ unset is left unset", (unsigned long long)(getenv("TZ") == NULL), 1);

    /*
     *  No memory to keep TZ: TZ is not touched, the time is taken in UTC
     */
    setenv("TZ", "Asia/Tokyo", 1);
    tzset();
    sys_realloc_fn_t real_realloc;
    sys_calloc_fn_t real_calloc;
    sys_free_fn_t real_free;
    gbmem_get_allocators(&s_real_malloc, &real_realloc, &real_calloc, &real_free);
    gbmem_set_allocators(malloc_that_fails_once, real_realloc, real_calloc, real_free);
    s_fail_next_malloc = TRUE;
    offset = -1;
    local = gmtime2timezone(1768816800, "Europe/Madrid", &tm, &offset);
    gbmem_set_allocators(s_real_malloc, real_realloc, real_calloc, real_free);
    tz = getenv("TZ");
    check_number("gmtime2timezone() with no memory: TZ of the process kept",
        (unsigned long long)(tz && strcmp(tz, "Asia/Tokyo") == 0), 1);
    check_number("gmtime2timezone() with no memory: offset 0", (unsigned long long)offset, 0);
    check_number("gmtime2timezone() with no memory: the time in UTC", (unsigned long long)local, 1768816800ULL);
    check_number("gmtime2timezone() with no memory: hour in UTC", (unsigned long long)tm.tm_hour, 10);

    setenv("TZ", "Europe/Madrid", 1);
    tzset();
}

/***************************************************************************
 *  A range, checked
 ***************************************************************************/
PRIVATE void check_range(const char *label, time_range_t range, time_t start, time_t end)
{
    if(range.start == start && range.end == end) {
        printf("ok   %s -> %lld %lld\n", label, (long long)range.start, (long long)range.end);
    } else {
        printf("FAIL %s -> %lld %lld (expected %lld %lld)\n", label,
            (long long)range.start, (long long)range.end, (long long)start, (long long)end);
        global_result += -1;
    }
}

/***************************************************************************
 *  27. get_hours_range() and the rest: the bounds are those of the zone
 *  asked (UTC with none), whatever the zone of the process. They were
 *  built with mktime() in the zone of the process and tm_isdst 0: with
 *  the process in Tokyo, the day of Madrid started at 00:00 of Tokyo, and
 *  with no zone the day of UTC started at the midnight of the process.
 *  An hour is one of the zone: a change of offset of 30 minutes
 *  (Australia/Lord_Howe), or at 02:45 (Pacific/Chatham), ends one and
 *  starts the next (hours of 3600 seconds from the start of the hour of
 *  `t` ended at 02:30 or 03:00 of the clock).
 ***************************************************************************/
PRIVATE void test_time_ranges(void)
{
    setenv("TZ", "Asia/Tokyo", 1);
    tzset();

    /*
     *  Sun 2026-07-19 10:00:00 UTC, 12:00 in Madrid
     */
    time_t t = 1784455200;
    check_range("days of Madrid", get_days_range(t, 1, "Europe/Madrid"), 1784412000, 1784498400);
    check_range("days of UTC", get_days_range(t, 1, ""), 1784419200, 1784505600);
    check_range("hours of Madrid", get_hours_range(t + 1800, 2, "Europe/Madrid"), t, t + 7200);
    check_range("weeks of Madrid, from Monday", get_weeks_range(t, 1, "Europe/Madrid"), 1783893600, 1784498400);
    check_range("months of Madrid", get_months_range(t, 1, "Europe/Madrid"), 1782856800, 1785535200);
    check_range("months of Madrid, into the next year", get_months_range(t, 6, "Europe/Madrid"),
        1782856800, 1798758000);
    check_range("years of Madrid", get_years_range(t, 1, "Europe/Madrid"), 1767222000, 1798758000);

    /*
     *  Sun 2026-10-25, the day of the change of hour in Madrid: 25 hours.
     *  01:30 UTC is 02:30 CET, the second 02:30: its hour is the second.
     */
    check_range("a day of 25 hours in Madrid", get_days_range(1792929600, 1, "Europe/Madrid"),
        1792879200, 1792969200);
    check_range("the second of a repeated hour in Madrid", get_hours_range(1792891800, 1, "Europe/Madrid"),
        1792890000, 1792893600);

    /*
     *  Seconds in `t`, a negative count of months back into the year
     *  before, and more than one year
     */
    check_range("hours of Madrid, t with seconds", get_hours_range(t + 1800 + 17, 2, "Europe/Madrid"),
        t, t + 7200);
    check_range("months of Madrid, 8 back", get_months_range(t, -8, "Europe/Madrid"), 1782856800, 1761951600);
    check_range("years of Madrid, 3", get_years_range(t, 3, "Europe/Madrid"), 1767222000, 1861916400);

    /*
     *  Hours of a zone whose changes are not whole hours, or not at a
     *  whole hour (the bounds of zoneinfo, by the minute)
     */
    static const struct {
        const char *tz;
        time_t t;
        int range;
        time_t start;
        time_t end;
        const char *label;
    } hours[] = {
        {"Australia/Lord_Howe", 1791038717,   1, 1791037800, 1791041400, "01:15 +1030 before 02:00 -> 02:30 +1100"},
        {"Australia/Lord_Howe", 1791038717,   2, 1791037800, 1791043200, "01:15 +1030, 2 hours to 03:00 +1100"},
        {"Australia/Lord_Howe", 1791042005,   1, 1791041400, 1791043200, "02:40 +1100, the hour from 02:30"},
        {"Australia/Lord_Howe", 1775312109,   1, 1775311200, 1775314800, "01:15 +1100 before 02:00 -> 01:30 +1030"},
        {"Australia/Lord_Howe", 1775312109,   2, 1775311200, 1775316600, "01:15 +1100, 2 hours to 02:00 +1030"},
        {"Australia/Lord_Howe", 1775315709,   1, 1775314800, 1775316600, "01:45 +1030, the second 01, from 01:30"},
        {"Australia/Lord_Howe", 1790946000,  48, 1790944200, 1791115200, "48 hours over the change"},
        {"Australia/Lord_Howe", 1791205200, -48, 1791205200, 1791034200, "48 hours back over the change"},
        {"Pacific/Chatham",     1790430330,   1, 1790428500, 1790431200, "02:30 +1245 before 02:45 -> 03:45 +1345"},
        {"Pacific/Chatham",     1790430330,   2, 1790428500, 1790432100, "02:30 +1245, 2 hours to 04:00 +1345"},
        {"Pacific/Chatham",     1790431500,   1, 1790431200, 1790432100, "03:50 +1345, the hour from 03:45"},
        {"Pacific/Chatham",     1790431500,  -2, 1790431200, 1790424900, "03:50 +1345, 2 hours back to 01:00 +1245"},
        {"Pacific/Chatham",     1790338500,  48, 1790338500, 1790507700, "48 hours over the change"},
        /*
         *  A range that goes on from an hour that does not start at
         *  hh:00:00: 24 of those are not a day
         */
        {"Australia/Lord_Howe",  352213559,  25,  352213200,  352301400, "25 hours from before the change of 1981"},
        {"<XAA>-0:19:32<XBB>-0:20,J182/0,J300/0", 1782864605, 1, 1782862828, 1782866400,
            "a change of 28 seconds, the hour from 00:00:28"},
        {"<XAA>-0:19:32<XBB>-0:20,J182/0,J300/0", 1782864605, 2, 1782862828, 1782870000,
            "a change of 28 seconds, 2 hours to 02:00:00"},
        {"<XAA>-0:19:32<XBB>-0:20,J182/0,J300/0", 1782864605, 24, 1782862828, 1782949200,
            "a change of 28 seconds, 24 hours from 00:00:28 are no day"},
    };
    for(size_t i = 0; i < sizeof(hours)/sizeof(hours[0]); i++) {
        char label[160];
        snprintf(label, sizeof(label), "hours of %s, %s", hours[i].tz, hours[i].label);
        check_range(label, get_hours_range(hours[i].t, hours[i].range, hours[i].tz),
            hours[i].start, hours[i].end);
    }

    const char *tz = getenv("TZ");
    check_number("ranges: the TZ of the process put back",
        (unsigned long long)(tz && strcmp(tz, "Asia/Tokyo") == 0), 1);
    struct tm tm;
    localtime_r(&t, &tm);
    check_number("ranges: the zone of the process loaded again, a localtime_r() after them in Tokyo",
        (unsigned long long)tm.tm_hour, 19);

    /*
     *  A zone libc does not know: UTC, with an ERROR
     */
    s_errors_logged = 0;
    check_range("days of an unknown zone: UTC", get_days_range(t, 1, "Nowhere/Zone"), 1784419200, 1784505600);
    check_errors("days of an unknown zone", 1, "Unknown time zone");

    /*
     *  The cost: UTC takes hours in one step, a zone a day at a time, an
     *  hour at a time only around a change of offset
     */
    s_errors_logged = 0;
    s_gmtime_r_calls = 0;
    check_range("10^7 hours of UTC", get_hours_range(t, 10000000, ""), t, t + 36000000000LL);
    check_number("10^7 hours of UTC: one step, gmtime_r() at most 3 times",
        (unsigned long long)(s_gmtime_r_calls <= 3), 1);
    s_gmtime_r_calls = 0;
    check_range("10^7 hours back in UTC", get_hours_range(t, -10000000, ""), t, t - 36000000000LL);
    check_number("10^7 hours back in UTC: one step, gmtime_r() at most 3 times",
        (unsigned long long)(s_gmtime_r_calls <= 3), 1);
    s_localtime_r_calls = 0;
    check_range("a year of hours of Madrid", get_hours_range(1784455200, 8760, "Europe/Madrid"),
        1784455200, 1815991200);
    check_number("a year of hours of Madrid: by days, at most 1400 localtime_r()",
        (unsigned long long)(s_localtime_r_calls <= 1400), 1);

    /*
     *  An end no calendar holds: the range 0 with one ERROR, in UTC and in
     *  a zone, whatever the unit (a gmtime() of it answers NULL)
     */
    static const struct {
        const char *tz;
        const char *label;
    } zones[] = {{"", "UTC"}, {"Europe/Madrid", "Madrid"}};
    for(size_t i = 0; i < sizeof(zones)/sizeof(zones[0]); i++) {
        char label[160];
        snprintf(label, sizeof(label), "2147483647 years of %s", zones[i].label);
        s_errors_logged = 0;
        check_range(label, get_years_range(t, 2147483647, zones[i].tz), 0, 0);
        check_errors(label, 1, "no calendar holds the time");
    }
    time_t last_hour = 67768036191676799LL - 1800;  // the last second of the calendar of gmtime_r()
    s_errors_logged = 0;
    check_range("2 hours from the last hour of the calendar", get_hours_range(last_hour, 2, ""), 0, 0);
    check_errors("2 hours from the last hour of the calendar", 1, "no calendar holds the time");

    /*
     *  A range back past the year 1: the year 0 is the one before it. From
     *  the proleptic calendar of timegm().
     */
    check_range("years of UTC back to the year 0", get_years_range(t, -2026, ""), 1767225600, -62167219200LL);
    check_range("years of UTC back to the year -974", get_years_range(t, -3000, ""), 1767225600, -92903673600LL);
    check_range("months of UTC back to March of the year -2", get_months_range(t, -24340, ""),
        1782864000, -62225193600LL);

    setenv("TZ", "Europe/Madrid", 1);
    tzset();
}

/***************************************************************************
 *  28. A run of 12 digits is a date and a time written with no 'T'
 *  ("202610011000"), not epoch seconds: no date (it was the year 8390,
 *  "202610011000-0500" too); bare epoch seconds have 11 digits at most,
 *  and with '@' up to the year 9999. After a clock time a minus sign and
 *  four digits is its offset, a unit after it or not: "10:00 -0500 hours"
 *  is no date (it was 500 hours back). From 100000000 on, a number is epoch
 *  seconds, and a minus sign glued to it is its offset.
 ***************************************************************************/
PRIVATE void test_twelve_digits_and_four_after_a_clock_time(void)
{
    static const approxidate_case_t cases[] = {
        {"202610011000",                    "2026-10-21 11:00:00 +0200", 1},
        {"202610011000-0500",               "2026-10-21 11:00:00 +0200", 1},
        {"202610011000+0500",               "2026-10-21 11:00:00 +0200", 1},
        {"100000000000",                    "2026-10-21 11:00:00 +0200", 1},
        {"99999999999",                     "5138-11-16 10:46:39 +0100", 0},
        {"@202610011000",                   "8390-06-16 08:36:40 +0200", 0},
        {"20261001T1000-0500",              "2026-10-01 17:00:00 +0200", 0},
        {"10:00 -0500 hours",               "2026-10-21 11:00:00 +0200", 1},
        {"10:00:00 -1000 hours",            "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01T10:00-0500 hours",     "2026-10-21 11:00:00 +0200", 1},
        {"10:00 -0500",                     "2026-10-21 17:00:00 +0200", 0},
        {"10:00 -5 hours",                  "2026-10-21 05:00:00 +0200", 0},
        {"10:00 -05 hours",                 "2026-10-21 05:00:00 +0200", 0},
        {"10:00 -10 days",                  "2026-10-11 10:00:00 +0200", 0},
        {"10:00 -12345 hours",              "2025-05-25 01:00:00 +0200", 0},
        {"-0500 hours",                     "2026-09-30 15:00:00 +0200", 0},
        {"100000000-0500",                  "1973-03-03 10:46:40 +0100", 0},
        {"99999999-0500",                   "2026-10-21 11:00:00 +0200", 1},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));

    timestamp_t t = 0;
    int off = 0;
    check_number("parse_date_basic '202610011000' ret",
        (unsigned long long)(parse_date_basic("202610011000", &t, &off) + 1), 0);
    check_number("parse_date_basic '100000000-0500' ret",
        (unsigned long long)(parse_date_basic("100000000-0500", &t, &off) + 1), 1);
    check_number("parse_date_basic '100000000-0500' offset", (unsigned long long)(off + 300), 0);
}

/***************************************************************************
 *  29. After anything an offset can follow, a minus sign and four digits
 *  is that offset, a unit after it or not: "noon -0500", "yesterday -0500"
 *  and "2026-10-01 -0500" are zones, so with a unit after them they are no
 *  date (they were 500 hours back). Fewer or more digits, or nothing before
 *  the sign, are a count.
 ***************************************************************************/
PRIVATE void test_four_digits_after_what_takes_an_offset(void)
{
    static const approxidate_case_t cases[] = {
        {"noon -0500 hours",                "2026-10-21 11:00:00 +0200", 1},
        {"midnight -0500 hours",            "2026-10-21 11:00:00 +0200", 1},
        {"tea -1000 days",                  "2026-10-21 11:00:00 +0200", 1},
        {"yesterday -0500 hours",           "2026-10-21 11:00:00 +0200", 1},
        {"now -0500 hours",                 "2026-10-21 11:00:00 +0200", 1},
        {"today -0500 hours",               "2026-10-21 11:00:00 +0200", 1},
        {"2 days ago -0500 hours",          "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01 -0500 hours",          "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01-0500 hours",           "2026-10-21 11:00:00 +0200", 1},
        {"Oct 1 2026 -0500 hours",          "2026-10-21 11:00:00 +0200", 1},
        {"2026-10-01 -1000 days",           "2026-10-21 11:00:00 +0200", 1},

        /*
         *  What the sign is there with no unit: the zone
         */
        {"noon -0500",                      "2026-10-20 19:00:00 +0200", 0},
        {"yesterday -0500",                 "2026-10-20 11:00:00 +0200", 0},
        {"2026-10-01 -0500",                "2026-10-01 11:00:00 +0200", 0},

        /*
         *  Any other count with its unit is a duration
         */
        {"noon -5 hours",                   "2026-10-20 07:00:00 +0200", 0},
        {"noon -05 hours",                  "2026-10-20 07:00:00 +0200", 0},
        {"noon -500 hours",                 "2026-09-29 16:00:00 +0200", 0},
        {"yesterday -5 hours",              "2026-10-20 06:00:00 +0200", 0},
        {"2026-10-01 -5 hours",             "2026-10-01 06:00:00 +0200", 0},
        {"2026-10-01 -12345 hours",         "2025-05-05 02:00:00 +0200", 0},
        {"2026-10-01-1d",                   "2026-09-30 11:00:00 +0200", 0},

        /*
         *  With nothing before it no offset can stand there: a duration
         */
        {"-0500 hours",                     "2026-09-30 15:00:00 +0200", 0},
    };
    check_approxidate(cases, sizeof(cases)/sizeof(cases[0]));
}

/***************************************************************************
 *  A file of the zones test
 ***************************************************************************/
PRIVATE void write_zone_file(const char *name, const void *bytes, size_t len)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", ZONES_DIR, name);
    FILE *f = fopen(path, "w");
    if(!f || fwrite(bytes, 1, len, f) != len) {
        printf("FAIL cannot write %s\n", path);
        global_result += -1;
    }
    if(f) {
        fclose(f);
    }
}

/***************************************************************************
 *  gmtime2timezone() in a zone: its offset in January, or UTC with an ERROR
 ***************************************************************************/
PRIVATE void check_known_zone(const char *tz, long expected_offset)
{
    char label[128];
    struct tm tm;
    time_t offset = -1;
    s_errors_logged = 0;
    s_last_error[0] = 0;
    gmtime2timezone(1768816800, tz, &tm, &offset);
    snprintf(label, sizeof(label), "zone '%.90s': its offset", tz);
    check_number(label, (unsigned long long)(offset + 100000), (unsigned long long)(expected_offset + 100000));
    snprintf(label, sizeof(label), "zone '%.90s'", tz);
    check_errors(label, 0, NULL);
}

PRIVATE void check_unknown_zone(const char *tz)
{
    char label[128];
    struct tm tm;
    time_t offset = -1;
    s_errors_logged = 0;
    s_last_error[0] = 0;
    time_t local = gmtime2timezone(1768816800, tz, &tm, &offset);
    snprintf(label, sizeof(label), "zone '%.90s': offset 0", tz);
    check_number(label, (unsigned long long)offset, 0);
    snprintf(label, sizeof(label), "zone '%.90s': the time in UTC", tz);
    check_number(label, (unsigned long long)local, 1768816800ULL);
    snprintf(label, sizeof(label), "zone '%.90s'", tz);
    check_errors(label, 1, "Unknown time zone");
}

/***************************************************************************
 *  30. A zone is what libc will read as one. A file is a zone when it is a
 *  TZif file: libc takes any other as UTC, in silence, and with no 'T' in
 *  its name to tell it the file was no zone. A POSIX TZ is one when all of
 *  it can be read: libc stops at the first thing it cannot, and what it read
 *  up to there (a summer time with no rules) gave UTC in silence, or a day
 *  for an offset of 25 hours. With no tzdata (a container, a minimal node)
 *  the names of UTC are no error: UTC is what they ask for, and it is what
 *  libc gives.
 ***************************************************************************/
PRIVATE void test_zones_libc_reads(void)
{
    char path[PATH_MAX];

    rmrdir(ZONES_DIR);
    mkrdir(ZONES_DIR "/Test", 02770);
    mkrdir(ZONES_DIR "/Directory", 02770);
    mkrdir(ZONES_DIR "/no_tzdata", 02770);

    /*
     *  A TZif file of version 1 with one type: +01:00, XAA
     */
    static const unsigned char tzif[54] = {
        'T', 'Z', 'i', 'f', 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 4,
        0, 0, 0x0e, 0x10, 0, 0,
        'X', 'A', 'A', 0
    };
    write_zone_file("Test/Zone", tzif, sizeof(tzif));
    static const char text[] = "# tzdb data, text\nAD\t+4230+00131\tEurope/Andorra\n";
    write_zone_file("zone.tab", text, sizeof(text) - 1);
    write_zone_file("tzdata.zi", text, sizeof(text) - 1);
    write_zone_file("leapseconds", text, sizeof(text) - 1);
    write_zone_file("empty", text, 0);
    write_zone_file("short", "TZi", 3);
    write_zone_file("almost", "TZiX and more", 13);
    snprintf(path, sizeof(path), "%s/fifo", ZONES_DIR);
    if(mkfifo(path, 0660) < 0) {
        printf("FAIL cannot make %s\n", path);
        global_result += -1;
    }

    setenv("TZ", "Asia/Tokyo", 1);
    tzset();
    setenv("TZDIR", ZONES_DIR, 1);

    check_known_zone("Test/Zone", 3600);
    check_known_zone(":Test/Zone", 3600);
    check_known_zone(ZONES_DIR "/Test/Zone", 3600);
    check_known_zone(":" ZONES_DIR "/Test/Zone", 3600);

    static const char *not_zones[] = {
        "zone.tab", "tzdata.zi", "leapseconds", "empty", "short", "almost", "fifo", "Directory", "Test", "Test/Zone/",
        ":zone.tab", ZONES_DIR "/zone.tab", ":" ZONES_DIR "/zone.tab", ZONES_DIR "/fifo", ZONES_DIR "/none",
        ZONES_DIR, "/etc/passwd", ":/etc/passwd", "/",
    };
    alarm(30);  // a FIFO that reaches libc blocks the process for ever
    for(size_t i = 0; i < sizeof(not_zones)/sizeof(not_zones[0]); i++) {
        check_unknown_zone(not_zones[i]);
    }
    alarm(0);

    /*
     *  POSIX TZ
     */
    static const struct {
        const char *tz;
        long offset;
    } posix_zones[] = {
        {"UTC0",                                    0},
        {"<+00>0",                                  0},
        {":ABC5",                                   -18000},
        {"ABC5",                                    -18000},
        {"ABC+5",                                   -18000},
        {"ABC-5:30",                                19800},
        {"ABC5:30:15",                              -19815},
        {"ABC24",                                   -86400},
        {"ABC-24",                                  86400},
        {"<+0330>-3:30",                            12600},
        {"ABC0DEF",                                 0},
        {"ABC0DEF-2",                               0},
        {"ABC0DEF,M3.2.0",                          0},
        {"ABC0DEF,M3.2.0,M11.1.0",                  0},
        {"ABC0DEF-1,M3.2.0/2,M11.1.0/-1",           0},
        {"ABC0DEF,J60,J300",                        0},
        {"ABC0DEF,60,300",                          0},
        {"ABC0DEF,0,365",                           3600},
        {"ABC0DEF,J1,J365",                         3600},
        {"ABC0DEF-1,J60/0,J61/0",                   0},
        {"ABC0DEF,M3.2.0/167:59:59,M11.1.0/-167",   0},
        {"ABC0DEF,M12.5.6,M1.1.0",                  0},
        {"EET-2EEST,M4.5.5/0,M10.5.4/24",           7200},
        {"<XAA>-0:19:32<XBB>-0:20,J182/0,J300/0",   1172},
    };
    for(size_t i = 0; i < sizeof(posix_zones)/sizeof(posix_zones[0]); i++) {
        check_known_zone(posix_zones[i].tz, posix_zones[i].offset);
    }

    static const char *not_posix_zones[] = {
        "ABC0DEF,M13.1.0", "ABC0DEF,M0.1.0", "ABC0DEF,M3.0.0", "ABC0DEF,M3.6.0", "ABC0DEF,M3.1.7",
        "ABC0DEF,M3.1", "ABC0DEF,M3", "ABC0DEF,Mx", "ABC0DEF,M3-2.0", "ABC0DEF,M3.2-0", "ABC0DEF,J0", "ABC0DEF,J366", "ABC0DEF,366",
        "ABC0DEF,Jx", "ABC0DEF,x", "ABC0DEF,M3.2.0/x", "ABC0DEF,M3.2.0/", "ABC0DEF,M3.2.0/168",
        "ABC0DEF,M3.2.0/-168", "ABC0DEF,M3.2.0/1:60", "ABC0DEF,M3.2.0/1:00:60", "ABC0DEF,M3.2.0,M13.1.0",
        "ABC0DEF,M3.2.0,M11.1.0,J5", "ABC0DEF,M3.2.0,", "ABC0DEF,", "ABC0DEF,M3.2.0 ", "ABC0,M3.2.0",
        "ABC25", "ABC-25", "ABC5:60", "ABC5:00:60", "ABC5:", "ABC5:30:", "ABC5:30:15:10", "ABC0 ", "ABC0DE",
        "ABC0DEF1:60", "ABC0DEF5:", "ABC0DEF+", "ABC0DEF,M3.2.0/1:00:00:00",
        "ABC0DEF25", "<AB>0", "<A B>0", "<ABC>", "<ABC0", "ABC", "ABC+", "AB0", "5ABC", "A0",
    };
    for(size_t i = 0; i < sizeof(not_posix_zones)/sizeof(not_posix_zones[0]); i++) {
        check_unknown_zone(not_posix_zones[i]);
    }

    /*
     *  No tzdata: the names of UTC are UTC, the others are not known
     */
    setenv("TZDIR", ZONES_DIR "/no_tzdata", 1);
    static const char *utc_names[] = {
        "UTC", "GMT", "UCT", "Universal", "Zulu", "Greenwich", "Etc/UTC", "Etc/GMT", ":UTC",
    };
    for(size_t i = 0; i < sizeof(utc_names)/sizeof(utc_names[0]); i++) {
        check_known_zone(utc_names[i], 0);
    }
    check_unknown_zone("Europe/Madrid");
    check_unknown_zone("Etc/GMT+5");
    check_unknown_zone("utc");
    check_unknown_zone("Etc/");

    unsetenv("TZDIR");
    rmrdir(ZONES_DIR);
    setenv("TZ", "Europe/Madrid", 1);
    tzset();
}

/***************************************************************************
 *  31. A time no calendar holds, in the functions that write one: nothing
 *  half written, nothing dereferenced. gmtime_r() leaves its struct tm
 *  written up to where it gave up, and that went to the caller of
 *  gmtime2timezone(); t2timestamp() and formatdate() handed the NULL of
 *  localtime() and gmtime() on, and a column of time with such a value,
 *  from a peer, crashed ycli.
 ***************************************************************************/
PRIVATE void test_time_no_calendar_holds(void)
{
    time_t huge = (time_t)0x7fffffffffffffffLL;
    struct tm tm;
    struct tm zeroed;
    memset(&tm, 0x55, sizeof(tm));
    memset(&zeroed, 0, sizeof(zeroed));
    time_t offset = -1;

    s_errors_logged = 0;
    time_t local = gmtime2timezone(huge, "Europe/Madrid", &tm, &offset);
    check_number("gmtime2timezone() of a time no calendar holds: *ltm zeroed",
        (unsigned long long)(memcmp(&tm, &zeroed, sizeof(tm)) == 0), 1);
    check_number("gmtime2timezone() of a time no calendar holds: offset 0", (unsigned long long)offset, 0);
    check_number("gmtime2timezone() of a time no calendar holds: t returned",
        (unsigned long long)(local == huge), 1);
    check_errors("gmtime2timezone() of a time no calendar holds, *ltm", 1, "no calendar holds the time");

    char bf[90];
    strcpy(bf, "untouched");
    t2timestamp(bf, sizeof(bf), huge, TRUE);
    check_text("t2timestamp() local of a time no calendar holds", bf, "");
    check_errors("t2timestamp() local of a time no calendar holds", 1, "no calendar holds the time");
    strcpy(bf, "untouched");
    t2timestamp(bf, sizeof(bf), huge, FALSE);
    check_text("t2timestamp() UTC of a time no calendar holds", bf, "");
    check_errors("t2timestamp() UTC of a time no calendar holds", 1, "no calendar holds the time");
    strcpy(bf, "untouched");
    t2timestamp(bf, 0, huge, FALSE);
    check_text("t2timestamp() of a time no calendar holds, no room", bf, "untouched");
    check_errors("t2timestamp() of a time no calendar holds, no room", 1, "no calendar holds the time");
    t2timestamp(bf, sizeof(bf), 1768816800, FALSE);
    check_text("t2timestamp() UTC", bf, "2026-01-19T10:00:00.0+0000");
    check_errors("t2timestamp() UTC", 0, NULL);

    strcpy(bf, "untouched");
    formatdate(huge, bf, sizeof(bf), "CCYY-MM-DD");
    check_text("formatdate() of a time no calendar holds", bf, "");
    check_errors("formatdate() of a time no calendar holds", 1, "no calendar holds the time");
    strcpy(bf, "untouched");
    formatdate(huge, bf, 0, "CCYY-MM-DD");
    check_text("formatdate() of a time no calendar holds, no room", bf, "untouched");
    check_errors("formatdate() of a time no calendar holds, no room", 1, "no calendar holds the time");
    formatdate(1768816800, bf, sizeof(bf), "CCYY-MM-DD");
    check_text("formatdate()", bf, "2026-01-19");
    check_errors("formatdate()", 0, NULL);
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
    gobj_log_register_handler("count_errors", 0, count_errors, 0);
    gobj_log_add_handler("count_errors", "count_errors", LOG_OPT_ALL, 0);

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
    test_repeated_and_skipped_hours();
    test_am_pm_at_never_noon_and_zones();
    test_glued_signs_far_offsets_and_counts_after_a_month();
    test_whole_day_skipped_tzset_and_close_changes();
    test_tzset_only_when_tz_changes();
    test_gmtime2timezone();
    test_time_ranges();
    test_twelve_digits_and_four_after_a_clock_time();
    test_four_digits_after_what_takes_an_offset();
    test_zones_libc_reads();
    test_time_no_calendar_holds();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
