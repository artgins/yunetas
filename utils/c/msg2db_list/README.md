# msg2db_list

`msg2db_list` lists the messages of a msg2db database. A msg2db database is a store of dict messages, organized by topic, on top of timeranger2. The tool reads the records through the `pkey` and `pkey2` indices, that is, the active message and its instances. Options filter by id, select fields, and print as a form or a table.

## Usage

```bash
msg2db_list PATH [--database <db>] [--topic <topic>] [--ids <id,…>] [options]
```

`PATH` is a positional argument. It is the tranger root, the database, or a topic directory. If you do not give the database or the topic, the tool finds them from `PATH`. Run `msg2db_list --help` for the filters and the output modes. For TreeDB graph databases, use `treedb_list`.
