(util-tr2list)=
# `tr2list`

List the records of a **timeranger2** topic, with rich filtering, field
selection, metadata, and local-time output. The topic/database are deduced from
the `PATH`.

## Usage

```bash
tr2list PATH [options]
```

```bash
tr2list /yuneta/store/.../topic --from-rowid -10            # last 10 records
tr2list /yuneta/store/.../topic --key mykey --mode table
tr2list /yuneta/store/.../topic --follow                    # tail -f
```

## Options

| Option | Purpose |
|--------|---------|
| `--key` / `--not-key` / `--rkey` | Match by key (exact / negated / regex) |
| `--from-t` / `--to-t` | Record-time range |
| `--from-tm` / `--to-tm` | Message-time range |
| `--from-rowid` / `--to-rowid` | Rowid range (negative = from the end) |
| `--user-flag-set` / `--system-flag-set` (+ `…-not-set`) | Flag filters |
| `--filter` | JSON dict of field matches; each key is a PATH (see below) |
| `--fields` | Select output fields |
| `--mode form\|table` | Output layout |
| `--show_md2` | Include `__md2__` metadata |
| `--print-local-time` / `-t` | Local timestamps |
| `--recursive` / `-r` | Walk subtopics |
| `--follow` / `-F` | Live tail (not with `-r`) |
| `--dry-run` / `-n` | Resolve and print the plan only |
| `--list-databases` | List databases under the path |
| `--verbose` / `-l` | Verbosity level |

## Time bounds

`--from-t`, `--to-t`, `--from-tm` and `--to-tm` take an integer or a date.
An integer is used as it is, in the topic's own unit: seconds, or
milliseconds for `t` on an `sf_t_ms` topic and for `tm` on an `sf_tm_ms` one.
A date (any form `approxidate` reads: `"2026-10-01"`, `"2 days ago"`,
`"yesterday"`) is read in seconds and taken to that unit, topic by topic, so
one `-r` listing over topics of different units cuts them all at the same
instant:

```bash
# the records appended on 2026-10-01 (UTC), whatever the unit of t
tr2list /yuneta/store/x/topic1 --from-t="2026-10-01 00:00:00 +0000" --to-t="2026-10-01 23:59:59 +0000"

# the same bound as an integer: seconds on a topic in seconds...
tr2list /yuneta/store/x/topic1 --from-t=1790812800
# ...milliseconds on an sf_t_ms topic
tr2list /yuneta/store/x/topic_ms --from-t=1790812800000
```

A **negative** integer counts back from the last record of each key, as
timeranger2 reads a negative bound:

```bash
# the last day of every key (a topic in seconds)
tr2list /yuneta/store/x/topic1 --from-t=-86400
```

Up to 7.27.4 a date stayed in seconds on a topic in milliseconds, and selected
nothing or everything there. Up to 7.27.6 a negative integer was read as a
date, and `approxidate` read `-86400` as NOW (and `-1` as the first day of
the month), with no word. `--dry-run` shows the dates resolved in seconds, and
a negative bound as relative to the last record of each key; an integer out
of range is refused (*"TIME out of range"*).

An integer TIME is exactly `-?[0-9]+`. A text of spaces, signs and digits
only that is not one is refused, not read as a date, wherever its signs and
spaces are: up to 7.27.6 `approxidate` read `+5`, `" -5"`, `5+`, `5-` and
`-5-` as the 5th of this month, `"5 5"` and `5+5` as the 5th of May, and a
lone `-` or an empty TIME as NOW, with no word. A `-` between two digits makes
the text a date, and a date keeps working (`2026-10-01`; `1.day` has a
letter):

```bash
tr2list /yuneta/store/x/topic1 --from-t=+5
# -> TIME is not an integer -?[0-9]+ nor a date: '+5'
tr2list /yuneta/store/x/topic1 --from-t=5-
# -> TIME is not an integer -?[0-9]+ nor a date: '5-'
tr2list /yuneta/store/x/topic1 --from-t='5 5'
# -> TIME is not an integer -?[0-9]+ nor a date: '5 5'
tr2list /yuneta/store/x/topic1 --from-t=2026-10-01 --dry-run
# -> from_t = ... 2026-10-01T...
```

## Filtering by field

`--filter` takes a JSON dict. A record is listed when every key of the dict,
read as a **path** into the record (segments joined by `` ` ``), holds exactly
the value given. A key without `` ` `` is a top-level field:

```bash
# records whose top-level field "state" is "on"
tr2list /yuneta/store/x/topic1 --filter='{"state": "on"}'

# records whose nested field device.model is "T-100", AND state is "on"
tr2list /yuneta/store/x/topic1 --filter='{"device`model": "T-100", "state": "on"}'
```

For the record `{"state": "on", "device": {"model": "T-100", "fw": 3}}` both
filters match. The value is compared whole: `{"device": {"model": "T-100"}}`
does NOT match it, because `device` also holds `fw` — name the leaf with a
path instead.

## See also

- [`tr2keys`](tr2keys.md), [`tr2search`](tr2search.md) — keys-only / content search.
- [timeranger2 + treedb crash course](../../../yunos/c/yuno_agent/YUNO_TREEDB.md).
- [`utils/c/tr2list/README.md`](https://github.com/artgins/yunetas/blob/7.28.2/utils/c/tr2list/README.md).
