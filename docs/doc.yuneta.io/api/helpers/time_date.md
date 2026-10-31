# Time & Date

Wall-clock and monotonic time helpers: format and parse ISO-8601 timestamps, convert `time_t` to and from strings, and compute durations.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c)

(approxidate_careful)=
## [`approxidate_careful()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L8576)

Reads a date as a person types it on a command line -- `"2 hours ago"`, `"yesterday noon"`, `"2026-10-01 10:00"`, `"@1700000000 +0000"` -- against now. It is what reads a date in [`tranger2_time_of_text()`](<#tranger2_time_of_text>), the rule every tool of the SDK reads a TIME option with (`tr2list --from-t`, `tr2search`, `stats_list`, `emu_device`, `time2range`) and timeranger2 reads a text bound and a string tkey with: there a text written as a number (`"007"`, `"10.5"`, `"1,5"`), or with digits that it misreads in groups (`"1,5 days ago"` September 26, `"5 5 ago"` May 5, `"2026 10 01 10:00"` January 10), never gets to it. `approxidate(s)` is the same with no error flag: a text that says no date is now.

```C
timestamp_t approxidate_careful(
    const char  *date,
    int         *error_ret
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `date` | `const char *` | The text to read. |
| `error_ret` | `int *` | Set to 1 when the text says no date, else to 0. May be `NULL`. |

**Returns**

The time the text names, in seconds since the epoch. When the text says no date: now, with `*error_ret` set to 1.

**Notes**

What is read (the full grammar is in `helpers.h`, above `approxidate_careful()`):

| Kind | Examples |
|---|---|
| A duration back from now | `"2 hours ago"`, `"3 days"`, `"1.day"`, `"2 days 3 hours ago"`, `"one week ago"`, `"last month"` (`last` goes with a unit or a weekday: `"last pm"` is no date; up to 7.29.3 it was 13:00). `ago` is optional, but it says how long back a count is, so it follows one, with only blanks, dots, commas, underscores or comments between them (`"1.day.ago"`, `"2 days (x) ago"`): `"5 ago"`, `"5 (days) ago"` and `"1 day 5 ago"` are no date (up to 7.29.3 they were the 5th of the month, and October 4: the 5 read as a day) |
| Short units | `"1s"` `"5m"` `"2h"` `"1d"` `"1w"` `"1M"` `"1y"`, `"1min"`, `"2hrs"`: `m` is a minute, `M` a month |
| A duration with a minus sign | `"-1d"`, `"-2 hours"`, `"-1w ago"`, `"10:00 -1d"`, `"10:00 -5 hours"`: the same as without it (7.28.5 to 7.28.8 refused it, and a tool read it as NOW; up to 7.29.3 it was no date after a clock time, `"10:00 -1d"` and `"10:00 2026-10-01-1d"`, where the `-1` was read as a zone). Where an offset can stand, after a clock time, noon, a date or a day, a minus sign and four digits is that offset, as ISO 8601 writes it, with a unit after it or not: `"10:00 -0500 hours"`, `"noon -0500 hours"`, `"yesterday -0500 hours"` and `"2026-10-01 -0500 hours"` are no date (7.29.3 read the last three as 500 hours back). Write the count with no leading zero, `"noon -500 hours"`. With nothing before it, it is a count: `"-0500 hours"` |
| Days and times | `"now"`, `"today"`, `"yesterday"`, `"never"` (0; it is a whole text: `"now never"`, `"never 10:00"`, `"never Mon"`, `"never UTC"` and `"never (+0200)"` are no date, up to 7.29.3 the epoch, but for `"never UTC"`), `"noon"`, `"midnight"`, `"tea"` (of the day the text names: `"today noon"` is today's, ahead of now or not, `"yesterday noon"` yesterday's, `"last friday noon"` that Friday's, `"Oct 1 noon"` October 1's; up to 7.29.3, before noon, the first two were yesterday's and the day before yesterday's, and the third the Thursday before; with no day named, the last one that passed: `"noon"`; `"noon +0200"`, `"noon CEST"` in that zone; a 12 before noon or midnight is their hour, right after `at` or with no date before it: `"12 noon"`, `"Oct 1 at 12 noon"`, `"yesterday 12 midnight"`, up to 7.29.3 the 12th at noon, refused, and the 11th; after a date the 12 is still the date's, `"Oct 12 noon"` is October 12), `"last friday"`, `"2 fridays ago"`, `"2 friday"` (the second Friday back), `"3pm"`, `"10:30"`, `"yesterday 15:00"`; am/pm needs an hour: `"pm"`, `"today pm"`, `"pm 3"` are no date (up to 7.29.3, at 11:00, today at 23:00 and the 3rd at 23:00). After `at` comes a time of day or a date: `"at yesterday"`, `"at now"`, `"at today"`, `"at never"` are no date (up to 7.29.3 they were read). After a month, a day or a year, a number before a weekday is that date's and the weekday only names it: `"Oct 1 2026 Thu"` is October 1 (up to 7.29.3 it was 2026 Thursdays back, 1987-12-03), `"Oct 5 friday"` October 5; in the plural it stays a count (`"Oct 1 2 fridays"`). Beside a date the weekday is not checked: `"Oct 5 2026 friday"` is Monday, October 5. A year (4 digits, 1970-9999) before a weekday is a year, never a count: `"Oct 1 2026 thursdays"` and `"2000 mondays ago"` are no date (write `"2000 weeks ago"`; up to 7.29.3 `"2026 Thu"` was 2026 Thursdays back, 38 years; `"2026 days ago"` is still a count). A weekday names a day, and the text must say it, as a date or counted from today (`"Thu, 01 Oct 2026"`, `"yesterday Tue"`, `"2 days ago Mon"`; a count of zero and `"now"` are today, `"0 days ago Wed"`, `"now Wed"` on a Wednesday): `"monday 10:00"` and `"2026 Thu"` are no date (up to 7.29.3 they were today at 10:00, and today in 2026). Beside a day counted from today the weekday must be that day's: on a Wednesday `"now Mon"`, `"yesterday Wed"` and `"now 2026 Mon"` are no date (up to 7.29.3 today, yesterday and 1988). That day is the one the text counted, in the zone said or the UTC offset of now (days are 24 hours), not the local day of the time reached: on Sunday 2026-10-25 at 23:30 CET, the night of a change of hour, `"yesterday Sat"` is Saturday at 23:30 +0100 (Sunday at 00:30 CEST) and `"yesterday Sun"` is no date; a count of hours is in that frame too: `"24 hours ago Sat"`, said then, is Sunday at 00:30 CEST. A text says one weekday: `"Mon Thu now"` and `"Mon Thu, 01 Oct 2026"` are no date. A count and its unit, weekday or am/pm are parted by any blank or a comment: `"Oct 1 10\vpm"` is 22:00, `"Oct 1 5 (CEST) pm"` 17:00 CEST (up to 7.29.3 only a space or a tab parted them, and both were 2010-10-01 and 2005-10-01). After `at` a person means a time, so a number alone there (or after `at the`) is no date, whatever its value: an hour with no half of the day (`"Oct 1 at 10"`, `"at 3"`) or no hour at all (`"Oct 1 at 24"`, `"at 2026"`, `"Oct 1 at the 10"`) is refused (up to 7.29.3 2010-10-01, the 3rd of the month, 2024-10-01, today in 2026 and 2010-10-01), and so are `"at 10.30"` and `"at 10.30.00"` (October 30 and 2000-10-30 up to 7.29.3; write `"at 10:30"`); a date of numbers with dots needs its year of 4 digits there (`"at 1.10.2026"`; `"at 1.10.26"` may be 01:10:26). A count back right after `at` is no date either (`"at 5 days"`, `"at 2 fridays ago"`, `"at -1d"`, `"Oct 1 at -2 days"`, `"at last friday"`; up to 7.29.3 they were read), but an hour with an `h`, as Spanish and French write it, is a clock time of 24 hours: `"at 15h"` is 15:00 and `"at 10h30"` 10:30 (up to 7.29.3 15 hours back, and 10 hours back from the 30th; `"15h"` anywhere else is still 15 hours back), and am/pm with it is no date (`"at 15h am"`, `"at 12h am"`). am/pm is the half of the day of an hour of 12: `"15:00 am"`, `"13 pm"`, `"0:30 pm"`, `"noon am"` and `"tea pm"` are no date (up to 7.29.3 15:00, 13:00, 12:30, midnight and 17:00). `"Oct 1 at 10:00"`, `"Oct 1 at 10 pm"` are read, and a number before a month name is a day: `"at 5 Oct 2026"` and `"at 5 of October"` are October 5 |
| Dates | `"2026-10-01"`, `"10/01/2026"` (M/D/Y first), `"1.10.2026"` (D.M.Y), `"Oct 1 2026"`, `"July 5th"`, `"the 5th of July"`, `"Oct 1 3 pm"` (a count right after a month name says no day: with no day in the text it is no date, `"Oct 1 pm"`, `"Jan 1 pm"`, `"Oct 5 days ago"`, `"Oct 2 fridays"`; up to 7.29.3 they took the day of today, `"Jan 1 pm"` was January 21 at 13:00), `"Sep 2024"`, `"2026-10"`, `"2026.10"` and `"at 2026.10"` (a year and its month: the day is today's; `"10.2026"` is no date), `"01-Oct-2026"`, `"2026-Oct-01"`, `"2026-10-01T10:00:00+02:00"`, `"2026-10-01 10:00:00,5"`, `"Thu, 01 Oct 2026 10:00:00 +0000"` |
| Dates as programs write them | `"Thu Oct 01 2026 10:00:00 GMT+0200 (CEST)"` (`Date.toString()` of javascript), `"01/Oct/2026:10:00:00 +0200"` (the access log of a web server), `"2026-10-01 10:00:00 (UTC)"` (a zone name in a comment is the zone when no other is given), `"2026-10-01 10:00:00 (UTC+02:00)"` (an offset in a comment comes before a zone name: 10:00 at +02:00; up to 7.29.3 the `UTC` was taken and the offset lost). An offset in a comment is read only when it is whole, as outside one: `"(UTC-05:0)"` and `"(-05:)"` say no offset |
| Epoch seconds | `"@1700000000"`, `"1700000000"`: bare, 9 to 11 digits (to the year 5138); a bare number of 12 digits is a date and a time written with no `T`, so it is no date (`"202610011000"`, also `"202610011000-0500"`; write `"20261001T1000"`; up to 7.29.3 it was the year 8390); with `@` up to the year 9999 (`"@202610011000"` is 8390; `"1761000000000"`, milliseconds, is no date); `"@99999999999999 +0000"`, with `@` and an offset, has no year limit; a value no calendar holds (`"@9223372036854775807"`) is no date |

A date with no year is of this year, or of the year before when it is more than 10 days ahead. A date with no clock time keeps the time of day of now, in the UTC offset of now, or of the zone said (`"2026-10-01 UTC"` is October 1 at the time of day of now in UTC). Months and years back are of the calendar (`"1 month ago"` on March 31 is February 28 or 29). A count of days or weeks moves the day: with no clock time it is 24 hours a day back from now, and a clock time, noon, midnight or tea is then that time of the day reached. A count of hours, minutes or seconds is taken back from the time the rest of the text says. So `"1 day ago"` and `"24 hours ago"` are the same time, but beside noon they are not: at 11:00 `"noon 1 day ago"` is yesterday's noon, and `"noon 24 hours ago"` 24 hours before the last noon that passed, the day before yesterday's.

An offset (`"+0200"`, `"-05"`) follows a clock time, noon, midnight, tea, epoch seconds or a day (a date, `now`, `today`, a count back), as a zone name does: `"yesterday +0000"` is `"yesterday UTC"` and `"2026-10-01 -05"` is read (up to 7.29.3 both were refused). With nothing of them before it, a sign is no date (`"+0200"`, `"+0200 10:00"`).

With no clock time an offset moves the date, so it must be the offset of a zone there is, from -12:00 to +14:00: `"now-2026"`, `"yesterday -2300"` and `"2026-10-01 +1500"` are no date (a development version after 7.29.3 read the first in a zone of -20:26), and so is one in a comment: `"2026-10-01 (UTC+15:00)"`, `"now (+1500)"` (7.29.3 lost the offset of a comment: it read the first in UTC, and the second as now). With a clock time any offset is read: `"noon -2300"`, `"2026-10-01 10:00 +2300"`. After epoch seconds the offset moves nothing, it only says the offset to show the time with, so any offset is read there too: `"@1761000000 +1500"` is that second. A sign that touches digits is an offset right after the digits of a clock time, as ISO 8601 writes it: `"2026-10-01T10:00-05:00"`, `"10:00-0500"`, `"20261001T100000-0500"`; and after a clock time (noon, midnight, tea) or epoch seconds said before it, where no clock time can follow: `"10:00 2026-10-01-0500"` is October 1 at 10:00 -05:00, `"@1700000000-0500"` and `"1700000000-0500"` that second (up to 7.29.3 the second was no date, while `"1700000000+0200"` was read: the minus was taken as the `-` of a date of numbers). Glued to the digits of a date with only a day before it, it is no date, because it may be a clock time as well: `"2026-10-01-10:00"`, `"10/01/2026-10:00"`, `"1.10.2026-23"` (that development version read the first as October 1 in a zone of -10:00, at the time of day of now there: October 2 in Madrid). Write the offset apart, `"2026-10-01 -10:00"`, or after a clock time. A count and its unit is still a duration there, and after a clock time: `"2026-10-01-1d"` is September 30, `"10:00 2026-10-01-1d"` September 30 at 10:00.

A clock time with a zone and no date is of the day of now in that zone: at 00:30 CEST on 2026-10-25, which is 23:30 CET on the 24th, `"02:30 CET"` is the 24th at 02:30 CET (`"02:30"`, local, is the 25th).

A local time is read with the offset in force before a change of hour near it. A local time that the change repeats is the first one: in Madrid `"2026-10-25 02:30"` is 02:30 +0200 (write `"02:30 CET"` or `"02:30 +0100"` for the second one). A local time that the change skips is that many minutes after the change: in Madrid `"2026-03-29 02:30"` is 02:30 +0100, which is 03:30 +0200. A day that the zone skips whole is no day counted back from today: Apia skipped 2011-12-30, and on the 31st `"yesterday noon"`, `"yesterday 12:30"` and `"last friday noon"` are no date, while `"noon"` before noon, the last noon that passed, is the one of the 29th (in 7.29.3 the answer depended on the text read before: at 00:00 on the 31st `"yesterday 12:30"` read first was the 29th at 12:30, and after a text of the 29th the 31st at 12:30, ahead of now). A count of hours, minutes or seconds counts no day, so it is taken back from that noon: `"noon -2 hours"` is the 29th at 10:00 and `"tea -30 minutes"` the 29th at 16:30, while `"noon -1 day"` is no date. A date said is read as any time a change skips: `"2011-12-30 12:00"` is 24 hours later, the 31st at 12:00 +1400. The answer does not depend on what was read before it: up to 7.29.3 the repeated hour was the one `mktime()` cached from its last call, so `"2026-10-25 02:30"` and `"2am"` (on that day) were 02:30 +0200 after a text of 01:30 that day and 02:30 +0100 after one of the 26th.

The offsets before and after a change are taken a day before and a day after the time read, so the reading is exact for a zone that does not change its offset twice in less than 48 hours. tzdata has no such zone since 1970: its shortest interval between two changes is 6.96 days. A POSIX `TZ` can have one: with `TZ=AAA0BBB-1,J60/0,J61/0` (summer time for one day, March 1) `"2027-03-01 12:00"` is read with the offset of the days around it, +0000, which is 13:00 of summer time. The zone is the `TZ` of the process when the text is read, whatever other code did before it. A `TZ` set, changed or unset with `setenv()` or `unsetenv()` alone is seen, also after other code made libc load another zone: `localtime()`, `mktime()` and `ctime()` call `tzset()` inside libc, so a line of log does it (`current_timestamp()` calls `localtime()`). The zone libc holds is seen also when it has the names and offsets of the local one and other rules: `Africa/Cairo` and `Europe/Athens` are both `EET`/`EEST`, +0200, and `"2026-04-10 12:00"` is an hour apart in them. So the reader calls `tzset()` before the first look of each text at the local zone, with `TZ` set (glibc compares `TZ` with the text of its last load) or unset (glibc looks at `/etc/localtime`, so a change of it is seen at the next text). A text that says its zone (`"2026-10-01T10:00:00Z"`, `"Thu, 01 Oct 2026 10:00:00 +0000"`) or only counts back from now (`"3 hours ago"`, `"now"`, `"@1790000000"`) is the same time in any zone, so it looks at none and costs no `tzset()`.

**The price of that rule, with `TZ` unset: one `stat()` of `/etc/localtime` per text that reads the local zone**, about a microsecond, and nothing with `TZ` set (glibc only compares two strings). It is the same `stat()` a line of log pays, because `localtime()` calls `tzset()` too. Counted with `strace`, for 10 texts (the first loads the file, so it costs an `open()`):

| Text | 7.29.3, `TZ` unset | now, `TZ` unset | `TZ` set |
|---|---|---|---|
| `"2026-10-01T10:00:00Z"`, `"Thu, 01 Oct 2026 10:00:00 +0000"`, `"3 hours ago"`, `"2 days ago"`, `"now"`, `"@1790000000 +0000"` | 0 | 0 | 0 |
| `"2026-10-01 10:00"`, `"noon"`, `"10:00"`, `"noon yesterday"` | 10 | 9 | 0 |
| `"2026-10-01"`, `"Oct 1 2026"`, `"10/01/2026"`, `"last friday"`, `"1 month ago"` | 0 | 9 | 0 |

The last row is what the rule added: 7.29.3 called `tzset()` only for the forms that went through `mktime()`, and read those in whatever zone libc held. It cannot be cached. A `stat()` only once per interval would read a text in the zone other code left in libc until the interval ends, and nothing tells that zone from the local one when both have the same names and offsets. A process that reads many texts with no zone, and that will never see `/etc/localtime` change, can start with `TZ=:/etc/localtime`: glibc reads the same file once and does no `stat()` after it, for its texts and its lines of log. A change of the file is then seen only by a new process.

```bash
TZ=:/etc/localtime ./my_yuno    # no stat() per text; a change of /etc/localtime needs a restart
```

M/D/Y is read in the other order when it is no date or is more than 10 days ahead: `"13/10/2026"` is October 13. D.M.Y is read in the other order when it is no date, or when it is more than 10 days ahead and the other order is not: `"1.12.2026"` read in October is January 12, but `"5.4.2027"` is April 5 (both orders are far ahead; 7.28.5 to 7.29.2 took May 4). A date with its year **and a clock time after it** is read as written, however far ahead: `"1.12.2026 10:00"` is December 1 and `"10/01/2027 10:00"` is October 1 (7.28.5 to 7.29.1 took the other order for them too).

A text with a piece that says nothing known is refused as a whole: `"tomorrow"`, `"in 3 days"` (no time ahead of now is read), `"1.5 hours"`, `"2026-02-30"`, `"+5"`, `"-5"`, `"+1d"`; and so is one that says a weekday and no day (`"monday 10:00"`), a number alone or a count back after `at` (`"Oct 1 at 10"`, `"Oct 1 at 24"`, `"at 5 days"`, `"at -1d"`, `"at last friday"`), am/pm beside an hour that is not of 12 (`"at 15h am"`, `"15:00 am"`) or with no hour (`"pm"`, `"today pm"`), a day word right after `at` (`"at yesterday"`, `"at now"`), `never` with anything else (`"now never"`, `"never (+0200)"`), `ago` with no count right before it (`"5 ago"`, `"1 day 5 ago"`), two weekdays (`"Mon Thu now"`), a weekday that is not the one of the day counted from today (`"now Mon"` on a Wednesday), a sign glued to the digits of a date with only a day before it (`"2026-10-01-10:00"`), an offset of no zone with no clock time (`"now-2026"`), a count right after a month name with no day (`"Oct 1 pm"`), or a day counted back that the zone skipped whole (`"yesterday noon"` in Apia on 2011-12-31). Up to 7.28.4 the port of git's `date.c` read many of them as another date, in silence (`"2 days from now"` was two days BACK, `"2026-02-30"` was March 2); see [`test_date_text`](https://github.com/artgins/yunetas/blob/7.29.3/tests/c/helpers/test_date_text.c).

**Example**

```C
int not_a_date = 0;
timestamp_t t = approxidate_careful("2 hours ago", &not_a_date);  /* now - 7200, not_a_date 0 */
t = approxidate_careful("in 3 days", &not_a_date);                /* now, not_a_date 1 */
t = approxidate("2026-10-01 10:00:00 +0200");                     /* 1790841600 */
t = approxidate_careful("Oct 1 2026 5 (UTC) pm", &not_a_date);    /* 1790874000, not_a_date 0 */
t = approxidate_careful("Oct 1 at 10", &not_a_date);              /* now, not_a_date 1: 10 am or pm? */
t = approxidate_careful("monday 10:00", &not_a_date);             /* now, not_a_date 1: which monday? */
t = approxidate_careful("at 5 Oct 2026", &not_a_date);            /* October 5, 2026, not_a_date 0 */
t = approxidate_careful("0 days ago Mon", &not_a_date);           /* now, not_a_date 0 on a Monday, 1 on another day */
t = approxidate_careful("at 15h", &not_a_date);                   /* today at 15:00, not_a_date 0 */
t = approxidate_careful("at 15h am", &not_a_date);                /* now, not_a_date 1: 15h is of 24 hours */
t = approxidate_careful("today noon", &not_a_date);               /* today at 12:00, before noon too */
t = approxidate_careful("12 noon", &not_a_date);                  /* the last noon that passed, as "noon" */
t = approxidate_careful("today pm", &not_a_date);                 /* now, not_a_date 1: pm of which hour? */
t = approxidate("2026-10-25 02:30");                              /* in Madrid 02:30 +0200, the first one */
t = approxidate_careful("1 day 5 ago", &not_a_date);              /* now, not_a_date 1: what is the 5? */
```

---

(current_timestamp)=
## [`current_timestamp()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9029)

