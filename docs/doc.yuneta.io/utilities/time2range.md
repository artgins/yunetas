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
| `--range` / `-r` | Integer offset (for example previous/next period) |
| `--TZ` / `-z` | Time zone |

Prints `Start: … End: …`.

## See also

- [`time2date`](time2date.md) — single-timestamp conversion.
- [`utils/c/time2range/README.md`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/time2range/README.md).
