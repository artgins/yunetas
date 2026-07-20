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
| `--from-t` / `--to-t` | Time range |
| `--limits` | Show configured limits |
| `--recursive` / `-r` | Walk subtrees |
| `--raw` | Raw output |
| `--verbose` / `-l` | Verbosity level |

## See also

- [`ystats`](ystats.md) — live stats subscription on a running yuno.
- [`utils/c/stats_list/README.md`](https://github.com/artgins/yunetas/blob/7.8.5/utils/c/stats_list/README.md).