Generates a timestamp string with nanosecond precision in ISO 8601 format, including the local timezone offset.

```C
char *current_timestamp(
    char *bf,
    size_t bfsize
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the generated timestamp. Must be at least 90 bytes long. |
| `bfsize` | `size_t` | Size of the buffer in bytes. |

**Returns**

Returns a pointer to the buffer `bf` containing the formatted timestamp string.

**Notes**

The function uses `clock_gettime(CLOCK_REALTIME, &ts)` to obtain the current time with nanosecond precision and formats it as `YYYY-MM-DDTHH:MM:SS.nnnnnnnnn±hhmm`.

---

(formatdate)=
## [`formatdate()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9388)

`formatdate()` formats a given timestamp into a human-readable date string based on a specified format.

```C
char *formatdate(
    time_t t,
    char *bf,
    int bfsize,
    const char *format
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `t` | `time_t` | The timestamp to be formatted. |
| `bf` | `char *` | The buffer where the formatted date string will be stored. |
| `bfsize` | `int` | The size of the buffer `bf`. |
| `format` | `const char *` | The format string specifying the output format of the date. |

**Returns**

Returns a pointer to the formatted date string stored in `bf`.

**Notes**

['If `format` is NULL or empty, the default format `DD/MM/CCYY-W-ZZZ` is used.', 'Internally, `strftime()` is used to generate the formatted date string.', 'Make sure that `bf` has enough space to store the formatted string.', 'A `t` that no calendar holds gives an empty text and one ERROR (*"no calendar holds the time"*); up to 7.29.3 the process crashed.']

---

(htonll)=
## [`htonll()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9240)

