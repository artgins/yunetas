# modules/c/postgres

This module gives optional PostgreSQL client GClasses. To enable it, set `CONFIG_MODULE_POSTGRES=y`. The module wraps `libpq` (connection pooling, async queries, result parsing). With it, yunos can talk to a Postgres database through the usual gobj and event-loop pattern.

`yunos/c/dba_postgres` uses this module.

> It does not connect in this version. The code that polls the libpq socket
> (`PQconnectPoll()`, `PQconsumeInput()`) was written for libuv. Since the
> port to `yev_loop`, that code is commented out, so the gclass never gets to
> `ST_CONNECTED`. The code stays as it is, by decision, until a project needs
> PostgreSQL. See `TODO.md` §4 and `docs/doc.yuneta.io/modules/postgres.md`.

The build host needs the `libpq` client dev headers: `libpq-dev`
(Debian/Ubuntu) or `libpq-devel` (RHEL/Rocky/Alma). The source includes
`<libpq-fe.h>`, not `<postgresql/libpq-fe.h>`. The header is in
`/usr/include` on RHEL, but in `/usr/include/postgresql` on Debian. Thus
`CMakeLists.txt` adds the correct directory from `pg_config --includedir`. As
a result, the same include works on both distro families.
