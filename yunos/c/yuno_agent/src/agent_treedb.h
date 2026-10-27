/****************************************************************************
 *          agent_treedb.h
 *
 *          The agent's own treedb (treedb_yuneta_agent): opening it, with
 *          the schema compiled in the agent, and what happens when it does
 *          not open.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*
 *  Open the agent's treedb in the C_TREEDB service `gobj_treedbs`, with the
 *  schema of treedb_schema_yuneta_agent.c imposed whatever the store holds.
 *  Its name, the id of the schema, is written in `treedb_name`.
 *
 *  An agent does not run without its treedb, so this does not return when
 *  the treedb does not open: an open-treedb answered -1 ENDS THE PROCESS
 *  with exit 0, which ydaemon does not relaunch (the main agent stays down
 *  until it is started again, and yuneta_agent22, which opens no treedb,
 *  stays up as the way in), with "Cannot start agent treedb: <answer>" on
 *  the console and the answer in the log. A schema that does not parse
 *  ends it with exit -1. Returns 0: the treedb is open.
 */
PUBLIC int open_agent_treedb(
    hgobj gobj,
    hgobj gobj_treedbs,
    char *treedb_name,
    size_t treedb_name_size
);

#ifdef __cplusplus
}
#endif