`htonll()` converts a 64-bit integer from host byte order to network byte order. This makes sure of correct endianness for network communication.

```C
uint64_t htonll(uint64_t value);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `value` | `uint64_t` | The 64-bit integer to be converted to network byte order. |

**Returns**

Returns the 64-bit integer in network byte order.

**Notes**

If the system is little-endian, the function swaps the byte order. Otherwise, it returns the value unchanged.

---

(list_open_files)=
## [`list_open_files()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9270)

Lists all open file descriptors for the current process by reading symbolic links in `/proc/self/fd` and printing their resolved paths.

```C
void list_open_files(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

None.

**Notes**

['This function is only available on Linux systems.', 'It reads the `/proc/self/fd` directory to obtain a list of open file descriptors.', 'For each file descriptor, it resolves the symbolic link to determine the actual file path.', 'Errors encountered while reading symbolic links are printed using `perror`.']

---

(ntohll)=
## [`ntohll()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9255)

Converts a 64-bit integer from network byte order to host byte order. The function makes sure of proper endianness conversion based on the system's architecture.

```C
uint64_t ntohll(uint64_t value);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `value` | `uint64_t` | The 64-bit integer in network byte order to be converted. |

**Returns**

Returns the 64-bit integer in host byte order.

**Notes**

This function checks the system's byte order and swaps bytes if necessary to make sure that correct conversion.

---

(parse_date_basic)=
## [`parse_date_basic()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L8552)

