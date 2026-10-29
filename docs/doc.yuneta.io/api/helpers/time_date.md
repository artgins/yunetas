# Time & Date

Wall-clock and monotonic time helpers: format and parse ISO-8601 timestamps, convert `time_t` to and from strings, and compute durations.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c)

(approxidate_careful)=
## [`approxidate_careful()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L8456)

Reads a date as a person types it on a command line -- `"2 hours ago"`, `"yesterday noon"`, `"2026-10-01 10:00"`, `"@1700000000 +0000"` -- against now. It is what every tool of the SDK reads a TIME option with (`tr2list --from-t`, `tr2search`, `stats_list`), and what timeranger2 reads a string tkey with. `approxidate(s)` is the same with no error flag: a text that says no date is now.

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
| A duration back from now | `"2 hours ago"`, `"3 days"`, `"1.day"`, `"2 days 3 hours ago"`, `"one week ago"`, `"last month"` |
| Short units | `"1s"` `"5m"` `"2h"` `"1d"` `"1w"` `"1M"` `"1y"`, `"1min"`, `"2hrs"`: `m` is a minute, `M` a month |
| A duration with a minus sign | `"-1d"`, `"-2 hours"`, `"-1w ago"`: the same as without it (7.28.5 to 7.28.8 refused it, and a tool read it as NOW) |
| Days and times | `"now"`, `"today"`, `"yesterday"`, `"never"` (0), `"noon"`, `"midnight"`, `"tea"` (the last one that passed; `"noon +0200"`, `"noon CEST"` in that zone), `"last friday"`, `"2 fridays ago"`, `"3pm"`, `"10:30"`, `"yesterday 15:00"` |
| Dates | `"2026-10-01"`, `"10/01/2026"` (M/D/Y first), `"1.10.2026"` (D.M.Y), `"Oct 1 2026"`, `"July 5th"`, `"the 5th of July"`, `"Sep 2024"`, `"01-Oct-2026"`, `"2026-Oct-01"`, `"2026-10-01T10:00:00+02:00"`, `"2026-10-01 10:00:00,5"`, `"Thu, 01 Oct 2026 10:00:00 +0000"` |
| Dates as programs write them | `"Thu Oct 01 2026 10:00:00 GMT+0200 (CEST)"` (`Date.toString()` of javascript), `"01/Oct/2026:10:00:00 +0200"` (the access log of a web server), `"2026-10-01 10:00:00 (UTC)"` (a zone name in a comment is the zone when no other is given) |
| Epoch seconds | `"@1700000000"`, `"1700000000"`, up to the year 9999 (`"1761000000000"`, milliseconds, is no date); `"@99999999999999 +0000"`, with `@` and an offset, has no year limit; a value no calendar holds (`"@9223372036854775807"`) is no date |

A date with no year is of this year, or of the year before when it is more than 10 days ahead. A date with no clock time keeps the time of day of now. Months and years back are of the calendar (`"1 month ago"` on March 31 is February 28 or 29); days are 24 hours.

M/D/Y is read in the other order when it is no date or is more than 10 days ahead: `"13/10/2026"` is October 13. D.M.Y is read in the other order when it is no date, or when it is more than 10 days ahead and the other order is not: `"1.12.2026"` read in October is January 12, but `"5.4.2027"` is April 5 (both orders are far ahead; 7.28.5 to 7.29.2 took May 4). A date with its year **and a clock time after it** is read as written, however far ahead: `"1.12.2026 10:00"` is December 1 and `"10/01/2027 10:00"` is October 1 (7.28.5 to 7.29.1 took the other order for them too).

A text with a piece that says nothing known is refused as a whole: `"tomorrow"`, `"in 3 days"` (no time ahead of now is read), `"1.5 hours"`, `"2026-02-30"`, `"+5"`, `"-5"`, `"+1d"`. Up to 7.28.4 the port of git's `date.c` read many of them as another date, in silence (`"2 days from now"` was two days BACK, `"2026-02-30"` was March 2); see [`test_date_text`](https://github.com/artgins/yunetas/blob/7.29.2/tests/c/helpers/test_date_text.c).

**Example**

```C
int not_a_date = 0;
timestamp_t t = approxidate_careful("2 hours ago", &not_a_date);  /* now - 7200, not_a_date 0 */
t = approxidate_careful("in 3 days", &not_a_date);                /* now, not_a_date 1 */
t = approxidate("2026-10-01 10:00:00 +0200");                     /* 1790841600 */
```

---

(current_timestamp)=
## [`current_timestamp()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L8909)

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
## [`formatdate()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L9268)

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

['If `format` is NULL or empty, the default format `DD/MM/CCYY-W-ZZZ` is used.', 'Internally, `strftime()` is used to generate the formatted date string.', 'Make sure that `bf` has enough space to store the formatted string.']

---

(htonll)=
## [`htonll()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L9120)

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
## [`list_open_files()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L9150)

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
## [`ntohll()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L9135)

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
## [`parse_date_basic()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L8432)

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

0, or -1 when `date` is no whole date and time (nothing is written).

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
## [`show_date_relative()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L8366)

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
## [`start_msectimer()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L8983)

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
## [`start_sectimer()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L8954)

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
## [`t2timestamp()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L8935)

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

The function uses `strftime()` to format the timestamp in ISO 8601 format with timezone information.

---

(test_msectimer)=
## [`test_msectimer()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L8993)

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
## [`test_sectimer()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L8966)

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
## [`time_in_seconds()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L9033)

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
## [`tm2timestamp()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L8926)

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
## [`get_days_range()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L11399)

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

Day boundaries are computed in the specified timezone so that the range aligns to local midnight.

---

(get_hours_range)=
## [`get_hours_range()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L11367)

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

Hour boundaries are computed in the specified timezone so that the range aligns to the start of each hour.

---

(get_months_range)=
## [`get_months_range()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L11475)

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

Month boundaries are computed in the specified timezone so that the range aligns to the first day of each month at local midnight.

---

(get_weeks_range)=
## [`get_weeks_range()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L11430)

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

Week boundaries are computed in the specified timezone with weeks starting on Monday at local midnight.

---

(get_years_range)=
## [`get_years_range()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L11506)

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

Year boundaries are computed in the specified timezone so that the range aligns to January 1st at local midnight.

---

(gmtime2timezone)=
## [`gmtime2timezone()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L9212)

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
| `tz` | `const char *` | The target timezone name (for example `"Europe/Madrid"`). |
| `ltm` | `struct tm *` | Optional pointer to a `struct tm` to receive the broken-down local time. Can be NULL. |
| `offset` | `time_t *` | Optional pointer to receive the UTC offset in seconds for the given timezone. Can be NULL. |

**Returns**

The adjusted `time_t` value representing the local time in the specified timezone.

**Notes**

The function temporarily sets the `TZ` environment variable to perform the conversion and restores it afterwards.

---

(time_in_milliseconds)=
## [`time_in_milliseconds()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L9020)

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
## [`time_in_milliseconds_monotonic()`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/gobj-c/src/helpers.c#L9008)

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

