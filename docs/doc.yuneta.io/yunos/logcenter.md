(yuno-logcenter)=
# `logcenter`

Centralized log collector for a node. Every yuno ships its log records to
`logcenter` over UDP (the peer of the kernel's `to_udp` log handler, default
`udp://127.0.0.1:1992`). `logcenter` counts them by priority, writes them to
size/disk-guarded rotatory files, aggregates a summary report, watches free
disk/memory, and can e-mail the report or even restart the node on a queue
alarm.

## Architecture

```
C_LOGCENTER            <- counting, rotation, summary, monitoring, self-heal
    > C_GSS_UDP_S      <- UDP server (udp://127.0.0.1:1992), receives log records
    > C_TIMER          <- periodic disk/memory monitor
```

Summary e-mails are sent by publishing `EV_SEND_EMAIL` to the `emailsender`
service (see [`emailsender`](yuno-emailsender)).

### Wire format

Each UDP datagram from the `to_udp` handler is:

```
<priority:1 char><sequence:8 hex><message bytes...><crc:8 hex>
```

`logcenter` verifies the trailing CRC, tracks the sequence (gaps are only
logged under the `debug` trace), strips both, then files the message. If the
message body is JSON (`{...}`) it is also folded into the summary tree.

## What it does with each record

1. **Counts** it by syslog priority — Alert, Critical, Error, Warning, Info,
   Debug, Audit, Monitor.
2. **Writes** it to a rotatory log file under the yuno's `logs/` directory,
   named from `log_filename` (mask `DD/MM/CCYY-W-ZZZ`). Rotation is bounded by
   `max_rotatoryfile_size` and pauses when free disk drops below
   `min_free_disk`.
3. **Aggregates** JSON records into a summary grouped by `msgset` → `msg` →
   count (separately for errors, warnings, infos).

## Configuration

Inspect the effective config at runtime with
`ycommand command-yuno id=<id> service=__yuno__ command=view-config`.

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `url` | `udp://127.0.0.1:1992` | UDP server to receive log records |
| `from` | *(required)* | From address for the summary e-mail |
| `to` | *(required)* | Recipient(s) of the summary e-mail (`,`/`;` separated) |
| `subject` | `Log Center Summary` | Summary e-mail subject |
| `log_filename` | `W.log` | Rotatory file name (mask `DD/MM/CCYY-W-ZZZ`) |
| `max_rotatoryfile_size` | `600` | Max log file size (MB) before rotating |
| `rotatory_bf_size` | `10` | Rotatory write buffer (MB) |
| `min_free_disk` | `20` | Stop writing / warn below this % free disk |
| `min_free_mem` | `20` | Warn below this % free RAM |
| `send_summary_disabled` | `false` | Suppress summary e-mails |
| `restart_on_alarm` | `false` | Run `restart_yuneta_command` on a queue alarm |
| `restart_yuneta_command` | `yshutdown … ; yuneta_agent --start …` | Command run on alarm |
| `queue_restart_limit` | `0` | Only restart if reported `queue_size` ≥ this (cmd enforces ≥ 10000) |
| `timeout_restart_yuneta` | `3600` | Min seconds between restarts (must be ≥ 3600) |
| `timeout` | `1000` | Monitor tick (ms) |

## Commands

| Command | Parameters | Description |
|---------|------------|-------------|
| `display-summary` | — | Return the summary report (includes internal errors) |
| `send-summary` | — | E-mail the summary report now (via `emailsender`) |
| `enable-send-summary` / `disable-send-summary` | — | Toggle `send_summary_disabled` |
| `reset-counters` | — | Zero the priority counters and summary trees |
| `search` | `text`, `maxcount` | Search the stored log records |
| `tail` | `lines` | Output the last `lines` log records (default 100) |
| `restart-yuneta-on-queue-alarm` | `enable`, `queue_restart_limit`, `timeout_restart_yuneta` | Configure the queue-alarm self-heal |
| `help` | `cmd`, `level` | Command help |

The summary report (from `display-summary` / `send-summary`) looks like:

```json
{
    "Global Counters": { "Alert": 0, "Critical": 0, "Error": 2, "Warning": 39, "Info": 1185, "Debug": 1937, "Audit": 0, "Monitor": 0 },
    "Node Owner": "artgins",
    "Global Errors":   { "App": { "email NOT sent": 1 }, "Liburing": { "getaddrinfo() FAILED": 1 } },
    "Global Warnings": { "Info": { "No authz db, authz only to local access": 1 } },
    "Global Infos":    { "Connect Disconnect": { "Connected": 295, "Disconnected": 386 } }
}
```

## Monitoring & self-heal

- **Resource watch:** on its timer `logcenter` checks free disk and free RAM
  (each re-armed for 24 h after a check). When either falls to/below
  `min_free_disk` / `min_free_mem` it logs a warning and e-mails a report.
- **Queue-alarm restart:** when a record carries a `queue_size` field and
  `restart_on_alarm` is set with `queue_size ≥ queue_restart_limit`, it runs
  `restart_yuneta_command` via `system()`, throttled so a new restart cannot
  fire until `timeout_restart_yuneta` has elapsed. This is a last-resort
  watchdog for a node whose queues are runaway; it is off by default.

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_LOGCENTER` | `debug` | CRC/sequence mismatches on incoming datagrams |

Enable with
`ycommand command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=C_LOGCENTER set=1 level=debug`.
