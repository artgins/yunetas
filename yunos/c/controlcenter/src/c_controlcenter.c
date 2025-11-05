/***********************************************************************
 *          C_CONTROLCENTER.C
 *          Controlcenter GClass.
 *
 *          Control Center of cccc Systems
 *
 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <grp.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <pwd.h>

#include "c_controlcenter.h"
#include "treedb_schema_controlcenter.c"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_logout_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_agents(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_command_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_stats_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_drop_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_agents[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "expand",        0,              0,          "Expand details"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_command_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "agent_id",     0,              0,          "agent id (UUID or HOSTNAME)"),
SDATAPM (DTP_STRING,    "agent_service",0,              0,          "agent service"),
SDATAPM (DTP_STRING,    "cmd2agent",    0,              0,          "command to agent"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_stats_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "agent_id",     0,              0,          "agent id (UUID or HOSTNAME)"),
SDATAPM (DTP_STRING,    "agent_service",0,              0,          "agent service"),
SDATAPM (DTP_STRING,    "stats2agent",  0,              0,          "stats to agent"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_drop_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
    SDATAPM (DTP_STRING,    "agent_id",     0,              0,          "agent id (UUID or HOSTNAME)"),
    SDATA_END()
};
PRIVATE sdata_desc_t pm_write_tty[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "agent_id",     0,              0,          "agent id"),
SDATAPM (DTP_STRING,    "content64",    0,              0,          "Content64 data to write to tty"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_logout_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "username",     0,              0,          "Username"),
SDATAPM (DTP_BOOLEAN,   "disabled",     0,              0,          "Disable user"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_write_tty[] = {"EV_WRITE_TTY", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "authzs",           0,                  0,              pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATACM (DTP_SCHEMA,    "logout-user",      0,                  pm_logout_user, cmd_logout_user,"Logout user"),
SDATACM (DTP_SCHEMA,    "list-agents",      0,                  pm_list_agents, cmd_list_agents, "List connected agents"),

SDATACM2 (DTP_SCHEMA,   "command-agent",    SDF_WILD_CMD,       0,              pm_command_agent,cmd_command_agent,"Command to agent (agent id = UUID or HOSTNAME)"),

SDATACM2 (DTP_SCHEMA,   "stats-agent",      SDF_WILD_CMD,       0,              pm_stats_agent, cmd_stats_agent, "Get statistics of agent"),

SDATACM2 (DTP_SCHEMA,   "drop-agent",       SDF_WILD_CMD,       0,              pm_drop_agent,cmd_drop_agent,"Drop connection to agent (agent id = UUID or HOSTNAME)"),

SDATACM2 (DTP_SCHEMA,   "write-tty",        0,                  a_write_tty,    pm_write_tty,   0,              "Write data to tty (internal use)"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_STRING,      "__username__",     SDF_RD,             "",         "Username"),
SDATA (DTP_INTEGER,     "txMsgs",           SDF_RD|SDF_PSTATS,  0,          "Messages transmitted"),
SDATA (DTP_INTEGER,     "rxMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages received"),

SDATA (DTP_INTEGER,     "txMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (DTP_INTEGER,     "rxMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (DTP_INTEGER,     "maxtxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Tx Messages by second"),
SDATA (DTP_INTEGER,     "maxrxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Rx Messages by second"),

SDATA (DTP_BOOLEAN,     "enabled_new_devices",SDF_PERSIST,      "1",          "Auto enable new devices"),
SDATA (DTP_BOOLEAN,     "enabled_new_users",SDF_PERSIST,        "1",          "Auto enable new users"),

// TODO a 0 cuando funcionen bien los out schemas
SDATA (DTP_BOOLEAN,     "use_internal_schema",SDF_WR,           "1",          "Use internal (hardcoded) schema"),

SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/

PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATAAUTHZ (DTP_SCHEMA, "list-agents",      0,      0,      0,      "Permission to list remote agents"),
SDATAAUTHZ (DTP_SCHEMA, "command-agent",    0,      0,      0,      "Permission to send command to remote agent"),
SDATAAUTHZ (DTP_SCHEMA, "drop-agent",       0,      0,      0,      "Permission to drop connection to remote agent"),
SDATAAUTHZ (DTP_SCHEMA, "logout-user",      0,      0,      0,      "Permission to logout users"),
SDATAAUTHZ (DTP_SCHEMA, "write-tty",        0,      0,      0,      "Internal use. Feed remote consola from local keyboard"),

SDATAAUTHZ (DTP_SCHEMA, "list-groups",      0,      0,      0,      "Permission to list groups"),
SDATAAUTHZ (DTP_SCHEMA, "list-tracks",      0,      0,      0,      "Permission to list tracks"),
SDATAAUTHZ (DTP_SCHEMA, "realtime-track",   0,      0,      0,      "Permission to realtime-tracks"),

SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    int32_t timeout;

    hgobj gobj_top_side;
    hgobj gobj_input_side;

    hgobj gobj_treedbs;
    hgobj gobj_treedb_controlcenter;
    hgobj gobj_authz;

    uint64_t txMsgs;
    uint64_t rxMsgs;
    uint64_t txMsgsec;
    uint64_t rxMsgsec;
} PRIVATE_DATA;





            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,             gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------*
     *      Get Authzs service
     *-----------------------------*/
    priv->gobj_authz =  gobj_find_service("authz", TRUE);
    gobj_subscribe_event(priv->gobj_authz, 0, 0, gobj);

    gobj_start(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_unsubscribe_event(priv->gobj_authz, 0, 0, gobj);

    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 *  cccc rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-------------------------------------------*
     *          Create Treedb System
     *-------------------------------------------*/
    char path[PATH_MAX];
    yuneta_realm_store_dir(
        path,
        sizeof(path),
        gobj_yuno_role(),
        gobj_yuno_realm_owner(),
        gobj_yuno_realm_id(),
        "",  // gclass-treedb controls the directories
        TRUE
    );
    json_t *kw_treedbs = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i}",
        "path", path,
        "filename_mask", "%Y",  // to management treedbs we don't need multifiles (per day)
        "master", 1,
        "xpermission", 02770,
        "rpermission", 0660,
        "exit_on_error", LOG_OPT_EXIT_ZERO
    );
    priv->gobj_treedbs = gobj_create_service(
        "treedbs",
        C_TREEDB,
        kw_treedbs,
        gobj
    );

    /*
     *  HACK pipe inheritance
     */
    gobj_set_bottom_gobj(gobj, priv->gobj_treedbs);

    /*
     *  Start treedbs
     */
    gobj_subscribe_event(priv->gobj_treedbs, 0, 0, gobj);
    gobj_start_tree(priv->gobj_treedbs);

    /*-------------------------------------------*
     *      Load schema
     *      Open treedb controlcenter service
     *-------------------------------------------*/
    helper_quote2doublequote(treedb_schema_controlcenter);
    json_t *jn_treedb_schema_controlcenter = legalstring2json(treedb_schema_controlcenter, TRUE);
    if(!jn_treedb_schema_controlcenter) {
        /*
         *  Exit if schema fails
         */
        exit(-1);
    }

    if(parse_schema(jn_treedb_schema_controlcenter)<0) {
        /*
         *  Exit if schema fails
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", "Parse schema fails",
            NULL
        );
        exit(-1);
    }

    BOOL use_internal_schema = gobj_read_bool_attr(gobj, "use_internal_schema");

    const char *treedb_name = kw_get_str(gobj,
        jn_treedb_schema_controlcenter,
        "id",
        "treedb_controlcenter",
        KW_REQUIRED
    );

    json_t *kw_treedb = json_pack("{s:s, s:i, s:s, s:o, s:b}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", treedb_name,
        "treedb_schema", jn_treedb_schema_controlcenter,
        "use_internal_schema", use_internal_schema
    );
    json_t *jn_resp = gobj_command(priv->gobj_treedbs,
        "open-treedb",
        kw_treedb,
        gobj
    );
    int result = (int)kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    if(result < 0) {
        const char *comment = kw_get_str(gobj, jn_resp, "comment", "", KW_REQUIRED);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", comment,
            NULL
        );
    }
    json_decref(jn_resp);

    priv->gobj_treedb_controlcenter = gobj_find_service("treedb_controlcenter", TRUE);
    gobj_subscribe_event(priv->gobj_treedb_controlcenter, 0, 0, gobj);

    /*-------------------------*
     *      Start services
     *-------------------------*/
    priv->gobj_top_side = gobj_find_service("__top_side__", TRUE);
    gobj_subscribe_event(priv->gobj_top_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_top_side);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------*
     *      Stop services
     *---------------------------------------*/
    if(priv->gobj_top_side) {
        if(gobj_is_playing(priv->gobj_top_side)) {
            gobj_pause(priv->gobj_top_side);
        }
        gobj_stop_tree(priv->gobj_top_side);
    }
    if(priv->gobj_input_side) {
        if(gobj_is_playing(priv->gobj_input_side)) {
            gobj_pause(priv->gobj_input_side);
        }
        gobj_stop_tree(priv->gobj_input_side);
    }

    /*---------------------------------------*
     *      Close treedb controlcenter
     *---------------------------------------*/
    json_decref(gobj_command(priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s}",
            "treedb_name", "treedb_controlcenter"
        ),
        gobj
    ));
    priv->gobj_treedb_controlcenter = 0;

    /*-------------------------*
     *      Stop treedbs
     *-------------------------*/
    if(priv->gobj_treedbs) {
        gobj_unsubscribe_event(priv->gobj_treedbs, 0, 0, gobj);
        gobj_stop_tree(priv->gobj_treedbs);
        EXEC_AND_RESET(gobj_destroy, priv->gobj_treedbs)
    }

    clear_timeout(priv->timer);
    return 0;
}




            /***************************
             *      Commands
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    return gobj_build_authzs_doc(gobj, cmd, kw);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_logout_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(gobj, kw, "username", "", 0);
    BOOL disabled = kw_get_bool(gobj, kw, "disabled", 0, KW_WILD_NUMBER);

    if(empty_string(username)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What realm owner?"),
            0,
            0,
            kw  // owned
        );
    }

    int result = gobj_send_event(
        priv->gobj_authz,
        EV_REJECT_USER,
        json_pack("{s:s, s:b}",
            "username", username,
            "disabled", disabled
        ),
        gobj
    );

    return msg_iev_build_response(
        gobj,
        0,
        result<0?
            json_sprintf("%s", gobj_log_last_message()):
            json_sprintf("%d sessions dropped", result),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_agents(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int expand = (int)kw_get_int(gobj, kw, "expand", 0, KW_WILD_NUMBER);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "list-agents";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Job
     *----------------------------------------*/
    json_t *jn_data = json_array();

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );
    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = json_deep_copy(gobj_read_json_attr(child, "identity_card"));
        json_object_del(jn_attrs, "jwt");
        if(expand) {
            json_array_append_new(jn_data, jn_attrs);
        } else {
            json_array_append_new(jn_data,
                json_sprintf("UUID:%s (%s,%s),  HOSTNAME:'%s'",
                    kw_get_str(gobj, jn_attrs, "id", "", 0),
                    kw_get_str(gobj, jn_attrs, "yuno_role", "", 0),
                    kw_get_str(gobj, jn_attrs, "yuno_version", "", 0),
                    kw_get_str(gobj, jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0)
                )
            );
            json_decref(jn_attrs);
        }
    }
    gobj_free_iter(dl_children);

    return msg_iev_build_response(gobj,
        0,
        0,
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_command_agent(hgobj gobj, const char *cmd, json_t *kw_, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *kw = json_deep_copy(kw_);
    KW_DECREF(kw_);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "command-agent";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Job
     *----------------------------------------*/
    const char *keys2delete[] = { // WARNING parameters of command-yuno command of agent
        "id",
        "command",
        "service",
        "realm_id",
        "yuno_role",
        "yuno_name",
        "yuno_release",
        "yuno_tag",
        "yuno_disabled",
        "yuno_running",
        0
    };
    for(int i=0; keys2delete[i]!=0; i++) {
        json_object_del(kw, keys2delete[i]);
    }

    const char *agent_id = kw_get_str(gobj, kw, "agent_id", "", 0);
    const char *cmd2agent = kw_get_str(gobj, kw, "cmd2agent", "", 0);
    const char *agent_service = kw_get_str(gobj, kw, "agent_service", "", 0);
    if(!empty_string(agent_service)) {
        json_object_set_new(kw, "service", json_string(agent_service));
    }

    if(empty_string(cmd2agent)) {
        return msg_iev_build_response(gobj,
            -1,
            json_string("What cmd2agent?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );

    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int some = 0;
    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(gobj, jn_attrs, "id", "", 0);
            const char *host_ = kw_get_str(gobj, jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0);
            if(strcmp(id_, agent_id)!=0 && strcmp(host_, agent_id)!=0) {
                continue;
            }
        }

        json_t *webix = gobj_command( // debe retornar siempre 0.
            child,
            cmd2agent,
            json_incref(kw),
            src
        );
        JSON_DECREF(webix);
        some++;
        break; // connect ONLY one
    }

    gobj_free_iter(dl_children);

    return msg_iev_build_response(gobj, // Asynchronous response too
        some?0:-1,
        json_sprintf("Command sent to %d nodes", some),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_stats_agent(hgobj gobj, const char *cmd, json_t *kw_, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *kw = json_deep_copy(kw_);
    KW_DECREF(kw_);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "command-agent";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Job
     *----------------------------------------*/
    const char *keys2delete[] = { // WARNING parameters of command-yuno command of agent
        "id",
        "command",
        "service",
        "realm_id",
        "yuno_role",
        "yuno_name",
        "yuno_release",
        "yuno_tag",
        "yuno_disabled",
        "yuno_running",
        0
    };
    for(int i=0; keys2delete[i]!=0; i++) {
        json_object_del(kw, keys2delete[i]);
    }

    const char *agent_id = kw_get_str(gobj, kw, "agent_id", "", 0);
    const char *stats2agent = kw_get_str(gobj, kw, "stats2agent", "", 0);
    const char *agent_service = kw_get_str(gobj, kw, "agent_service", "", 0);
    if(!empty_string(agent_service)) {
        json_object_set_new(kw, "service", json_string(agent_service));
    }

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );
    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int some = 0;
    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(gobj, jn_attrs, "id", "", 0);
            const char *host_ = kw_get_str(gobj, jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0);
            if(strcmp(id_, agent_id)!=0 && strcmp(host_, agent_id)!=0) {
                continue;
            }
        }

        json_t *webix = gobj_stats( // debe retornar siempre 0.
            child,
            stats2agent,
            json_incref(kw),
            src
        );
        JSON_DECREF(webix);
        some++;
        break; // connect ONLY one
    }

    gobj_free_iter(dl_children);

    return msg_iev_build_response(gobj, // Asynchronous response too
        some?0:-1,
        json_sprintf("Stats sent to %d nodes", some),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_drop_agent(hgobj gobj, const char *cmd, json_t *kw_, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *kw = json_deep_copy(kw_);
    KW_DECREF(kw_);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "drop-agent";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }


    const char *agent_id = kw_get_str(gobj, kw, "agent_id", "", 0);

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );
    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int some = 0;
    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(gobj, jn_attrs, "id", "", 0);
            const char *host_ = kw_get_str(gobj, jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0);
            if(strcmp(id_, agent_id)!=0 && strcmp(host_, agent_id)!=0) {
                continue;
            }
        }

        gobj_send_event( // debe retornar siempre 0.
            child,
            EV_DROP,
            json_object(),
            src
        );
        some++;
    }

    gobj_free_iter(dl_children);

    return msg_iev_build_response(gobj, // Asynchronous response too
        some?0:-1,
        json_sprintf("Drop sent to %d nodes", some),
        0,
        0,
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Identity_card on from
 *      Web clients (__top_side__)
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src != priv->gobj_top_side && src != priv->gobj_input_side) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "on_open NOT from GOBJ_TOP_SIDE",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Identity_card off from
 *      Web clients (__top_side__)
 *      agent clients (__input_side__)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    const char *dst_service = json_string_value(
        gobj_read_user_data(channel_gobj, "tty_mirror_dst_service")
    );

    if(!empty_string(dst_service)) {
        hgobj gobj_requester = gobj_child_by_name(
            priv->gobj_top_side,
            dst_service
        );

        if(!gobj_requester) {
            // Debe venir del agent
        }

        if(gobj_requester) {
            gobj_write_user_data(channel_gobj, "tty_mirror_dst_service", json_string(""));
            gobj_send_event(gobj_requester, EV_DROP, 0, gobj);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_stats_yuno_answer(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "service not found",
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_ievent_id);

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE); // "__answer__"

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_command_yuno_answer(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "service not found",
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_ievent_id);

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_tty_mirror_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "service not found",
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }

    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    gobj_write_user_data(channel_gobj, "tty_mirror_dst_service", json_string(dst_service));

    JSON_DECREF(jn_ievent_id);

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_tty_mirror_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "service not found",
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }

    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    gobj_write_user_data(channel_gobj, "tty_mirror_dst_service", json_string(""));

    JSON_DECREF(jn_ievent_id);

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_tty_mirror_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_top_side,
        dst_service
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "service not found",
            "service",      "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_ievent_id);

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node, pero al revés(???)
 ***************************************************************************/
