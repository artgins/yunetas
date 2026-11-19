# dba_postgres

`dba_postgres` is the PostgreSQL database administrator yuno. It gives connection pooling, query execution, and data-access services to other Yuneta applications that need a relational store.

> Not usable in this version. `C_POSTGRES` does not connect (see
> `modules/c/postgres/README.md`). With `CONFIG_DBA_POSTGRES` on, this yuno
> does not link, because the static `libpq.a` needs `libpgcommon` and
> `libpgport` (`TODO.md` §2). No configuration builds it.

The yuno requires the `postgres` module (`CONFIG_MODULE_POSTGRES=y`) and `libpq` from the external libs.
