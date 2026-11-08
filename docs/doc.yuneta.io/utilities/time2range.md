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
| `--range` / `-r` | Number of periods from the one of `-t`, negative to count back. A JSON integer, `-?(0\|[1-9][0-9]*)`, as the command parser of a yuno reads one: `-r 5`, `-r -1`, `-r 0`. Any other text is refused (*"RANGE refused, it is no integer: ' 5'"*): `-r abc`, `-r 1x`, `-r " 5"`, `-r +5`, `-r 05`, `-r 5.0`. Up to 7.29.3 `abc` was a range of 0, and up to 7.30.14 `" 5"`, `+5` and `05` were 5. Without it the range is 0 |
| `--TZ` / `-z` | Time zone, a `TZ` text: see [`gmtime2timezone()`](<#gmtime2timezone>). One that libc will not read as a zone (`-z Nowhere/Zone`, `-z zone.tab`) is refused: the ERROR *"Unknown time zone: taken as UTC"* is printed once on stdout, and no range. Up to 7.29.3 it was UTC in silence, and up to 7.30.14 the range in UTC with that ERROR three times and exit code 0 |

Prints `Start: … End: …`.

```bash
time2range -t 2026-10-01T10:30:00Z -p hours -r 2 -z Europe/Madrid
# Start: 2026-10-01T12:00:00+0200, End: 2026-10-01T14:00:00+0200
time2range -t 2026-10-01T10:30:00Z -p days -r -1
# Start: 2026-10-01T00:00:00+0000, End: 2026-09-30T00:00:00+0000
```

A refusal prints no range and exits with code -1 (255 in the shell). Every ERROR the range logs is one: an unknown zone, as above, or a time no calendar holds. A range whose start or end no calendar holds is refused with *"No range: no calendar holds its start or its end"*: up to 7.29.3 `-p years -r 2147483647` printed an end some two billion years back. `-t 67768036191676799 -p days -r 1`, the last second of the calendar and a day more, is such a range. With `-r 0` a `-t` that no calendar holds (`-t 67768036191676800 -p days -r 0`) is refused with its ERROR *"no calendar holds the time: range 0"*: up to 7.30.14 it printed 1970 with exit code 0. The first day of the epoch is a range like any other: `-t 1 -p days -r 0` prints `Start: 1970-01-01T00:00:00+0000, End: 1970-01-01T00:00:00+0000`.

```bash
time2range -t 2026-10-01T10:30:00Z -p days -r 1 -z Nowhere/Zone; echo $?
# ERROR: {... "msg": "Unknown time zone: taken as UTC", "tz": "Nowhere/Zone" }
# 255
time2range -t 2026-10-01T10:30:00Z -p days -r +5; echo $?
# RANGE refused, it is no integer: '+5'
# 255
``` With `-z`, hours are counted as the zone counts them, so the time grows with the years the range covers: a year takes 0.2 ms and 245146 years, the most `-r` can ask for, take minutes. Without `-z` it is one step.

## See also

- [`time2date`](time2date.md) — single-timestamp conversion.
- [`utils/c/time2range/README.md`](https://github.com/artgins/yunetas/blob/7.30.14/utils/c/time2range/README.md).
