/****************************************************************************
 *          C_POSTGRES.H
 *          Postgres GClass.
 *
 *          Postgress uv-mixin for Yuneta
Support of Postgres for Yuneta

install
-------

sudo apt-get install postgresql-server-dev-all libpq-dev

To fix this error in ubuntu 20.04 "fatal error: postgresql/libpq-fe.h: No such file or directory" ::

    sudo apt-get install --reinstall libpq-dev


Utils
-----

Number of rows::

    select COUNT(*) from tablename;

Size in megas::

    \dt+

Delete all rows::

    delete from tablename;

List tables::

    \dt+

Desc of table::

    \d+ tablename;

See last record::

    SELECT id,rowid,__created_at__ FROM tracks_purezadb order by rowid DESC limit 1;

    SELECT * FROM tracks_purezadb order by __created_at__ DESC limit 1;

Add new column::

    ALTER TABLE tracks_purezadb ADD COLUMN noise bigint;

Cortar las conexiones::

    SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE pid <> pg_backend_pid() AND datname ='mussss_local';

 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_POSTGRES);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_SEND_QUERY);
GOBJ_DECLARE_EVENT(EV_CLEAR_QUEUE);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_postgres(void);

#ifdef __cplusplus
}
#endif
