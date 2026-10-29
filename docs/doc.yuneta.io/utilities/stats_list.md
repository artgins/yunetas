(util-stats_list)=
# `stats_list`

List **persisted statistics** (timeranger2-backed) filtered by group, variable,
metric, time-unit and time range.

## Usage

```bash
stats_list --path <path> --group <group> [options]
```

All inputs are flags — `stats_list` takes **no positional arguments**.

| Option | Purpose |
|--------|---------|
| `--path` / `-a` | Stats store path |
| `--group` / `-b` | Stats group |
| `--variable` | Variable name |
| `--metric` | Metric name |
| `--units SEC\|MIN\|HOUR\|MDAY\|MON\|YEAR\|WDAY\|YDAY\|CENT` | Time bucket |
| `--from-t` / `--to-t` | Time range, in seconds, read as [`tranger2_time_of_text()`](<#tranger2_time_of_text>) reads a time: the text of a JSON integer (`1790812800`), or a date (`2026-10-01`, `-1d`, `"2 hours ago"`). A number of another shape (`007`, `" 5"`, `1,5`) or a text that is no date is refused, saying why: `stats_list ... --from-t=007` answers *"TIME of --from-t refused, a number that is not the text of a JSON integer: '007'"*. Up to 7.29.3 `007` was 7 and `" 5"`, `1,5` dates |
| `--limits` | Show configured limits |
| `--recursive` / `-r` | Walk subtrees |
| `--raw` | Raw output |
| `--verbose` / `-l` | Verbosity level |

## See also

- [`ystats`](ystats.md) — live stats subscription on a running yuno.
- [`utils/c/stats_list/README.md`](https://github.com/artgins/yunetas/blob/7.29.3/utils/c/stats_list/README.md).
