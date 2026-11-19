# treedb_list

`treedb_list` lists the nodes of a TreeDB graph database. TreeDB runs on timeranger2.

## Usage

```bash
treedb_list --path <tranger-path> --database <treedb> [--topic <topic>] [--ids <id,…>] [options]
```

For the filters and the output modes, run `treedb_list --help`. For lower-level queries of a topic, use `tr2search`.

The tool walks the topics and their hook/fkey relationships. It prints each node with its `__md_treedb__` metadata.
