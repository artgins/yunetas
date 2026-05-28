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