Reads only a whole date and time, or epoch seconds: a year, a month, a day and a clock time, with an optional zone (`+0200`, `+02:00`, `Z`, `UTC`, `CEST`, `GMT+0200`, ...), an optional weekday and an optional comment in parentheses. Anything relative, or a word it does not know, is refused: that is what [`approxidate_careful()`](#approxidate_careful) is for.

```C
int parse_date_basic(
    const char  *date,
    timestamp_t *timestamp,
    int         *offset
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `date` | `const char *` | The text to read. |
| `timestamp` | `timestamp_t *` | Where the time goes, in seconds since the epoch. |
| `offset` | `int *` | Where the offset goes, in minutes east of UTC: the one given, or the local one at that time when none was. |

**Returns**

0, or -1 when `date` is no whole date and time (nothing is written). Check
the answer: on -1 `*timestamp` keeps whatever it held, and a caller that
reads it anyway reads garbage. Up to 7.29.3 `time2range -t garbage` did,
and answered a range in a year in the millions, other from one build to the
next.

**Example**

```C
timestamp_t t;
int offset;
parse_date_basic("2026-10-01 10:00:00 +0200", &t, &offset);     /* 0, t 1790841600, offset 120 */
parse_date_basic("Thu, 01 Oct 2026 10:00:00 GMT", &t, &offset); /* 0, t 1790848800, offset 0 */
parse_date_basic("@1700000000", &t, &offset);                   /* 0, offset the local one */
parse_date_basic("2026-10-01", &t, &offset);                    /* -1: no clock time */
```

---

(show_date_relative)=
## [`show_date_relative()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L8486)

