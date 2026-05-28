# tr2list

List records from a **timeranger2** topic. Supports filtering by time range and record id, selecting specific fields, printing metadata, and showing timestamps in local time.

## Usage

```bash
tr2list <PATH> [options]
```

`PATH` is a positional argument (the topic/database are deduced from it).
Run `tr2list --help` for all filters (`--from-t`, `--to-t`, `--from-rowid`, `--fields`, `--show_md2`, …).
