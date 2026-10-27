/****************************************************************************
 *          agent_treedb.c
 *
 *          The agent's own treedb. See agent_treedb.h.
 *
 *          Up to 7.25.4 an open-treedb that refused the schema answered 0,
 *          and the agent ran without its treedb. It answers -1 now, and the
 *          agent ends with exit 0. This lives out of c_agent.c, which no
 *          test compiles, so that a test can open the agent's treedb as the
 *          agent does and see the process end when it does not open
 *          (tests/c/c_agent_treedb_open).
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>

#include "treedb_schema_yuneta_agent.c"
#include "agent_treedb.h"

/***************************************************************************
 *  See agent_treedb.h
 ***************************************************************************/
PUBLIC int open_agent_treedb(
    hgobj gobj,
    hgobj gobj_treedbs,
    char *treedb_name,
    size_t treedb_name_size
)
{
    helper_quote2doublequote(treedb_schema_yuneta_agent);
    json_t *jn_treedb_schema_yuneta_agent;
    jn_treedb_schema_yuneta_agent = legalstring2json(treedb_schema_yuneta_agent, TRUE);
    if(parse_schema(jn_treedb_schema_yuneta_agent)<0) {
        /*
         *  Exit if schema fails
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "Parse schema fails",
            NULL
        );
        exit(-1);
    }

    const char *treedb_name_ = kw_get_str(gobj,
        jn_treedb_schema_yuneta_agent,
        "id",
        "treedb_agentdb",
        KW_REQUIRED
    );
    snprintf(treedb_name, treedb_name_size, "%s", treedb_name_);

    json_t *kw_treedb = json_pack("{s:s, s:i, s:s, s:o, s:b}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", treedb_name,
        "treedb_schema", jn_treedb_schema_yuneta_agent,
        "impose_c_schema", 1    // the binary imposes its schema, whatever the attribute says
    );
    json_t *jn_resp = gobj_command(gobj_treedbs,
        "open-treedb",
        kw_treedb,
        gobj
    );
    int result = (int)kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    if(result < 0) {
        const char *comment = kw_get_str(gobj, jn_resp, "comment", "", KW_REQUIRED);
        print_error(
            0,
            "Cannot start agent treedb: %s",
            comment
        );
        gobj_log_error(gobj, LOG_OPT_EXIT_ZERO|LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", comment,
            NULL
        );
    }
    json_decref(jn_resp);

    return 0;
}