How long ago a time was, in the unit that fits: seconds up to 89, minutes up to 89, hours up to 35, days up to 13, weeks up to 69 days, months up to 364 days, years and months up to five years, then years. Each unit is rounded from the one below it. A time ahead of now is `in the future`.

```C
void show_date_relative(
    timestamp_t time,
    char        *timebuf,
    int         timebufsize
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `time` | `timestamp_t` | Seconds since the epoch. |
| `timebuf` | `char *` | Buffer for the text. |
| `timebufsize` | `int` | Size of `timebuf`. |

**Returns**

Nothing.

**Example**

```C
char bf[64];
show_date_relative(time(NULL) - 90, bf, sizeof(bf));        /* "2 minutes ago" */
show_date_relative(time(NULL) - 32873370, bf, sizeof(bf));  /* "1 year, 1 month ago" */
```

---

(start_msectimer)=
## [`start_msectimer()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9103)

`start_msectimer()` initializes a millisecond-resolution timer and returns the expiration timestamp.

```C
uint64_t start_msectimer(
    uint64_t milliseconds
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `milliseconds` | `uint64_t` | The duration of the timer in milliseconds. |

**Returns**

Returns the absolute expiration timestamp in milliseconds of the monotonic clock (`CLOCK_MONOTONIC`, see [`time_in_milliseconds_monotonic()`](#time_in_milliseconds_monotonic)). With `milliseconds` 0 it returns the current time: the timer has expired at once.

**Notes**

Use [`test_msectimer()`](#test_msectimer) to check if the timer has expired.
This is the pair to time a timeout: the monotonic clock does not move when the wall clock is set back or forward (by hand, or by an NTP step at boot).
The pair [`start_sectimer()`](#start_sectimer) / [`test_sectimer()`](#test_sectimer) reads the wall clock.

```C
uint64_t t_retry = start_msectimer(60*1000);    // one minute of real time
...
if(test_msectimer(t_retry)) {
    // try again
}
```

---

(start_sectimer)=
## [`start_sectimer()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9074)

`start_sectimer()` initializes a timer by adding the specified number of seconds to the current system time and returns the future timestamp.

