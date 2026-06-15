# msg2db_list

List messages from a **msg2db** database (dict-style message store organised by topic, built on top of timeranger2). Queries records through the `pkey` / `pkey2` indices (the active message plus its instances) with optional id filtering, field selection and form/table output.

## Usage

```bash
msg2db_list PATH [--database <db>] [--topic <topic>] [--ids <id,…>] [options]
```

`PATH` is a positional argument (the tranger root, the database, or a topic directory); the database and topic are auto-resolved from it when not given. Run `msg2db_list --help` for filters and output modes. See also `treedb_list` for **TreeDB** graph databases.
