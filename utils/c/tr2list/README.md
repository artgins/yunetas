# tr2list

List records from a **timeranger2** topic. Supports filtering by time range and record id, selecting specific fields, printing metadata, and showing timestamps in local time.

## Usage

```bash
tr2list <PATH> [options]
```

`PATH` is a positional argument (the topic/database are deduced from it).
Run `tr2list --help` for all filters (`--from-t`, `--to-t`, `--from-rowid`, `--fields`, `--show_md2`, …).

A TIME (`--from-t`, `--to-t`, `--from-tm`, `--to-tm`) is read by the rule timeranger2 reads a time bound with, [`tranger2_time_of_text()`](https://doc.yuneta.io/timeranger2#tranger2_time_of_text), and passed to it as text: each topic reads it in the unit of its axis. The text of a JSON integer is that integer (`--from-t=-86400` is the last day of each key); a text written as another number is refused (`' 5'`, `'007'`, `'10.5'`, `'1,5'`, `'5 5'`); any other text is a date (`--from-t='2026-10-01T00:00:00+0000'`, `--from-t='1 day ago'`), and one that is no date is refused, saying why:

```
$ tr2list /yuneta/store/db/topic --from-t=1,5
TIME of --from-t refused, a number that is not the text of a JSON integer: '1,5'
```

`tr2search`, `stats_list`, `emu_device` and `time2range` read a TIME the same way (up to 7.29.3 each had a reader of its own: `007` was 7, and `1,5` May 1).
