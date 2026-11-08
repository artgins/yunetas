# time2range

Convert a Unix timestamp (`time_t`) into a human-friendly time range (seconds, minutes, hours, days, weeks, months, years) with timezone support.

Useful for quick conversions when debugging logs, computing range boundaries, or scripting around timeranger2 data.

## Usage

```bash
time2range -t <time_t> -p <type> [-r <range>] [-z <TZ>]
```

The timestamp is the `-t`/`--time_t` option (not a positional). `-t` and
`-p`/`--type` (`hours`|`days`|`weeks`|`months`|`years`) are required.
Run `time2range --help` for the full option list.

```bash
time2range -t 2026-10-01T10:30:00Z -p hours -r 2 -z Europe/Madrid
# Start: 2026-10-01T12:00:00+0200, End: 2026-10-01T14:00:00+0200
```

`-r` must be a JSON integer, as the command parser of a yuno reads one
(`-r abc`, `-r " 5"`, `-r +5` and `-r 05` are refused; one outside the range
of an `int` is refused as out of range). A range whose start or
end no calendar holds, and any range that logs an ERROR (an unknown zone,
`-z Nowhere/Zone`, said once), is refused with exit code -1 and no range. With `-z`, hours are counted as
the zone counts them, so the time grows with the years the range covers: a
year takes 0.2 ms.
