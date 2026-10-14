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

:::{warning}
`C_POSTGRES` does not connect in this version. The code that polled the libpq
socket (`PQconnectPoll()`, `PQconsumeInput()`) and sent `EV_CONNECTED` was
written for libuv and is commented out since the port to `yev_loop`: the
gclass opens a connection with `PQconnectStart()` and never reads it, so it
never reaches `ST_CONNECTED` and runs no query. `dba_postgres`, its one
consumer, is built by no configuration (`CONFIG_DBA_POSTGRES` is off).
:::

**Commands:** `list-size` / `list-queue` (pending-query queue), `view-channels`,
`authzs`. **Trace levels:** `messages`.

Consumed by the [`dba_postgres`](../yunos/dba_postgres.md) yuno (the
`__postgres__` service), which adds the DBA layer on top.
