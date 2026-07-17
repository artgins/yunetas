(util-list_queue_msgs2)=
# `list_queue_msgs2`

List the messages of a **msg2db queue** (a queue persisted on timeranger2 — the
same queue API the MQTT broker and emailsender use). By default it shows only
*pending* messages.

## Usage

```bash
list_queue_msgs2 PATH [options]
```

| Option | Purpose |
|--------|---------|
| `--all` / `-a` | Include non-pending messages (default: pending only) |
| `--print-local-time` / `-t` | Local timestamps |
| `--verbose` / `-l` | Verbosity level |

## See also

- [`msg2db_list`](msg2db_list.md) — msg2db topic listing.
- [`utils/c/list_queue_msgs2/README.md`](https://github.com/artgins/yunetas/blob/7.8.1/utils/c/list_queue_msgs2/README.md).
