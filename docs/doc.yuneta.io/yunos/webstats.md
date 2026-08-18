(yuno-webstats)=
# `webstats`

Reads the logs of the node's web server (nginx or openresty), makes a daily
report of who used the sites and what went wrong, and mails it through
[`emailsender`](#yuno-emailsender).

One yuno per node. Each node reports on its own logs and sends its own mail.

A sealed node has no SSH, so a tool you must log in to run is a task that stops
working the day the node is sealed. This one runs inside the node and sends the
answer out.

**Full design:** [`yunos/c/webstats/README.md`](https://github.com/artgins/yunetas/blob/7.13.0/yunos/c/webstats/README.md)

## Architecture

```
C_WEBSTATS
    C_TIMER         <- the daily schedule
    C_LOG_READER    <- one per file being read (created, used, destroyed)
```

`C_LOG_READER` turns one file into events (`EV_LOG_LINES`, `EV_LOG_EOF`,
`EV_LOG_ERROR`) and knows nothing about nginx.

The two continuations of a run are not timers. A file ends inside the reader's
own publish stack, so the reader cannot be destroyed there, and the work has to
cross a cycle of the loop: `C_WEBSTATS` posts `EV_NEXT_FILE` to itself and the
reader posts `EV_READ_CHUNK` to itself, with
[`gobj_post_event()`](../api/gobj/events_state.md#gobj_post_event). The only
timer left is the schedule, which measures a real time. `C_WEBSTATS` parses the lines,
keeps the counters, writes the daily record and hands the mail over.

## The line sets the day, not the file

The yuno keeps **no read offset** and does not hook into `logrotate`. To report
day D it reads `access.log` and `access.log.1` and keeps the lines whose own
`[$time_local]` falls inside that day.

So it is idempotent (`report-day` can be run again), it survives being down for
a day, and it does not care when `logrotate.timer` fires.

## What it measures

**Visitors** lead the report. A visitor is an address that asked for a piece of
the page (`.js` or `.css`) **and got it** (2xx), and whose user agent carries no
crawler mark. A browser fetches the page and its sub-resources. A scanner
wearing a browser user agent asks for one URL and leaves. Measured on one node
on 2026-08-06: 1346 addresses claimed to be a browser and 71 ever fetched a
script.

**New visitors** are the ones whose fingerprint appears in none of the last
`new_visitor_days` stored days. The record keeps fingerprints, never addresses.

Also: totals and status classes, per hour, per vhost, the top paths / 404s /
clients / agents / referrers, every 5xx whole, probes (counted, never banned —
that is `fail2ban`'s job), a latency histogram, and the error log grouped by
signature.

## Configuration

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `access_log_paths` | both trees | Access logs. The yuno also reads each `<path>.1` |
| `error_log_paths` | both trees | Error logs, same rule |
| `report_hour` / `report_minute` | 6 / 0 | Local time of the daily run |
| `send_email` | `true` | `false` keeps the record and skips the mail |
| `email_to` | — | Destination |
| `email_service` | `emailsender` | Service that sends |
| `top_n` | 20 | Rows per top table |
| `internal_networks` | — | Address prefixes not counted as clients |
| `asset_extensions` | `.js`, `.css` | What a browser fetches to draw a page |
| `bot_agents` | the usual marks | A user agent that says it is a crawler |
| `new_visitor_days` | 30 | History that decides whether a visitor is new |
| `visitor_salt` | — | Salt of the visitor fingerprint |
| `keep_days` | 400 | Days of aggregates kept |

## Installing it on a node

`webstats` goes in the **utilities batch of the node's operations repo**, next
to [`emailsender`](#yuno-emailsender) and [`logcenter`](#yuno-logcenter) — the
same realm the node's `create-*.sh` script builds. That is what makes a node
rebuilt from zero come up reporting instead of waiting for somebody to remember
it:

```
{"command": "-install-binary id=webstats content64=$$(webstats)"}
{"command": "-create-config id=webstats.<node> content64=$$(./webstats.<node>.json)"}
{"command": "-create-yuno id=3 realm_id=<utilities realm> yuno_role=webstats yuno_name=<node> must_play=1 yuno_tag=util"}
```

The binary comes from the package: the `.deb` and the `.rpm` ship
`outputs/yunos/` whole, so `install-binary` finds it even on a node that
carries no SDK sources.

**Name the tree the node really serves with.** The default reads both the nginx
and the openresty tree. A node runs one of them, and the other is usually a
leftover whose rotated files are read whole every day to contribute nothing —
on one node that was 111000 lines a day for zero rows. Set `access_log_paths`
and `error_log_paths` to the live tree.

## Commands

| Command | Description |
|---------|-------------|
| `help` | Command help |
| `analyze-now` | Build the report of yesterday, now |
| `report-day report_date=YYYY-MM-DD [send=1]` | Rebuild any day still on disk |
| `get-report report_date=YYYY-MM-DD` | The stored record of a day |
| `list-reports` | The days held in the store |
| `list-sources` | The files it will read, and whether each is readable now |
| `preview-report report_date=YYYY-MM-DD` | The mail body of a stored day, without sending it |

The day parameter is `report_date` and not `date`: `command-yuno` uses its whole
kw to select the yuno, so a parameter named like a field of the yuno record
matches no yuno and answers *"Yuno not found"*.

## Persistence

One TimeRanger2 topic, `daily_stats`, keyed by the date. Only the aggregates —
the rotated `.gz` files are the archive. A day reported twice keeps both
records and the newest answers, which is what makes `report-day` repeatable.

A run that reads nothing does **not** replace a run that read something: it is
abandoned with a warning. Without that, rebuilding a day whose log already
rotated away overwrites a good record with an empty one.

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_WEBSTATS` | `parse` | The lines the parser rejected, with the line |
| `C_WEBSTATS` | `report` | The built record before it is sent |
| `C_LOG_READER` | `read` | File opened, chunks, EOF |

Enable with
`ycommand command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=C_WEBSTATS set=1 level=parse`.

`parse` is the one that matters: it is the only way to see a log format change
that the parser silently tolerates.
