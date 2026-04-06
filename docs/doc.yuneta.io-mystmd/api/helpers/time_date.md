# Time & Date

Wall-clock and monotonic time helpers: format and parse ISO-8601 timestamps, convert `time_t` to and from strings, and compute durations.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(current_timestamp)=
## `current_timestamp()`

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

(date_mode_from_type)=
## `date_mode_from_type()`

The function `date_mode_from_type()` returns a pointer to a static `struct date_mode` initialized with the given `date_mode_type`. If the type is `DATE_STRFTIME`, an error message is logged.

```C
struct date_mode *date_mode_from_type(
    enum date_mode_type type
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `type` | `enum date_mode_type` | The date mode type to initialize the `date_mode` structure. |

**Returns**

A pointer to a static `struct date_mode` initialized with the given type. If `DATE_STRFTIME` is passed, an error message is logged.

**Notes**

The returned pointer refers to a static structure, so it should not be modified or freed by the caller.

---

(date_overflows)=
## `date_overflows()`

Checks if a given timestamp exceeds the system's time_t limits, ensuring it fits within the supported range.

```C
int date_overflows(timestamp_t date);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `date` | `timestamp_t` | The timestamp to check for overflow. |

**Returns**

Returns 1 if the timestamp exceeds the system's time_t limits, otherwise returns 0.

**Notes**

This function ensures that the given timestamp does not exceed the maximum representable value in time_t, preventing potential overflows.

---

(datestamp)=
## `datestamp()`

Generates a timestamp string representing the current date and time in a standard format.

