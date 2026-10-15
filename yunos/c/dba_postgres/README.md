# dba_postgres

PostgreSQL database administrator yuno. Provides connection pooling, query execution and data-access services for other Yuneta applications that need a relational store.

Requires the `postgres` module (`CONFIG_MODULE_POSTGRES=y`) to be enabled and `libpq` from the external libs.

> **Not usable in this version:** `C_POSTGRES` does not connect (see
> `modules/c/postgres/README.md`), and with `CONFIG_DBA_POSTGRES` on this yuno
> does not link (the static `libpq.a` needs `libpgcommon` and `libpgport`,
> `TODO.md` §2). No configuration builds it.