It reads the **wall clock** (`time()`): a clock set back makes the timer late, one set forward makes it expire early. To time a timeout, use [`start_msectimer()`](#start_msectimer) / [`test_msectimer()`](#test_msectimer), which read the monotonic clock.

```C
time_t start_sectimer(time_t seconds);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `seconds` | `time_t` | The number of seconds to add to the current system time. |

**Returns**

Returns a `time_t` value representing the future timestamp when the timer will expire.

**Notes**

['Use [`test_sectimer()`](#test_sectimer) to check if the timer has expired.', 'If `seconds` is less than or equal to zero, the function still returns the current time.']

---

(t2timestamp)=
## [`t2timestamp()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9055)

`t2timestamp()` converts a given time value into a formatted timestamp string, supporting both local and UTC time representations.

```C
char *t2timestamp(
    char *bf,
    int bfsize,
    time_t t,
    BOOL local
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the formatted timestamp. |
| `bfsize` | `int` | Size of the buffer to make sure that safe string formatting. |
| `t` | `time_t` | Time value to be converted into a timestamp. |
| `local` | `BOOL` | Flag indicating whether to use local time (`TRUE`) or UTC (`FALSE`). |

**Returns**

Returns a pointer to the formatted timestamp string stored in `bf`.

**Notes**

The function uses `strftime()` to format the timestamp in ISO 8601 format with timezone information. A `t` that no calendar holds gives an empty text and one ERROR (*"no calendar holds the time"*); up to 7.29.3 the process crashed.

---

(test_msectimer)=
## [`test_msectimer()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9113)

Checks if the given millisecond timer has expired by comparing it with the current monotonic time.

```C
BOOL test_msectimer(
    uint64_t value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `value` | `uint64_t` | The timestamp in milliseconds to check against the current monotonic time. |

**Returns**

Returns `TRUE` if the timer has expired, otherwise returns `FALSE`.

**Notes**

The function uses [`time_in_milliseconds_monotonic()`](#time_in_milliseconds_monotonic) to obtain the current monotonic time and compares it with `value`. A `value` of 0 never expires (`FALSE`).

---

(test_sectimer)=
## [`test_sectimer()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9086)

Checks if the given `value` time has elapsed compared to the current system time (the wall clock, `time()`: see [`start_sectimer()`](#start_sectimer)).

```C
BOOL test_sectimer(time_t value);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `value` | `time_t` | The target time to check against the current system time. |

**Returns**

Returns `TRUE` if the current time is greater than or equal to `value`, otherwise returns `FALSE`.

**Notes**

If `value` is less than or equal to zero, the function returns `FALSE` without performing any time comparison.

---

(time_in_seconds)=
## [`time_in_seconds()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9153)

`time_in_seconds()` returns the current system time in seconds since the Unix epoch (January 1, 1970).

```C
time_t time_in_seconds(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Returns the current time in seconds as a `uint64_t` integer.

**Notes**

['This function retrieves the current time using `time(&t)`, which provides the number of seconds elapsed since the Unix epoch.', 'It is useful for timestamping and time-based calculations.']

---

(tm2timestamp)=
## [`tm2timestamp()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9046)

Converts a `struct tm` time representation into an ISO 8601 formatted timestamp string.

```C
char *tm2timestamp(
    char *bf,
    int   bfsize,
    struct tm *tm
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Buffer to store the formatted timestamp. |
| `bfsize` | `int` | Size of the buffer `bf`. |
| `tm` | `struct tm *` | Pointer to a `struct tm` containing the time to be formatted. |

**Returns**

Returns a pointer to the buffer `bf` containing the formatted timestamp.

**Notes**

The output format follows the ISO 8601 standard: `YYYY-MM-DDTHH:MM:SS.0±HHMM`.

---

(get_days_range)=
## [`get_days_range()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L11519)

Calculates a time range spanning multiple days.

```C
time_range_t get_days_range(
    time_t t,
    int range,
    const char *TZ
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `t` | `time_t` | The reference timestamp around which to compute the range. |
| `range` | `int` | The number of days to span. |
| `TZ` | `const char *` | Timezone name for localizing the day boundaries. |

**Returns**

A `time_range_t` structure containing the `start` and `end` timestamps of the computed range.

**Notes**

Day boundaries are computed in the specified timezone so that the range aligns to local midnight: `range` days later is local midnight too, so the day of a change of hour has 23 or 25 hours. With an empty `TZ` the boundaries are those of UTC. The zone of the process does not matter: up to 7.29.3 the boundaries were built with `mktime()` in the zone of the process and `tm_isdst` 0, so with the process in `Asia/Tokyo` a day of `Europe/Madrid` started at 00:00 in Tokyo, and with an empty `TZ` a day of UTC started at the midnight of the process (an hour later in summer). A range back past the year 1 is proleptic, with a year 0 before it, and an `end` no calendar holds gives the range `{0, 0}` and an ERROR: up to 7.29.3 the year wrapped, and `get_years_range(t, 2147483647, "")` answered an `end` some two billion years back.

**Example**

```C
time_range_t r = get_days_range(1784455200, 1, "Europe/Madrid");
/* 2026-07-19 10:00 UTC: r.start 1784412000 (2026-07-19 00:00 +0200), r.end 1784498400 */
r = get_days_range(1784455200, 1, "");
/* r.start 1784419200 (2026-07-19 00:00 UTC), r.end 1784505600 */
```

---

(get_hours_range)=
## [`get_hours_range()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L11487)

Calculates a time range spanning multiple hours.

