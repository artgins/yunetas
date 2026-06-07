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
| `--time_t` / `-t` | Epoch seconds (or date string) — **required** |
| `--type` / `-p` | `hours` \| `days` \| `weeks` \| `months` \| `years` — **required** |
| `--range` / `-r` | Integer offset (e.g. previous/next period) |
| `--TZ` / `-z` | Time zone |

Prints `Start: … End: …`.

## See also

- [`time2date`](time2date.md) — single-timestamp conversion.
- [`utils/c/time2range/README.md`](https://github.com/artgins/yunetas/blob/7.5.2/utils/c/time2range/README.md).
