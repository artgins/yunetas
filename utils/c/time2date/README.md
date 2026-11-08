# time2date

Bidirectional converter between Unix timestamps (`time_t`, with optional millisecond precision) and ASCII date/time strings. Respects the local timezone.

## Usage

```bash
time2date <timestamp|date-string>
```

Run `time2date --help` for available flags.

```bash
time2date -z Europe/Madrid 1792573200
# gmtime of 1792573200 (...): UTC 2026-10-21T09:00:00+0000, Europe/Madrid 2026-10-21T11:00:00+0200, LOCAL ..., yday 293
```

A date that is no date (`time2date 2026-10-01Tgarbage`), a number that is no
JSON integer (`abc`, `007`, `0x10`) or is out of the range of a `time_t`, and a
zone libc will not read (`-z Nowhere/Zone`, said by its ERROR) are refused: no
answer, exit code -1. Up to 7.30.14 they printed 1970, or the time in UTC, with
exit code 0, and `007` was read as octal, `0x10` as hex. A time before the epoch
is written after `--`: `time2date -- -5`.