```C
time_range_t get_hours_range(
    time_t t,
    int range,
    const char *TZ
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `t` | `time_t` | The reference timestamp around which to compute the range. |
| `range` | `int` | The number of hours to span. |
| `TZ` | `const char *` | Timezone name for localizing the hour boundaries. |

**Returns**

A `time_range_t` structure containing the `start` and `end` timestamps of the computed range.

**Notes**

Hour boundaries are computed in the specified timezone so that the range aligns to the start of each hour, and `end` is `range` hours of the zone after `start` (before it, when `range` is negative). In an hour that a change of hour repeats, it is the hour of `t`: in Madrid 01:30 UTC of 2026-10-25 (the second 02:30) is in the hour from 01:00 to 02:00 UTC. A change of offset that is no whole hour, or not at a whole hour, ends an hour and starts the next one, shorter: in `Australia/Lord_Howe` (+1030, then +1100 at 02:00 of 2026-10-04, when the clock goes to 02:30) the hour of 01:15 ends at the change, and the next one goes from 02:30 to 03:00; in `Pacific/Chatham` (+1245, then +1345 at 02:45 of 2026-09-27) the hour of 02:30 goes from 02:00 to the change, and the next one from 03:45 to 04:00. A `TZ` that libc does not know is taken as UTC, with an ERROR. With an empty `TZ` the boundaries are those of UTC. The zone of the process does not matter: up to 7.29.3 the boundaries were built with `mktime()` in the zone of the process and `tm_isdst` 0, so with the process in `Asia/Tokyo` a day of `Europe/Madrid` started at 00:00 in Tokyo, and with an empty `TZ` a day of UTC started at the midnight of the process (an hour later in summer).

An `end` no calendar holds gives the range `{0, 0}` and an ERROR (*"no calendar holds the time"*). In UTC the range is one step. In a zone it is walked by days, and by hours around a change of offset, so the cost grows with the years it covers: a year of hours is some 1400 calls of `localtime_r()`, 0.2 ms; `range` is an `int`, and 2147483647 hours (245146 years, 337 million calls) take minutes. Up to 7.29.3 it was one step for any zone, and wrong in the zones above.

**Example**

```C
time_range_t r = get_hours_range(1784457000, 2, "Europe/Madrid");
/* 2026-07-19 12:30 in Madrid: r.start 1784455200 (12:00 +0200), r.end 1784462400 (14:00) */
r = get_hours_range(1791038717, 2, "Australia/Lord_Howe");
/* 2026-10-04 01:15:17 +1030: r.start 1791037800 (01:00 +1030), r.end 1791043200 (03:00 +1100) */
r = get_hours_range(1790431500, 1, "Pacific/Chatham");
/* 2026-09-27 03:50 +1345: r.start 1790431200 (03:45 +1345, the change), r.end 1790432100 (04:00) */
```

---

(get_months_range)=
## [`get_months_range()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L11595)

Calculates a time range spanning multiple months.

```C
time_range_t get_months_range(
    time_t t,
    int range,
    const char *TZ
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `t` | `time_t` | The reference timestamp around which to compute the range. |
| `range` | `int` | The number of months to span. |
| `TZ` | `const char *` | Timezone name for localizing the month boundaries. |

**Returns**

A `time_range_t` structure containing the `start` and `end` timestamps of the computed range.

**Notes**

Month boundaries are computed in the specified timezone so that the range aligns to the first day of each month at local midnight; a negative `range` counts back, into the years before too. With an empty `TZ` the boundaries are those of UTC. The zone of the process does not matter: up to 7.29.3 the boundaries were built with `mktime()` in the zone of the process and `tm_isdst` 0, so with the process in `Asia/Tokyo` a day of `Europe/Madrid` started at 00:00 in Tokyo, and with an empty `TZ` a day of UTC started at the midnight of the process (an hour later in summer). A range back past the year 1 is proleptic, with a year 0 before it, and an `end` no calendar holds gives the range `{0, 0}` and an ERROR: up to 7.29.3 the year wrapped, and `get_years_range(t, 2147483647, "")` answered an `end` some two billion years back.

**Example**

```C
time_range_t r = get_months_range(1784455200, 6, "Europe/Madrid");
/* r.start 1782856800 (2026-07-01 00:00 +0200), r.end 1798758000 (2027-01-01 00:00 +0100) */
r = get_months_range(1784455200, -8, "Europe/Madrid");
/* a negative range counts back: r.end 1761951600 (2025-11-01 00:00 +0100), before r.start */
```

---

(get_weeks_range)=
## [`get_weeks_range()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L11550)

Calculates a time range spanning multiple weeks, starting from Monday.

```C
time_range_t get_weeks_range(
    time_t t,
    int range,
    const char *TZ
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `t` | `time_t` | The reference timestamp around which to compute the range. |
| `range` | `int` | The number of weeks to span. |
| `TZ` | `const char *` | Timezone name for localizing the week boundaries. |

**Returns**

A `time_range_t` structure containing the `start` and `end` timestamps of the computed range.

**Notes**

Week boundaries are computed in the specified timezone with weeks starting on Monday at local midnight. With an empty `TZ` the boundaries are those of UTC. The zone of the process does not matter: up to 7.29.3 the boundaries were built with `mktime()` in the zone of the process and `tm_isdst` 0, so with the process in `Asia/Tokyo` a day of `Europe/Madrid` started at 00:00 in Tokyo, and with an empty `TZ` a day of UTC started at the midnight of the process (an hour later in summer). A range back past the year 1 is proleptic, with a year 0 before it, and an `end` no calendar holds gives the range `{0, 0}` and an ERROR: up to 7.29.3 the year wrapped, and `get_years_range(t, 2147483647, "")` answered an `end` some two billion years back.

**Example**

```C
time_range_t r = get_weeks_range(1784455200, 1, "Europe/Madrid");
/* Sunday 2026-07-19: r.start 1783893600 (Monday 2026-07-13 00:00 +0200), r.end 1784498400 */
```

---

(get_years_range)=
## [`get_years_range()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L11626)

Calculates a time range spanning multiple years, starting from January 1st.

