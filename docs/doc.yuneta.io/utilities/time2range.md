(util-time2range)=
# `time2range`

Given a `time_t`, compute the start/end boundaries of the surrounding time
period (the hour, day, week, month or year it falls in).

## Usage

```bash
time2range -t <time_t> -p <type> [-r <range>] [-z <TZ>]
```

The timestamp is the `-t` **option**, not a positional argument.

| Option | Purpose |
|--------|---------|
| `--time_t` / `-t` | Epoch seconds, or a date, read as [`tranger2_time_of_text()`](<#tranger2_time_of_text>) reads a time: `-t 1790812800`, `-t 2026-10-01T00:00:00Z`. A number of another shape (`007`, `1,5`) or a text that is no date is refused, saying why (*"TIME refused, a text that is no date: 'garbage'"*); up to 7.29.3 `007` was 7, and `1,5` or `garbage` whatever the stack held: a text the old reader (`parse_date_basic()`) could not read left the time uninitialised, a year in the millions that changed from one build to the next — **required** |
| `--type` / `-p` | `hours` \| `days` \| `weeks` \| `months` \| `years` — **required** |
| `--range` / `-r` | Number of periods from the one of `-t`, negative to count back. An integer: `-r abc` or `-r 1x` is refused (*"RANGE refused, it is no integer: 'abc'"*); up to 7.29.3 it was a range of 0. Without it the range is 0 |
| `--TZ` / `-z` | Time zone, a `TZ` text: see [`gmtime2timezone()`](<#gmtime2timezone>). One that libc will not read as a zone (`-z Nowhere/Zone`, `-z zone.tab`) gives the range in UTC with *"Unknown time zone: taken as UTC"* on stdout; up to 7.29.3 it was UTC in silence |

Prints `Start: … End: …`.

```bash
time2range -t 2026-10-01T10:30:00Z -p hours -r 2 -z Europe/Madrid
# Start: 2026-10-01T12:00:00+0200, End: 2026-10-01T14:00:00+0200
time2range -t 2026-10-01T10:30:00Z -p days -r -1
# Start: 2026-10-01T00:00:00+0000, End: 2026-09-30T00:00:00+0000
```

A range whose end no calendar holds is refused with exit code -1 (*"No range: no calendar holds its end"*): up to 7.29.3 `-p years -r 2147483647` printed an end some two billion years back. With `-z`, hours are counted as the zone counts them, so the time grows with the years the range covers: a year takes 0.2 ms and 245146 years, the most `-r` can ask for, take minutes. Without `-z` it is one step.

## See also

- [`time2date`](time2date.md) — single-timestamp conversion.
- [`utils/c/time2range/README.md`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/time2range/README.md).
