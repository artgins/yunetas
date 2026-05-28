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
