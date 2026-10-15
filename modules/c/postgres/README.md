# modules/c/postgres

Optional PostgreSQL client GClasses, enabled with `CONFIG_MODULE_POSTGRES=y`. Wraps `libpq` (connection pooling, async queries, result parsing) so yunos can talk to a Postgres database through the usual gobj + event-loop pattern.

Used by `yunos/c/dba_postgres`.

> **It does not connect in this version.** The polling of the libpq socket
> (`PQconnectPoll()`, `PQconsumeInput()`) was written for libuv and is
> commented out since the port to `yev_loop`, so the gclass never reaches
> `ST_CONNECTED`. Left as it is by decision until a project needs
> PostgreSQL: see `TODO.md` §4 and `docs/doc.yuneta.io/modules/postgres.md`.

**Build host requirements:** the `libpq` client dev headers — `libpq-dev`
(Debian/Ubuntu) or `libpq-devel` (RHEL/Rocky/Alma). The source includes
`<libpq-fe.h>` (not `<postgresql/libpq-fe.h>`): the header lives in
`/usr/include` on RHEL but `/usr/include/postgresql` on Debian, so
`CMakeLists.txt` adds the right directory from `pg_config --includedir`. That
keeps the same include working on both distro families.
