(util-time2date)=
# `time2date`

Convert a Unix `time_t` (optionally milliseconds) to ASCII date/time, and
vice-versa, in UTC, local and a given time zone.

## Usage

```bash
time2date [TIME_T]            # no arg => current time; also accepts a date string
```

| Option | Purpose |
|--------|---------|
| `[TIME_T]` | Epoch seconds as a JSON integer (`1792573200`, `0`, `-5`), or a date with a `T` (`2026-10-01T10:00:00Z`); optional positional, now when absent |
| `--TZ` / `-z` | Time zone for the conversion |
| `--miliseconds` / `-m` | Treat the input as milliseconds |

Prints the timestamp (and its hex form) plus UTC + local (and named-TZ)
ISO-8601 strings and `yday`. A time no calendar holds, in UTC or in the local
zone, prints *"timestamp too big"* with exit code -1: up to 7.29.3
`time2date 67768036191676799` in a zone east of UTC crashed.

```bash
time2date -z Europe/Madrid 1792573200
# gmtime of 1792573200 (...): UTC 2026-10-21T09:00:00+0000, Europe/Madrid 2026-10-21T11:00:00+0200, LOCAL ..., yday 293
time2date 2026-10-01T10:00:00Z
# gmtime of 1790848800 (...): UTC 2026-10-01T10:00:00+0000, LOCAL ..., yday 273
```

A refusal prints no answer and exits with code -1 (255 in the shell):

- A date (a text with a `T`) that is no date, read as
  [`parse_date_basic()`](<#parse_date_basic>) reads one:
  `time2date 2026-10-01Tgarbage` prints
  *"TIME_T refused, a text that is no date: '2026-10-01Tgarbage'"*. Up to
  7.30.14 it printed the epoch (1970) with exit code 0.
- A zone that libc will not read (`-z Nowhere/Zone`, `-z zone.tab`; see
  [`gmtime2timezone()`](<#gmtime2timezone>)): the ERROR *"Unknown time zone:
  taken as UTC"* is printed once on stdout. Up to 7.30.14 the ERROR went
  nowhere, and the time was printed in UTC with the name of that zone, exit
  code 0.
- A number (a text with no `T`) that is no JSON integer,
  `-?(0|[1-9][0-9]*)`, as the command parser of a yuno reads one:
  `time2date abc`, `007`, `0x10`, `" 5"`, `+5`, `1e3` print
  *"TIME_T refused, it is no integer: 'abc'"*, and one outside the 64 bits of
  a `time_t` (`9223372036854775808`) *"TIME_T refused, out of range: '...'"*.
  Up to 7.30.14 it was read with `strtoull(arg, NULL, 0)`: `abc` printed
  1970 with exit code 0, `007` was octal (7) and `0x10` hex (16).

A time before the epoch is a negative number, written after `--` so that it
is not taken as an option; with `-m` it is divided as a signed number (up to
7.30.14 as an unsigned one):

```bash
time2date -- -5           # UTC 1969-12-31T23:59:55+0000
time2date -m -- -5000     # the same second
```

## See also

- [`time2range`](time2range.md) — period start/end boundaries.
- [`utils/c/time2date/README.md`](https://github.com/artgins/yunetas/blob/7.30.20/utils/c/time2date/README.md).
