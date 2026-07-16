(util-treedb_list)=
# `treedb_list`

List the nodes of a **TreeDB** graph database (stored on timeranger2), walking
topics and their hook/fkey links, including each node's `__md_treedb__`
metadata.

## Usage

```bash
treedb_list PATH [options]
```

## Options

| Option | Purpose |
|--------|---------|
| `--database` / `-b` | TreeDB database (auto-resolved from `PATH` if omitted) |
| `--topic` / `-c` | Topic to list |
| `--ids` / `-i` | Comma-separated node ids |
| `--fields` / `-f` | Select output fields |
| `--mode form\|table` | Output layout |
| `--recursive` / `-r` | Walk subtopics |
| `--print-tranger` / `--print-treedb` | Raw JSON dumps of the underlying tranger / treedb |
| `--follow` / `-F` | Live CREATE/UPDATE/DELETE (not with `-r` or `--print-*`) |
| `--dry-run` / `-n` | Resolve and print the plan only |

## See also

- [TreeDB graph model](../../../yunos/c/yuno_agent/YUNO_TREEDB.md).
- [`utils/c/treedb_list/README.md`](https://github.com/artgins/yunetas/blob/7.8.0/utils/c/treedb_list/README.md).
