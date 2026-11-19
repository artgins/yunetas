# tr2list

`tr2list` lists the records of a timeranger2 topic. You can filter by time range and record id, select fields, print metadata, and show timestamps in local time.

## Usage

```bash
tr2list <PATH> [options]
```

`PATH` is a positional argument. The tool gets the topic and the database from it.
Run `tr2list --help` to see all the filters (`--from-t`, `--to-t`, `--from-rowid`, `--fields`, `--show_md2`, …).

The tool gives a TIME (`--from-t`, `--to-t`, `--from-tm`, `--to-tm`) as text to [`tranger2_time_of_text()`](https://doc.yuneta.io/timeranger2#tranger2_time_of_text). That function is the rule that timeranger2 uses to read a time bound. Each topic reads the TIME in the unit of its axis. The rules are:

- The text of a JSON integer is that integer. For example, `--from-t=-86400` is the last day of each key.
- The tool refuses a text written as another number: `' 5'`, `'007'`, `'10.5'`, `'1,5'`, `'5 5'`.
- Any other text is a date (`--from-t='2026-10-01T00:00:00+0000'`, `--from-t='1 day ago'`). If the text is not a date, the tool refuses it and says why:

```
$ tr2list /yuneta/store/db/topic --from-t=1,5
TIME of --from-t refused, a number that is not the text of a JSON integer: '1,5'
```

`tr2search`, `stats_list`, `emu_device` and `time2range` read a TIME the same way. Up to 7.29.3, each tool had a reader of its own: `007` was 7, and `1,5` was day 1 of month 5.
