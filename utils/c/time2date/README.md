# time2date

time2date converts a Unix timestamp to an ASCII date and time string, and a date string to a timestamp. The timestamp is a `time_t`, with optional millisecond precision. The tool uses the local timezone.

## Usage

```bash
time2date <timestamp|date-string>
```

Run `time2date --help` to see the flags.

```bash
time2date -z Europe/Madrid 1792573200
# gmtime of 1792573200 (...): UTC 2026-10-21T09:00:00+0000, Europe/Madrid 2026-10-21T11:00:00+0200, LOCAL ..., yday 293
```

To give a time before the epoch, write it after `--`: `time2date -- -5`.

The tool refuses these inputs:

- A date that is not a date (`time2date 2026-10-01Tgarbage`).
- A number that is not a JSON integer (`abc`, `007`, `0x10`), or that is out of the range of a `time_t`.
- A zone that libc cannot read (`-z Nowhere/Zone`). An ERROR names it.

For a refused input, the tool prints no answer and the exit code is -1. Up to 7.30.14, the tool printed 1970 or the time in UTC for these inputs, with exit code 0. It also read `007` as octal and `0x10` as hex.
