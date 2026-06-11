(util-msg2db_list)=
# `msg2db_list`

List the messages of a **msg2db** store (a dict-style message store on
timeranger2), via its pkey / pkey2 indices.

## Usage

```bash
msg2db_list --path <path> --topic <topic> [options]
```

All inputs are flags — `msg2db_list` takes **no positional arguments**.

| Option | Purpose |
|--------|---------|
| `--path` / `-a` | Store path |
| `--database` / `-b` | Database |
| `--topic` / `-c` | Topic |
| `--ids` / `-i` | Comma-separated ids |
| `--recursive` / `-r` | Walk subtopics |
| `--print-tranger` / `--print-msg2db` | Raw JSON dumps |

## See also

- [`list_queue_msgs2`](list_queue_msgs2.md) — msg2db **queue** listing.
- [`utils/c/msg2db_list/README.md`](https://github.com/artgins/yunetas/blob/7.6.0/utils/c/msg2db_list/README.md).