```C
void datestamp(
    char *out,
    int   outsize
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `out` | `char *` | Buffer to store the generated timestamp string. |
| `outsize` | `int` | Size of the output buffer to ensure safe string operations. |

**Returns**

None.

**Notes**

The function formats the current date and time into a string and stores it in the provided buffer. The format used is compatible with standard timestamp representations.

---

(formatdate)=
## `formatdate()`

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

['If `format` is NULL or empty, the default format `DD/MM/CCYY-W-ZZZ` is used.', 'Internally, `strftime()` is used to generate the formatted date string.', 'Ensure that `bf` has enough space to store the formatted string.']

---

(htonll)=
## `htonll()`

`htonll()` converts a 64-bit integer from host byte order to network byte order, ensuring correct endianness for network communication.

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

If the system is little-endian, the function swaps the byte order; otherwise, it returns the value unchanged.

---

(list_open_files)=
## `list_open_files()`

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
## `ntohll()`

Converts a 64-bit integer from network byte order to host byte order. The function ensures proper endianness conversion based on the system's architecture.

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

This function checks the system's byte order and swaps bytes if necessary to ensure correct conversion.

---

(parse_date)=
## `parse_date()`

Parses a date string into a structured timestamp and timezone offset. The function supports various date formats and converts them into a standardized format.

```C
int parse_date(
    const char *date,
    char *out,
    int outsize
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `date` | `const char *` | The input date string to be parsed. |
| `out` | `char *` | Buffer to store the formatted date output. |
| `outsize` | `int` | Size of the output buffer. |

**Returns**

Returns 0 on success, or -1 if the date string could not be parsed.

**Notes**

The function supports various date formats, including relative dates like 'yesterday' and 'now'. It converts the parsed date into a standardized format with a timestamp and timezone offset.

---

(parse_date_basic)=
## `parse_date_basic()`

Parses a date string into a timestamp and timezone offset. The function supports various date formats and extracts the corresponding Unix timestamp and timezone offset.

```C
int parse_date_basic(
    const char *date,
    timestamp_t *timestamp,
    int *offset
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `date` | `const char *` | The input date string to be parsed. |
| `timestamp` | `timestamp_t *` | Pointer to store the parsed Unix timestamp. |
| `offset` | `int *` | Pointer to store the parsed timezone offset in minutes. |

**Returns**

Returns 0 on success, or -1 if the date string could not be parsed.

**Notes**

This function is used internally by `approxidate_careful()` and `approxidate_relative()` to interpret date strings in various formats.

---

(parse_date_format)=
## `parse_date_format()`

Parses a date format string and initializes a `date_mode` structure with the corresponding format type and localization settings.

```C
void parse_date_format(
    const char *format,
    struct date_mode *mode
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `format` | `const char *` | The date format string to be parsed. |
| `mode` | `struct date_mode *` | Pointer to a `date_mode` structure that will be initialized based on the parsed format. |

**Returns**

This function does not return a value.

**Notes**

If the format string starts with `auto:`, the function selects an appropriate format based on whether the output is a terminal. The function also supports historical aliases such as `default-local` for local time formatting.

---

(parse_expiry_date)=
## `parse_expiry_date()`

Parses a date string and converts it into a timestamp. Special keywords like `never`, `all`, and `now` are handled explicitly.

```C
int parse_expiry_date(
    const char *date,
    timestamp_t *timestamp
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `date` | `const char *` | A string representing the date to be parsed. Can be a standard date format or special keywords like `never`, `all`, or `now`. |
| `timestamp` | `timestamp_t *` | Pointer to a variable where the parsed timestamp will be stored. |

**Returns**

Returns 0 on success, or a nonzero value if an error occurs during parsing.

**Notes**

If the `date` string is `never`, the function sets `timestamp` to 0. If `date` is `all` or `now`, `timestamp` is set to the maximum possible value (`TIME_MAX`). Otherwise, it attempts to parse the date using `approxidate_careful()`.

---

(show_date)=
## `show_date()`

Formats a given timestamp into a human-readable date string based on the specified date mode and timezone offset.

```C
const char *show_date(
    timestamp_t time,
    int timezone,
    const struct date_mode *mode
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `time` | `timestamp_t` | The timestamp to be formatted. |
| `timezone` | `int` | The timezone offset in minutes from UTC. |
| `mode` | `const struct date_mode *` | Pointer to a `date_mode` structure specifying the formatting style. |

**Returns**

A pointer to a statically allocated string containing the formatted date.

**Notes**

The returned string is stored in a static buffer and should not be modified or freed by the caller.

---

(show_date_relative)=
## `show_date_relative()`

Formats a timestamp into a human-readable relative time string, such as '3 days ago' or '2 hours ago'.

```C
void show_date_relative(
    timestamp_t time,
    char *timebuf,
    int timebufsize
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `time` | `timestamp_t` | The timestamp to be converted into a relative time string. |
| `timebuf` | `char *` | A buffer where the formatted relative time string will be stored. |
| `timebufsize` | `int` | The size of the buffer `timebuf` to ensure safe string operations. |

**Returns**

This function does not return a value. The formatted relative time string is stored in `timebuf`.

**Notes**

The function calculates the difference between the given timestamp and the current time, then formats it into a human-readable string. It supports various time units such as seconds, minutes, hours, days, weeks, months, and years.

---

(start_msectimer)=
## `start_msectimer()`

`start_msectimer()` initializes a millisecond-resolution timer and returns the expiration timestamp.

```C
uint64_t start_msectimer(
    uint64_t milliseconds
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `miliseconds` | `uint64_t` | The duration of the timer in milliseconds. A value of 0 disables the timer. |

**Returns**

Returns the absolute expiration timestamp in milliseconds since an unspecified epoch. If `miliseconds` is 0, the function returns 0.

**Notes**

Use [`test_msectimer()`](#test_msectimer) to check if the timer has expired.

---

(start_sectimer)=
## `start_sectimer()`

`start_sectimer()` initializes a timer by adding the specified number of seconds to the current system time and returns the future timestamp.

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
## `t2timestamp()`

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
| `bfsize` | `int` | Size of the buffer to ensure safe string formatting. |
| `t` | `time_t` | Time value to be converted into a timestamp. |
| `local` | `BOOL` | Flag indicating whether to use local time (`TRUE`) or UTC (`FALSE`). |

**Returns**

Returns a pointer to the formatted timestamp string stored in `bf`.

**Notes**

The function uses `strftime()` to format the timestamp in ISO 8601 format with timezone information.

---

(test_msectimer)=
## `test_msectimer()`

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

The function uses `time_in_miliseconds_monotonic()` to obtain the current monotonic time and compares it with `value`.

---

(test_sectimer)=
## `test_sectimer()`

Checks if the given `value` time has elapsed compared to the current system time.

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
## `time_in_seconds()`

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
## `tm2timestamp()`

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

(tm_to_time_t)=
## `tm_to_time_t()`

`tm_to_time_t()` converts a `struct tm` representation of a date and time into a `time_t` value, assuming UTC and without normalizing `tm_wday` or `tm_yday`.

```C
time_t tm_to_time_t(
    const struct tm *tm
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tm` | `const struct tm *` | Pointer to a `struct tm` containing the broken-down time representation. |

**Returns**

Returns the corresponding `time_t` value representing the given time in seconds since the Unix epoch (1970-01-01 00:00:00 UTC). Returns `-1` if the input is out of range.

**Notes**

This function does not perform normalization of `tm_wday` or `tm_yday`, and it assumes the input time is in UTC.

---

(get_days_range)=
## `get_days_range()`

*Description pending — signature extracted from header.*

```C
time_range_t get_days_range(
    time_t t,
    int range,
    const char *TZ
);
```

---

(get_hours_range)=
## `get_hours_range()`

*Description pending — signature extracted from header.*

```C
time_range_t get_hours_range(
    time_t t,
    int range,
    const char *TZ
);
```

---

(get_months_range)=
## `get_months_range()`

*Description pending — signature extracted from header.*

```C
time_range_t get_months_range(
    time_t t,
    int range,
    const char *TZ
);
```

---

(get_weeks_range)=
## `get_weeks_range()`

*Description pending — signature extracted from header.*

```C
time_range_t get_weeks_range(
    time_t t,
    int range,
    const char *TZ
);
```

---

(get_years_range)=
## `get_years_range()`

*Description pending — signature extracted from header.*

```C
time_range_t get_years_range(
    time_t t,
    int range,
    const char *TZ
);
```

---

(gmtime2timezone)=
## `gmtime2timezone()`

*Description pending — signature extracted from header.*

```C
time_t gmtime2timezone(
    time_t t,
    const char *tz,
    struct tm *ltm,
    time_t *offset
);
```

---

(time_in_milliseconds)=
## `time_in_milliseconds()`

*Description pending — signature extracted from header.*

```C
uint64_t time_in_milliseconds(void);
```

---

(time_in_milliseconds_monotonic)=
## `time_in_milliseconds_monotonic()`

*Description pending — signature extracted from header.*

```C
uint64_t time_in_milliseconds_monotonic(void);
```

---

