(yuno-dba_postgres)=
# `dba_postgres`

PostgreSQL data-access yuno. The `Dba_postgres` gclass is the DBA layer on top
of the `__postgres__` service (the `C_POSTGRES` gclass, backed by `libpq`):
it issues queries as `C_TASK` jobs and organizes persisted tables by time.
Requires `CONFIG_MODULE_POSTGRES=y` and `libpq`.

## Architecture

```
Dba_postgres (default service)   <- DBA logic, table organization, tasks
    __postgres__  (C_POSTGRES)   <- libpq connection to PostgreSQL
    C_TASK                       <- one async job per DB operation
```

`Dba_postgres` resolves the `__postgres__` service at start
(`gobj_find_service("__postgres__")`); the PostgreSQL connection (host, port,
database, user, password) is configured on that `C_POSTGRES` service through
the yuno's external configuration, not hardcoded in the binary.

The yuno also declares `emailsender` as a required service (for alerting).

## Configuration

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `master` | `1` | Only the master instance may write |
| `filename_mask` | `%Y-%m` | Time-based table/file organization (monthly) |
| `xpermission` | `02770` | Permission for created directories/executables |
| `rpermission` | `0660` | Permission for created files |
| `exit_on_error` | `2` | Exit policy on error |
| `timeout` | `1000` | Periodic tick (ms) |

PostgreSQL connection parameters live on the `__postgres__` (`C_POSTGRES`)
service configuration.

Throughput stats are exposed as reset-on-read stats: `rxMsgs`, `txMsgs`,
`rxMsgsec`/`txMsgsec` and their high-water marks `maxrxMsgsec`/`maxtxMsgsec`.

## Commands

| Command | Description |
|---------|-------------|
| `help` | Command help |

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `Dba_postgres` | `messages` | DBA message flow |
| `C_POSTGRES` | *(its own levels)* | libpq connection / query activity |

Enable with
`ycommand command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<G> set=1 level=<L>`.
