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
ISO-8601 strings and `yday`. A time no calendar holds, in UTC or in the local
zone, prints *"timestamp too big"* with exit code -1: up to 7.29.3
`time2date 67768036191676799` in a zone east of UTC crashed.

## See also

- [`time2range`](time2range.md) — period start/end boundaries.
- [`utils/c/time2date/README.md`](https://github.com/artgins/yunetas/blob/7.30.2/utils/c/time2date/README.md).
