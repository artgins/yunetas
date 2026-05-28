(module-postgres)=
# Postgres

**Kconfig:** `CONFIG_MODULE_POSTGRES` — **GClasses:** `C_POSTGRES`

PostgreSQL integration — async query execution with automatic JSON-to-SQL type
mapping. Requires `libpq`.

## C_POSTGRES

Async PostgreSQL client with an internal query queue. Maps JSON types
to PostgreSQL types:

| JSON type | PostgreSQL type |
|-----------|-----------------|
| string | `text` |
| integer | `bigint` |
| real | `double precision` |
| boolean | `boolean` |
| object | `text` (JSON serialized) |

Requires `libpq-dev` and `CONFIG_MODULE_POSTGRES=y`.

Queries are queued and run asynchronously over one or more connection channels.

**Commands:** `list-size` / `list-queue` (pending-query queue), `view-channels`,
`authzs`. **Trace levels:** `messages`.

Consumed by the [`dba_postgres`](../yunos/dba_postgres.md) yuno (the
`__postgres__` service), which adds the DBA layer on top.
