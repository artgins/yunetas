# modules/c/postgres

Optional PostgreSQL client GClasses, enabled with `CONFIG_MODULE_POSTGRES=y`. Wraps `libpq` (connection pooling, async queries, result parsing) so yunos can talk to a Postgres database through the usual gobj + event-loop pattern.

Used by `yunos/c/dba_postgres`. Requires `postgresql-server-dev-all` / `libpq-dev` on the build host.