PRIVATE int ac_write_tty(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS TODO función interna para todos? marca con flag
     *----------------------------------------*/
    const char *permission = "write-tty";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        gobj_send_event(
            src,
            event,
                msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("No permission to '%s'", permission),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
        KW_DECREF(kw);
        return 0;
    }

    /*----------------------------------------*
     *  Job
     *----------------------------------------*/
    const char *agent_id = kw_get_str(gobj, kw, "agent_id", "", 0);

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", C_IEVENT_SRV,
        "__state__", ST_SESSION
    );
    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int some = 0;
    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(gobj, jn_attrs, "id", "", 0);
            if(strcmp(id_, agent_id)!=0) {
                continue;
            }

        }

        json_t *webix = gobj_command( // debe retornar siempre 0.
            child,
            "write-tty",
            json_incref(kw),
            src
        );
        some++;
        JSON_DECREF(webix);
    }

    gobj_free_iter(dl_children);

    if(!some) {
        gobj_send_event(src, EV_DROP, 0, gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_treedb_node_create(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", KW_REQUIRED);
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", KW_REQUIRED);
    json_t *node_ = kw_get_dict(gobj, kw, "node", 0, KW_REQUIRED);

    // TODO wtf purezadb?
    if(strcmp(treedb_name, "treedb_purezadb")==0 &&
        strcmp(topic_name, "users")==0) {
        /*--------------------------------*
         *  Get user
         *  Create it if not exist
         *  Han creado el user en la tabla users de treedb_purezadb
         *  Puede que exista o no en la users de authzs
         *--------------------------------*/
        const char *username = kw_get_str(gobj, node_, "id", "", KW_REQUIRED);
        json_t *webix = gobj_command(
            priv->gobj_authz,
            "user-roles",
            json_pack("{s:s}",
                "username", username
            ),
            gobj
        );
        if(json_array_size(kw_get_dict_value(gobj, webix, "data", 0, KW_REQUIRED))==0) {
            gobj_send_event(
                priv->gobj_authz,
                EV_ADD_USER,
                json_pack("{s:s, s:s}",
                    "username", username,
                    "role", "roles^user-purezadb^users"
                ),
                gobj
            );
        }
        json_decref(webix);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_treedb_node_updated(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", KW_REQUIRED);
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", KW_REQUIRED);
    json_t *node_ = kw_get_dict(gobj, kw, "node", 0, KW_REQUIRED);

    if(strcmp(treedb_name, "treedb_purezadb")==0 &&
        strcmp(topic_name, "users")==0) {
        /*--------------------------------*
         *  Get user
         *  Create it if not exist
         *  Han creado el user en la tabla users de treedb_purezadb
         *  Puede que exista o no en la users de authzs
         *--------------------------------*/
        BOOL enabled = kw_get_bool(gobj, node_, "enabled", 0, KW_REQUIRED);
        const char *username = kw_get_str(gobj, node_, "id", "", KW_REQUIRED);
        json_t *webix = gobj_command(
            priv->gobj_authz,
            "user-roles",
            json_pack("{s:s}",
                "username", username
            ),
            gobj
        );

        if(json_array_size(kw_get_list(gobj, webix, "data", 0, KW_REQUIRED))==0) {
            gobj_send_event(
                priv->gobj_authz,
                EV_ADD_USER,
                json_pack("{s:s, s:s, s:b}",
                    "username", username,
                    "role", "roles^user-purezadb^users",
                    "disabled", enabled?0:1
                ),
                gobj
            );
        } else {
            gobj_send_event(
                priv->gobj_authz,
                EV_ADD_USER,
                json_pack("{s:s, s:b}",
                    "username", username,
                    "disabled", enabled?0:1
                ),
                gobj
            );
        }
        json_decref(webix);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_treedb_node_deleted(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", KW_REQUIRED);
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", KW_REQUIRED);
    json_t *node_ = kw_get_dict(gobj, kw, "node", 0, KW_REQUIRED);

    if(strcmp(treedb_name, "treedb_purezadb")==0 &&
        strcmp(topic_name, "users")==0) {
        /*--------------------------------*
         *  Get user
         *  Create it if not exist
         *  Han creado el user en la tabla users de treedb_purezadb
         *  Puede que exista o no en la users de authzs
         *--------------------------------*/
        const char *username = kw_get_str(gobj, node_, "id", "", KW_REQUIRED);
        gobj_send_event(
            priv->gobj_authz,
            EV_REJECT_USER,
            json_pack("{s:s, s:b}",
                "username", username,
                "disabled", 1
            ),
            gobj
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_user_login(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);
//     const char *dst_service = kw_get_str(gobj, kw, "dst_service", "", KW_REQUIRED);
//     json_t *user_ = kw_get_dict(gobj, kw, "user", 0, KW_REQUIRED);
//     json_t *session_ = kw_get_dict(gobj, kw, "session", 0, KW_REQUIRED);
//     json_t *services_roles_ = kw_get_dict(gobj, kw, "services_roles", 0, KW_REQUIRED);

    /*--------------------------------*
     *  Get user
     *  Create it if not exist
     *--------------------------------*/
    json_t *user = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "users",
        json_pack("{s:s}",
            "id", username
        ),
        0,
        gobj
    );
    if(!user) {
        time_t t;
        time(&t);
        BOOL enabled_new_users = gobj_read_bool_attr(gobj, "enabled_new_users");
        json_t *jn_user = json_pack("{s:s, s:b, s:I}",
            "id", username,
            "enabled", enabled_new_users,
            "time", (json_int_t)t
        );

        user = gobj_create_node(
            priv->gobj_treedb_controlcenter,
            "users",
            jn_user,
            0,
            gobj
        );
    }
    json_decref(user);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_user_logout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
//
//     const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);
//     json_t *user_ = kw_get_dict(gobj, kw, "user", 0, KW_REQUIRED);
//     json_t *session_ = kw_get_dict(gobj, kw, "session", 0, KW_REQUIRED);

    //print_json(kw);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_user_new(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);
    const char *dst_service = kw_get_str(gobj, kw, "dst_service", "", KW_REQUIRED);

    if(strcmp(dst_service, gobj_name(gobj))==0) {
        time_t t;
        time(&t);
        BOOL enabled_new_users = gobj_read_bool_attr(gobj, "enabled_new_users");
        if(enabled_new_users) {
            json_t *jn_user = json_pack("{s:s, s:b, s:I}",
                "id", username,
                "enabled", enabled_new_users,
                "time", (json_int_t)t
            );

            json_decref(gobj_create_node(
                priv->gobj_treedb_controlcenter,
                "users",
                jn_user,
                0,
                gobj
            ));
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint64_t maxtxMsgsec = gobj_read_integer_attr(gobj, "maxtxMsgsec");
    uint64_t maxrxMsgsec = gobj_read_integer_attr(gobj, "maxrxMsgsec");
    if(priv->txMsgsec > maxtxMsgsec) {
        gobj_write_integer_attr(gobj, "maxtxMsgsec", priv->txMsgsec);
    }
    if(priv->rxMsgsec > maxrxMsgsec) {
        gobj_write_integer_attr(gobj, "maxrxMsgsec", priv->rxMsgsec);
    }

    gobj_write_integer_attr(gobj, "txMsgsec", priv->txMsgsec);
    gobj_write_integer_attr(gobj, "rxMsgsec", priv->rxMsgsec);

    priv->rxMsgsec = 0;
    priv->txMsgsec = 0;

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create                  = mt_create,
    .mt_destroy                 = mt_destroy,
    .mt_start                   = mt_start,
    .mt_stop                    = mt_stop,
    .mt_play                    = mt_play,
    .mt_pause                   = mt_pause,
    .mt_writing                 = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_CONTROLCENTER);

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_MT_STATS_ANSWER,        ac_stats_yuno_answer,    0},
        {EV_MT_COMMAND_ANSWER,      ac_command_yuno_answer,  0},
        {EV_TTY_DATA,               ac_tty_mirror_data,      0},
        {EV_WRITE_TTY,              ac_write_tty,            0},

        {EV_TREEDB_NODE_CREATED,    ac_treedb_node_create,   0},
        {EV_TREEDB_NODE_UPDATED,    ac_treedb_node_updated,  0},
        {EV_TREEDB_NODE_DELETED,    ac_treedb_node_deleted,  0},

        {EV_AUTHZ_USER_LOGIN,       ac_user_login,           0},
        {EV_AUTHZ_USER_LOGOUT,      ac_user_logout,          0},
        {EV_AUTHZ_USER_NEW,         ac_user_new,             0},

        {EV_ON_OPEN,                ac_on_open,              0},
        {EV_ON_CLOSE,               ac_on_close,             0},
        {EV_TTY_OPEN,               ac_tty_mirror_open,      0},
        {EV_TTY_CLOSE,              ac_tty_mirror_close,     0},

        {EV_TIMEOUT,                ac_timeout,              0},
        {EV_STOPPED,                0,                       0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_MT_COMMAND_ANSWER,      EVF_PUBLIC_EVENT},
        {EV_TTY_DATA,               EVF_PUBLIC_EVENT},
        {EV_MT_STATS_ANSWER,        EVF_PUBLIC_EVENT},
        {EV_WRITE_TTY,              0},

        {EV_TTY_OPEN,               EVF_PUBLIC_EVENT},
        {EV_TTY_CLOSE,              EVF_PUBLIC_EVENT},
        {EV_AUTHZ_USER_LOGIN,       0},
        {EV_AUTHZ_USER_LOGOUT,      0},
        {EV_AUTHZ_USER_NEW,         0},

        {EV_TREEDB_NODE_CREATED,    0},
        {EV_TREEDB_NODE_UPDATED,    0},
        {EV_TREEDB_NODE_DELETED,    0},

        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},

        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        tattr_desc,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_controlcenter(void)
{
    return create_gclass(C_CONTROLCENTER);
}
