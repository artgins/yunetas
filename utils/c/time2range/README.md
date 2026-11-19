# time2range

`time2range` converts a Unix timestamp (`time_t`) to a time range (hours, days, weeks, months, years), with time zone support.

Use it to convert a time when you debug logs, to calculate the limits of a range, or in scripts that read timeranger2 data.

## Usage

```bash
time2range -t <time_t> -p <type> [-r <range>] [-z <TZ>]
```

You must give `-t` and `-p`/`--type` (`hours`|`days`|`weeks`|`months`|`years`).
The timestamp is the value of the `-t`/`--time_t` option. It is not a positional argument.
Run `time2range --help` to see all the options.

```bash
time2range -t 2026-10-01T10:30:00Z -p hours -r 2 -z Europe/Madrid
# Start: 2026-10-01T12:00:00+0200, End: 2026-10-01T14:00:00+0200
```

`-r` must be a JSON integer, the same as the command parser of a yuno reads.
The tool refuses `-r abc`, `-r " 5"`, `-r +5` and `-r 05`. It refuses a value
outside the range of an `int` as out of range.

The tool also refuses a range if no calendar holds its start or its end. It
refuses any range that logs an ERROR, for example an unknown zone
(`-z Nowhere/Zone`). The tool logs that error one time. When the tool refuses
a range, it exits with code -1 and prints no range.

With `-z`, the tool counts the hours as the zone counts them. Thus the time to
calculate a range increases with the number of years that the range covers.
One year takes 0.2 ms.
