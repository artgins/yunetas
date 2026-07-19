(util-msg2db_list)=
# `msg2db_list`

List the messages of a **msg2db** store (a dict-style message store on
timeranger2), via its pkey / pkey2 indices (the active message plus its
instances), including each record's `__md_msg2db__` metadata.

## Usage

```bash
msg2db_list PATH [options]
```

`PATH` is a positional argument: the tranger root, the database, or even a
topic directory — `msg2db_list` resolves the database and topic from it when
`--database` / `--topic` are omitted.

## Options

| Option | Purpose |
|--------|---------|
| `--database` / `-b` | msg2db database (auto-resolved from `PATH` if omitted) |
| `--topic` / `-c` | Topic to list |
| `--ids` / `-i` | Comma-separated message ids |
| `--fields` / `-f` | Select output fields (implies table mode) |
| `--mode form\|table` | Output layout |
| `--recursive` / `-r` | Walk subtopics |
| `--print-tranger` / `--print-msg2db` | Raw JSON dumps of the underlying tranger / msg2db |
| `--dry-run` / `-n` | Resolve and print the plan only |

## See also

- [`list_queue_msgs2`](list_queue_msgs2.md) — msg2db **queue** listing.
- [`treedb_list`](treedb_list.md) — sibling tool for **TreeDB** graph databases.
- [`utils/c/msg2db_list/README.md`](https://github.com/artgins/yunetas/blob/7.8.4/utils/c/msg2db_list/README.md).
