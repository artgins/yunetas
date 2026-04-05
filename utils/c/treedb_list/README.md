# treedb_list

List nodes of a **TreeDB** graph database (built on timeranger2). Walks topics and their hook/fkey relationships, printing nodes with their `__md_treedb__` metadata.

## Usage

```bash
treedb_list --path <tranger-path> --database <treedb> [--topic <topic>] [--ids <id,…>] [options]
```

Run `treedb_list --help` for filters and output modes. See also `tr2search` for lower-level topic queries.
