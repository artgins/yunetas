# msg2db_list

List messages from a **msg2db** database (dict-style message store organised by topic, built on top of timeranger2). Queries records through the `pkey` / `pkey2` indices with optional filtering and verbose output.

## Usage

```bash
msg2db_list --path <db-path> --topic <topic> [options]
```

Run `msg2db_list --help` for all filtering flags.