```C
time_range_t get_years_range(
    time_t t,
    int range,
    const char *TZ
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `t` | `time_t` | The reference timestamp around which to compute the range. |
| `range` | `int` | The number of years to span. |
| `TZ` | `const char *` | Timezone name for localizing the year boundaries. |

**Returns**

A `time_range_t` structure containing the `start` and `end` timestamps of the computed range.

**Notes**

Year boundaries are computed in the specified timezone so that the range aligns to January 1st at local midnight. With an empty `TZ` the boundaries are those of UTC. The zone of the process does not matter: up to 7.29.3 the boundaries were built with `mktime()` in the zone of the process and `tm_isdst` 0, so with the process in `Asia/Tokyo` a day of `Europe/Madrid` started at 00:00 in Tokyo, and with an empty `TZ` a day of UTC started at the midnight of the process (an hour later in summer). A range back past the year 1 is proleptic, with a year 0 before it, and an `end` no calendar holds gives the range `{0, 0}` and an ERROR: up to 7.29.3 the year wrapped, and `get_years_range(t, 2147483647, "")` answered an `end` some two billion years back.

**Example**

```C
time_range_t r = get_years_range(1784455200, 1, "Europe/Madrid");
/* r.start 1767222000 (2026-01-01 00:00 +0100), r.end 1798758000 (2027-01-01 00:00 +0100) */
r = get_years_range(1784455200, 3, "Europe/Madrid");
/* r.end 1861916400 (2029-01-01 00:00 +0100) */
```

---

(gmtime2timezone)=
## [`gmtime2timezone()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9332)

Converts a Unix timestamp to local time in a specified timezone.

```C
time_t gmtime2timezone(
    time_t t,
    const char *tz,
    struct tm *ltm,
    time_t *offset
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `t` | `time_t` | The Unix timestamp to convert. |
| `tz` | `const char *` | The target timezone, a `TZ` text (for example `"Europe/Madrid"` or `":America/Panama"`). |
| `ltm` | `struct tm *` | Optional pointer to a `struct tm` to receive the broken-down local time of `t` in `tz`. Can be NULL. |
| `offset` | `time_t *` | Optional pointer to receive the UTC offset of `tz` at the time `t`, in seconds. Can be NULL. |

**Returns**

`t` plus the UTC offset of `tz` at `t`: `gmtime_r()` of it is the local time.

**Notes**

The offset is the one in force at `t`, not now: a time of January in `Europe/Madrid` gets +3600 and one of July +7200 (up to 7.29.3 both got the offset of the moment of the call). The function sets the `TZ` of the process to `tz` for the conversion, and puts it back with a `tzset()` before returning. When the `TZ` of the process cannot be kept (no memory for a copy) or `tz` cannot be set, `TZ` is left as it was, the error is logged, and `t` is taken in UTC (offset 0); up to 7.29.3 a failed copy unset the `TZ` of the process, in silence. So is a `tz` that libc will not read as the zone it says, because libc takes it as UTC or as half of what it says, in silence: it logs *"Unknown time zone: taken as UTC"*, and `TZ` is not touched (up to 7.29.3 it was UTC in silence). A `t` that no calendar holds logs one ERROR (*"no calendar holds the time"*; up to 7.29.3 it logged two) and is returned as it is, with `*ltm` zeroed (it was left written up to where `gmtime_r()` gave up) and offset 0.

A zone is one of these:

| `tz` | Known | Example |
|---|---|---|
| A TZif file under `TZDIR`, else `/usr/share/zoneinfo`, or an absolute path | The first four bytes are `TZif` | `"Europe/Madrid"`, `":America/Panama"`, `"/etc/localtime"`. Any other file is no zone: `"zone.tab"`, `"tzdata.zi"`, `"leapseconds"`, `"/etc/passwd"`. Nor a directory, `"Europe"` |
| A POSIX `TZ`: `std offset [dst [offset] [,start[/time][,end[/time]]]]` | All of it can be read | `"UTC0"`, `"<+00>0"`, `"ABC-5:30"`, `"EET-2EEST,M3.5.0/3,M10.5.0/4"`. A name has 3 letters or more, or 3 characters between `<` and `>`; an offset is up to 24:59:59; a rule is `Mm.w.d` (`m` 1-12, `w` 1-5, `d` 0-6), `Jn` (1-365) or `n` (0-365), and its time up to 167:59:59. `"ABC0DEF,M13.1.0"`, `"ABC25"` and `"ABC5:60"` are no zone: libc read them as UTC, a day, and 5:59 |
| A name of UTC, with no zone file for it | `UTC`, `GMT`, `UCT`, `Universal`, `Zulu`, `Greenwich`, also after `Etc/` | A node or a container with no tzdata: `"UTC"` is UTC with no ERROR, and `"Europe/Madrid"` is UTC with one |
| Empty, or `":"` | UTC by POSIX | `""` |

A `tz` that is a FIFO does not block the process: it is no zone file, and it never reaches `tzset()`.

**Example**

```C
struct tm tm;
time_t offset;
time_t local = gmtime2timezone(1784455200, "Europe/Madrid", &tm, &offset);
/* 2026-07-19 10:00:00 UTC: tm is 12:00:00 of that day, offset 7200, local 1784462400 */
gmtime2timezone(1768816800, "Europe/Madrid", &tm, &offset);
/* 2026-01-19 10:00:00 UTC: tm is 11:00:00, offset 3600 */
```

---

(time_in_milliseconds)=
## [`time_in_milliseconds()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9140)

Returns the current wall-clock time in milliseconds since the Unix epoch.

```C
uint64_t time_in_milliseconds(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A `uint64_t` value representing the current time in milliseconds, obtained using `CLOCK_REALTIME`.

**Notes**

This clock is subject to system time adjustments (for example NTP). For measuring elapsed time, use [`time_in_milliseconds_monotonic()`](#time_in_milliseconds_monotonic) instead.

---

(time_in_milliseconds_monotonic)=
## [`time_in_milliseconds_monotonic()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9128)

Returns the current monotonic time in milliseconds.

```C
uint64_t time_in_milliseconds_monotonic(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A `uint64_t` value representing the current monotonic time in milliseconds, obtained using `CLOCK_MONOTONIC`.

**Notes**

Monotonic time is not affected by system clock adjustments and is suitable for measuring elapsed durations. This is the clock used by [`start_msectimer()`](#start_msectimer) and [`test_msectimer()`](#test_msectimer).

---

