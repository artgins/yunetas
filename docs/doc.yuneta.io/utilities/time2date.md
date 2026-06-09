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
| `[TIME_T]` | Epoch seconds, or an ISO-ish date string (optional positional) |
| `--TZ` / `-z` | Time zone for the conversion |
| `--miliseconds` / `-m` | Treat the input as milliseconds |

Prints the timestamp (and its hex form) plus UTC + local (and named-TZ)
ISO-8601 strings and `yday`.

## See also

- [`time2range`](time2range.md) — period start/end boundaries.
- [`utils/c/time2date/README.md`](https://github.com/artgins/yunetas/blob/7.5.10/utils/c/time2date/README.md).
