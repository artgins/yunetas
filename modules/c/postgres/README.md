# modules/c/postgres

Optional PostgreSQL client GClasses, enabled with `CONFIG_MODULE_POSTGRES=y`. Wraps `libpq` (connection pooling, async queries, result parsing) so yunos can talk to a Postgres database through the usual gobj + event-loop pattern.

Used by `yunos/c/dba_postgres`.

**Build host requirements:** the `libpq` client dev headers — `libpq-dev`
(Debian/Ubuntu) or `libpq-devel` (RHEL/Rocky/Alma). The source includes
`<libpq-fe.h>` (not `<postgresql/libpq-fe.h>`): the header lives in
`/usr/include` on RHEL but `/usr/include/postgresql` on Debian, so
`CMakeLists.txt` adds the right directory from `pg_config --includedir`. That
keeps the same include working on both distro families.
