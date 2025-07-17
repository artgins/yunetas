/***********************************************************************
 *          C_CONTROLCENTER.C
 *          Controlcenter GClass.
 *
 *          Control Center of Yuneta Systems
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <grp.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
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
SDATAPM (ASN_OCTET_STR, "cmd",          0,              0,          "command about you want help."),
SDATAPM (ASN_UNSIGNED,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "authz",        0,              0,          "permission to search"),
SDATAPM (ASN_OCTET_STR, "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_agents[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_UNSIGNED,  "expand",        0,              0,          "Expand details"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_command_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "agent_id",     0,              0,          "agent id (UUID or HOSTNAME)"),
SDATAPM (ASN_OCTET_STR, "agent_service",0,              0,          "agent service"),
SDATAPM (ASN_OCTET_STR, "cmd2agent",    0,              0,          "command to agent"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_stats_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "agent_id",     0,              0,          "agent id (UUID or HOSTNAME)"),
SDATAPM (ASN_OCTET_STR, "agent_service",0,              0,          "agent service"),
SDATAPM (ASN_OCTET_STR, "stats2agent",  0,              0,          "stats to agent"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_drop_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
    SDATAPM (ASN_OCTET_STR, "agent_id",     0,              0,          "agent id (UUID or HOSTNAME)"),
    SDATA_END()
};
PRIVATE sdata_desc_t pm_write_tty[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "agent_id",     0,              0,          "agent id"),
SDATAPM (ASN_OCTET_STR, "content64",    0,              0,          "Content64 data to write to tty"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_logout_user[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (ASN_OCTET_STR, "username",     0,              0,          "Username"),
SDATAPM (ASN_BOOLEAN,   "disabled",     0,              0,          "Disable user"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_write_tty[] = {"EV_WRITE_TTY", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (ASN_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (ASN_SCHEMA,   "authzs",           0,                  0,              pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATACM (ASN_SCHEMA,    "logout-user",      0,                  pm_logout_user, cmd_logout_user,"Logout user"),
SDATACM (ASN_SCHEMA,    "list-agents",      0,                  pm_list_agents, cmd_list_agents, "List connected agents"),

SDATACM2 (ASN_SCHEMA,   "command-agent",    SDF_WILD_CMD,       0,              pm_command_agent,cmd_command_agent,"Command to agent (agent id = UUID or HOSTNAME)"),

SDATACM2 (ASN_SCHEMA,   "stats-agent",      SDF_WILD_CMD,       0,              pm_stats_agent, cmd_stats_agent, "Get statistics of agent"),

SDATACM2 (ASN_SCHEMA,   "drop-agent",       SDF_WILD_CMD,       0,              pm_drop_agent,cmd_drop_agent,"Drop connection to agent (agent id = UUID or HOSTNAME)"),

SDATACM2 (ASN_SCHEMA,   "write-tty",        0,                  a_write_tty,    pm_write_tty,   0,              "Write data to tty (internal use)"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (ASN_OCTET_STR,   "__username__",     SDF_RD,             "",              "Username"),
SDATA (ASN_COUNTER64,   "txMsgs",           SDF_RD|SDF_PSTATS,  0,          "Messages transmitted"),
SDATA (ASN_COUNTER64,   "rxMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages receiveds"),

SDATA (ASN_COUNTER64,   "txMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (ASN_COUNTER64,   "rxMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (ASN_COUNTER64,   "maxtxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Tx Messages by second"),
SDATA (ASN_COUNTER64,   "maxrxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Rx Messages by second"),

SDATA (ASN_BOOLEAN,     "enabled_new_devices",SDF_PERSIST,      1,          "Auto enable new devices"),
SDATA (ASN_BOOLEAN,     "enabled_new_users",SDF_PERSIST,        1,          "Auto enable new users"),

// TODO a 0 cuando funcionen bien los out schemas
SDATA (ASN_BOOLEAN,     "use_internal_schema",SDF_WR,           1,          "Use internal (hardcoded) schema"),

SDATA (ASN_INTEGER,     "timeout",          SDF_RD,             1*1000,     "Timeout"),
SDATA (ASN_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (ASN_POINTER,     "user_data2",       0,                  0,          "more user data"),
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
SDATAAUTHZ (ASN_SCHEMA, "list-agents",      0,      0,      0,      "Permission to list remote agents"),
SDATAAUTHZ (ASN_SCHEMA, "command-agent",    0,      0,      0,      "Permission to send command to remote agent"),
SDATAAUTHZ (ASN_SCHEMA, "drop-agent",       0,      0,      0,      "Permission to drop connection to remote agent"),
SDATAAUTHZ (ASN_SCHEMA, "logout-user",      0,      0,      0,      "Permission to logout users"),
SDATAAUTHZ (ASN_SCHEMA, "write-tty",        0,      0,      0,      "Internal use. Feed remote consola from local keyboard"),

SDATAAUTHZ (ASN_SCHEMA, "list-groups",      0,      0,      0,      "Permission to list groups"),
SDATAAUTHZ (ASN_SCHEMA, "list-tracks",      0,      0,      0,      "Permission to list tracks"),
SDATAAUTHZ (ASN_SCHEMA, "realtime-track",   0,      0,      0,      "Permission to realtime-tracks"),

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

    uint64_t *ptxMsgs;
    uint64_t *prxMsgs;
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

    priv->timer = gobj_create(gobj_name(gobj), GCLASS_TIMER, 0, gobj);
    priv->ptxMsgs = gobj_danger_attr_ptr(gobj, "txMsgs");
    priv->prxMsgs = gobj_danger_attr_ptr(gobj, "rxMsgs");

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    BOOL is_yuneta = FALSE;
    struct passwd *pw = getpwuid(getuid());
    if(strcmp(pw->pw_name, "yuneta")==0) {
        gobj_write_str_attr(gobj, "__username__", "yuneta");
        is_yuneta = TRUE;
    } else {
        static gid_t groups[30]; // HACK to use outside
        int ngroups = sizeof(groups)/sizeof(groups[0]);

        getgrouplist(pw->pw_name, 0, groups, &ngroups);
        for(int i=0; i<ngroups; i++) {
            struct group *gr = getgrgid(groups[i]);
            if(strcmp(gr->gr_name, "yuneta")==0) {
                gobj_write_str_attr(gobj, "__username__", "yuneta");
                is_yuneta = TRUE;
                break;
            }
        }
    }
    if(!is_yuneta) {
        trace_msg("User or group 'yuneta' is needed to run %s", gobj_yuno_role());
        printf("User or group 'yuneta' is needed to run %s\n", gobj_yuno_role());
        exit(0);
    }

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
        GCLASS_TREEDB,
        kw_treedbs,
        gobj
    );

    /*
     *  HACK pipe inheritance
     */
    gobj_set_bottom_gobj(gobj, priv->gobj_treedbs);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_int32_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,             gobj_read_int32_attr)
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

    /*
     *  Start treedbs
     */
    gobj_subscribe_event(priv->gobj_treedbs, 0, 0, gobj);
    gobj_start_tree(priv->gobj_treedbs);

    gobj_start(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Stop treedbs
     */
    if(priv->gobj_treedbs) {
        gobj_unsubscribe_event(priv->gobj_treedbs, 0, 0, gobj);
        gobj_stop_tree(priv->gobj_treedbs);
    }

    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 *  Yuneta rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------*
    *      Load schema
    *---------------------------------------*/
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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", "Parse schema fails",
            NULL
        );
        exit(-1);
    }

    BOOL use_internal_schema = gobj_read_bool_attr(gobj, "use_internal_schema");

    const char *treedb_name = kw_get_str(
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
    int result = kw_get_int(jn_resp, "result", -1, KW_REQUIRED);
    if(result < 0) {
        const char *comment = kw_get_str(jn_resp, "comment", "", KW_REQUIRED);
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP_ERROR,
            "msg",          "%s", comment,
            NULL
        );
    }
    json_decref(jn_resp);

    priv->gobj_treedb_controlcenter = gobj_find_service("treedb_controlcenter", TRUE);
    gobj_subscribe_event(priv->gobj_treedb_controlcenter, 0, 0, gobj);

    /*-----------------------------*
     *      Get Authzs service
     *-----------------------------*/
    priv->gobj_authz =  gobj_find_service("authz", TRUE);
    gobj_subscribe_event(priv->gobj_authz, 0, 0, gobj);

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
        gobj_unsubscribe_event(priv->gobj_top_side, 0, 0, gobj);
        EXEC_AND_RESET(gobj_stop_tree, priv->gobj_top_side);
    }
    if(priv->gobj_input_side) {
        gobj_unsubscribe_event(priv->gobj_input_side, 0, 0, gobj);
        EXEC_AND_RESET(gobj_stop_tree, priv->gobj_input_side);
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

    clear_timeout(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method subscription_added
 ***************************************************************************/
PRIVATE int mt_subscription_added(
    hgobj gobj,
    hsdata subs)
{
//    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *__config__ = sdata_read_json(subs, "__config__");
    BOOL first_shot = kw_get_bool(__config__, "__first_shot__", TRUE, 0);
    if(!first_shot) {
        return 0;
    }

    const char *event = sdata_read_str(subs, "event");
    json_t *__global__ = sdata_read_json(subs, "__global__");

    if(strcasecmp(event, "EV_REALTIME_TRACK")==0) {
        json_t *__filter__ = sdata_read_json(subs, "__filter__");
        //json_t *__config__ = sdata_read_json(subs, "__config__");
        hgobj subscriber = sdata_read_pointer(subs, "subscriber");

        json_t *jn_comment = 0;
        int result = 0;
        json_t *jn_data = 0;

        /*----------------------------------------*
         *  Check AUTHZS
         *----------------------------------------*/
        const char *permission = "realtime-track";
        if(1 || gobj_user_has_authz(gobj, permission, 0, subscriber)) { // TODO PRUEBA!!
            // TODO crea la lista en el user
            JSON_INCREF(__filter__);
//            jn_data = trmsg_active_records(priv->tracks, __filter__); TODO
        } else {
            jn_comment = json_sprintf("No permission to '%s'", permission);
            result = -1;
        }

        /*
         *  Inform
         */
        return gobj_send_event(
            subscriber,
            event,
            msg_iev_build_webix2_without_answer_filter(gobj,
                result,
                jn_comment,
                0, //RESOURCE_WEBIX_SCHEMA(priv->resource, resource),
                jn_data, // owned
                __global__?kw_duplicate(__global__):0,  // owned
                "__first_shot__"
            ),
            gobj
        );
    }

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
    return msg_iev_build_webix(
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
    return gobj_build_authzs_doc(gobj, cmd, kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_logout_user(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(kw, "username", "", 0);
    BOOL disabled = kw_get_bool(kw, "disabled", 0, KW_WILD_NUMBER);

    if(empty_string(username)) {
        return msg_iev_build_webix(
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
        "EV_REJECT_USER",
        json_pack("{s:s, s:b}",
            "username", username,
            "disabled", disabled
        ),
        gobj
    );

    return msg_iev_build_webix(
        gobj,
        0,
        result<0?
            json_sprintf("%s", log_last_message()):
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
    int expand = kw_get_int(kw, "expand", 0, KW_WILD_NUMBER);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "list-agents";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_webix(
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
        "__gclass_name__", GCLASS_IEVENT_SRV_NAME,
        "__state__", "ST_SESSION"
    );
    dl_list_t *dl_childs = gobj_match_childs_tree(priv->gobj_input_side, 0, jn_filter);

    hgobj child; rc_instance_t *i_hs;
    i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
    while(i_hs) {
        json_t *jn_attrs = json_deep_copy(gobj_read_json_attr(child, "identity_card"));
        json_object_del(jn_attrs, "jwt");
        if(expand) {
            json_array_append_new(jn_data, jn_attrs);
        } else {
            json_array_append_new(jn_data,
                json_sprintf("UUID:%s (%s,%s),  HOSTNAME:'%s'",
                    kw_get_str(jn_attrs, "id", "", 0),
                    kw_get_str(jn_attrs, "yuno_role", "", 0),
                    kw_get_str(jn_attrs, "yuno_version", "", 0),
                    kw_get_str(jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0)
                )
            );
            json_decref(jn_attrs);
        }
        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
    }

    rc_free_iter(dl_childs, TRUE, 0);

    return msg_iev_build_webix(gobj,
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
        return msg_iev_build_webix(
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

    const char *agent_id = kw_get_str(kw, "agent_id", "", 0);
    const char *cmd2agent = kw_get_str(kw, "cmd2agent", "", 0);
    const char *agent_service = kw_get_str(kw, "agent_service", "", 0);
    if(!empty_string(agent_service)) {
        json_object_set_new(kw, "service", json_string(agent_service));
    }

    if(empty_string(cmd2agent)) {
        return msg_iev_build_webix(gobj,
            -1,
            json_string("What cmd2agent?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", GCLASS_IEVENT_SRV_NAME,
        "__state__", "ST_SESSION"
    );
    dl_list_t *dl_childs = gobj_match_childs_tree(priv->gobj_input_side, 0, jn_filter);

    int some = 0;
    hgobj child; rc_instance_t *i_hs;
    i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
    while(i_hs) {
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(jn_attrs, "id", "", 0);
            const char *host_ = kw_get_str(jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0);
            if(strcmp(id_, agent_id)!=0 && strcmp(host_, agent_id)!=0) {
                i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
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

        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
    }

    rc_free_iter(dl_childs, TRUE, 0);


    return msg_iev_build_webix(gobj, // Asynchronous response too
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
        return msg_iev_build_webix(
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

    const char *agent_id = kw_get_str(kw, "agent_id", "", 0);
    const char *stats2agent = kw_get_str(kw, "stats2agent", "", 0);
    const char *agent_service = kw_get_str(kw, "agent_service", "", 0);
    if(!empty_string(agent_service)) {
        json_object_set_new(kw, "service", json_string(agent_service));
    }

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", GCLASS_IEVENT_SRV_NAME,
        "__state__", "ST_SESSION"
    );
    dl_list_t *dl_childs = gobj_match_childs_tree(priv->gobj_input_side, 0, jn_filter);

    int some = 0;
    hgobj child; rc_instance_t *i_hs;
    i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
    while(i_hs) {
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(jn_attrs, "id", "", 0);
            const char *host_ = kw_get_str(jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0);
            if(strcmp(id_, agent_id)!=0 && strcmp(host_, agent_id)!=0) {
                i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
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

        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
    }

    rc_free_iter(dl_childs, TRUE, 0);


    return msg_iev_build_webix(gobj, // Asynchronous response too
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
        return msg_iev_build_webix(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }


    const char *agent_id = kw_get_str(kw, "agent_id", "", 0);

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", GCLASS_IEVENT_SRV_NAME,
        "__state__", "ST_SESSION"
    );
    dl_list_t *dl_childs = gobj_match_childs_tree(priv->gobj_input_side, 0, jn_filter);

    int some = 0;
    hgobj child; rc_instance_t *i_hs;
    i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
    while(i_hs) {
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(jn_attrs, "id", "", 0);
            const char *host_ = kw_get_str(jn_attrs, "__md_iev__`ievent_gate_stack`0`host", "", 0);
            if(strcmp(id_, agent_id)!=0 && strcmp(host_, agent_id)!=0) {
                i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
                continue;
            }
        }

        gobj_send_event( // debe retornar siempre 0.
            child,
            "EV_DROP",
            json_object(),
            src
        );
        some++;

        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
    }

    rc_free_iter(dl_childs, TRUE, 0);


    return msg_iev_build_webix(gobj, // Asynchronous response too
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




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int process_msg(
    hgobj gobj,
    json_t *kw,  // NOT owned
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *id = kw_get_str(kw, "id", "", KW_REQUIRED);
    if(empty_string(id)) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Message without id",
            NULL
        );
        log_debug_json(0, kw, "Message without id");
        return 0; // que devuelva ack para que borre el msg
    }

    /*--------------------------------*
     *  Get device of track
     *  Create it if not exist
     *--------------------------------*/
    json_t *device = gobj_get_node(
        priv->gobj_treedb_controlcenter,
        "devices",
        json_incref(kw),
        0,
        src
    );

    if(!device) {
        /*
         *  Nuevo device, crea registro
         */
        time_t t;
        time(&t);
        BOOL enabled_new_devices = gobj_read_bool_attr(gobj, "enabled_new_devices");
        json_t *jn_device = json_pack("{s:s, s:s, s:b, s:{}, s:s, s:I}",
            "id", id,
            "name", id,
            "enabled", enabled_new_devices,
            "properties",
            "yuno", kw_get_str(kw, "yuno", "", 0),
            "time", (json_int_t)t
        );

        device = gobj_create_node(
            priv->gobj_treedb_controlcenter,
            "devices",
            jn_device,
            0,
            src
        );
    }

    if(!device) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Device ???",
            NULL
        );
        return 0; // que devuelva ack para que borre el msg
    }

    /*
     *  Actualiza nombre si ha cambiado en el dispositivo
     */
    const char *old_name = kw_get_str(device, "name", "", 0);
    if(empty_string(old_name)) {
        old_name = id;
    }

//    /*--------------------------------*
//     *      Build track message
//     *--------------------------------*/
//    json_t *msg = build_track_message(
//        gobj,
//        device,
//        kw  // not owned
//    );
//
//    /*
//     *  Pon el name del device
//     */
//    json_object_set_new(
//        msg,
//        "name",
//        json_string(old_name)
//    );
//
//    msg = filtra_msg(gobj, msg);
//    if(!msg) {
//        log_info(0,
//            "gobj",         "%s", gobj_full_name(gobj),
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INFO,
//            "msg",          "%s", "Message invalid",
//            NULL
//        );
//        JSON_DECREF(device);
//        return 0; // que devuelva ack para que borre el msg
//    }
//
//    /*------------------------------------*
//     *      Save track message to trmsg
//     *------------------------------------*/
//    md_record_t md_record;
//    trmsg_add_instance(
//        priv->tranger_tracks,
//        "raw_tracks",
//        json_incref(msg),
//        0,
//        &md_record
//    );
//
//    /*---------------------------------------*
//     *      Save track message to postgres
//     *---------------------------------------*/
//    json_object_set_new(msg, "rowid", json_integer(md_record.__rowid__));
//    json_object_set_new(
//        msg,
//        "_dba_postgres",
//        json_pack("{s:s, s:s, s:O}",
//            "database", gobj_yuno_realm_owner(),
//            "topic_name", DBA_POSTGRES_TRACKS_PUREZADB,
//            "schema", priv->postgres_schema_tracks_purezadb
//        )
//    );
//    int ret = gobj_send_event(
//        priv->gobj_dba_postgres,
//        "EV_SEND_MESSAGE",
//        json_incref(msg),
//        gobj
//    );

    /*---------------------*
     *      Free device
     *---------------------*/
    JSON_DECREF(device);

//    /*--------------------------------*
//     *      Publish the trace
//     *--------------------------------*/
//    json_t * kw2publish = msg_iev_build_webix2(
//        gobj,
//        0,
//        0,
//        0,
//        msg, // owned
//        0,
//        "__publishing__"
//    );
//    gobj_publish_event(gobj, "EV_REALTIME_TRACK", kw2publish);
//
//    return ret;
    return 0;
}

/***************************************************************************
 *  Todo child debería tener data, device solo si es
 ***************************************************************************/
PRIVATE int add_devices_callback(
    json_t *parent, // not owned
    json_t *node, // not owned
    void *user_data
)
{
    hgobj gobj = user_data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *data = kw_get_list(node, "data", 0, 0);
    if(!data) {
        // Must be a device added to "data"
        return 0;
    }

    // Add id as value (for webix tree pattern)
    json_object_set_new(
        node,
        "value",
        json_string(
            kw_get_str(node, "id", "", KW_REQUIRED)
        )
    );

    json_t *devices = kw_get_list(node, "devices", 0, KW_REQUIRED|KW_EXTRACT);
    if(json_array_size(devices)==0) {
        json_object_set_new( // Open pure groups by default
            node,
            "open",
            json_true()
        );
    }

    int idx; json_t *jn_device;
    json_array_foreach(devices, idx, jn_device) {
        const char *id = kw_get_str(jn_device, "id", "", KW_REQUIRED);
        json_t *device = gobj_get_node(
            priv->gobj_treedb_controlcenter,
            "devices",
            json_pack("{s:s}",
                "id", id
            ),
            json_pack("{s:b}", "list_dict", 1),  // fkey,hook options
            gobj
        );
        json_object_set_new(device, "device", json_true()); // Marca como device

        // Add name as value (for webix tree pattern)
        json_object_set_new(
            device,
            "value",
            json_string(
                kw_get_str(device, "name", "", KW_REQUIRED)
            )
        );

        json_array_append_new(data, device); // Añade el device a "data"
        add_jtree_path(node, device); // Crea __path__
    }

    json_decref(devices);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *build_webix_tree_by_device_groups(
    hgobj gobj,
    json_t *user, // not owned
    int *result
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *user_groups = kw_get_list(user, "user_groups", 0, KW_REQUIRED);
    if(!user_groups) {
        *result = -1;
        return json_sprintf("No user groups");
    }

    json_t *jn_tree = json_array();

    int idx; json_t *jn_user_group;
    json_array_foreach(user_groups, idx, jn_user_group) {
        const char *id = kw_get_str(jn_user_group, "id", "", KW_REQUIRED);
        const char *topic_name = kw_get_str(jn_user_group, "topic_name", "", KW_REQUIRED);
        if(strcmp(topic_name, "device_groups")!=0) {
            continue;
        }

        json_t *root = gobj_topic_jtree( // Return MUST be decref
            priv->gobj_treedb_controlcenter,
            "device_groups",
            "device_groups",
            "data", // change the hook name in the tree response
            json_pack("{s:s}", "id", id),
            0,  // filter to match records
            0,  // fkey,hook options
            gobj
        );
        json_array_append_new(jn_tree, root);

        kwid_walk_childs(
            root,
            "data",
            add_devices_callback,
            (void *)gobj
        );
    }

    json_decref(user_groups);

    return jn_tree;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *build_webix_tree_by_user_groups(
    hgobj gobj,
    json_t *user, // not owned
    int *result
)
{
    *result = -1;
    return json_sprintf("Option not implemented");
}




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
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    const char *dst_service = json_string_value(
        gobj_read_user_data(channel_gobj, "tty_mirror_dst_service")
    );

    if(!empty_string(dst_service)) {
        hgobj gobj_requester = gobj_child_by_name(
            gobj_find_service("__top_side__", TRUE),
            dst_service,
            0
        );

        if(!gobj_requester) {
            // Debe venir del agent
        }

        if(gobj_requester) {
            gobj_write_user_data(channel_gobj, "tty_mirror_dst_service", json_string(""));
            gobj_send_event(gobj_requester, "EV_DROP", 0, gobj);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  HACK nodo intermedio
 ***************************************************************************/
PRIVATE int ac_stats_yuno_answer(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    json_t *jn_ievent_id = msg_iev_pop_stack(kw, IEVENT_MESSAGE_AREA_ID);
    const char *dst_service = kw_get_str(jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        gobj_find_service("__top_side__", TRUE),
        dst_service,
        0
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(kw, IEVENT_MESSAGE_AREA_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
    json_t *kw_redirect = msg_iev_answer(gobj, kw, kw, 0); // "__answer__"

    return gobj_send_event(
        gobj_requester,
        event,
        kw_redirect,
        gobj
    );
}

/***************************************************************************
 *  HACK nodo intermedio
 ***************************************************************************/
PRIVATE int ac_command_yuno_answer(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    json_t *jn_ievent_id = msg_iev_pop_stack(kw, IEVENT_MESSAGE_AREA_ID);
    const char *dst_service = kw_get_str(jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        gobj_find_service("__top_side__", TRUE),
        dst_service,
        0
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(kw, IEVENT_MESSAGE_AREA_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
    json_t *kw_redirect = msg_iev_answer(gobj, kw, kw, 0); // "__answer__"

    return gobj_send_event(
        gobj_requester,
        event,
        kw_redirect,
        gobj
    );
}

/***************************************************************************
 *  HACK nodo intermedio
 ***************************************************************************/
PRIVATE int ac_tty_mirror_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    json_t *jn_ievent_id = msg_iev_pop_stack(kw, IEVENT_MESSAGE_AREA_ID);
    const char *dst_service = kw_get_str(jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        gobj_find_service("__top_side__", TRUE),
        dst_service,
        0
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(kw, IEVENT_MESSAGE_AREA_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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

    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    gobj_write_user_data(channel_gobj, "tty_mirror_dst_service", json_string(dst_service));

    JSON_DECREF(jn_ievent_id);

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_answer(gobj, kw, kw, 0); // "__answer__"

    return gobj_send_event(
        gobj_requester,
        event,
        kw_redirect,
        gobj
    );
}

/***************************************************************************
 *  HACK nodo intermedio
 ***************************************************************************/
PRIVATE int ac_tty_mirror_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    json_t *jn_ievent_id = msg_iev_pop_stack(kw, IEVENT_MESSAGE_AREA_ID);
    const char *dst_service = kw_get_str(jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        gobj_find_service("__top_side__", TRUE),
        dst_service,
        0
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(kw, IEVENT_MESSAGE_AREA_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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

    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    gobj_write_user_data(channel_gobj, "tty_mirror_dst_service", json_string(""));

    JSON_DECREF(jn_ievent_id);

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_answer(gobj, kw, kw, 0); // "__answer__"

    return gobj_send_event(
        gobj_requester,
        event,
        kw_redirect,
        gobj
    );
}

/***************************************************************************
 *  HACK nodo intermedio
 ***************************************************************************/
PRIVATE int ac_tty_mirror_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    json_t *jn_ievent_id = msg_iev_pop_stack(kw, IEVENT_MESSAGE_AREA_ID);
    const char *dst_service = kw_get_str(jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        gobj_find_service("__top_side__", TRUE),
        dst_service,
        0
    );
    JSON_DECREF(jn_ievent_id);

    if(!gobj_requester) {
        // Debe venir del agent
        jn_ievent_id = msg_iev_get_stack(kw, IEVENT_MESSAGE_AREA_ID, 0);
        JSON_INCREF(jn_ievent_id);
        dst_service = kw_get_str(jn_ievent_id, "dst_service", "", 0);
        gobj_requester = gobj_find_service(dst_service, TRUE);
    }

    if(!gobj_requester) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
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
    json_t *kw_redirect = msg_iev_answer(gobj, kw, kw, 0); // "__answer__"

    return gobj_send_event(
        gobj_requester,
        event,
        kw_redirect,
        gobj
    );
}

/***************************************************************************
 *  HACK nodo intermedio, pero al reves(???)
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
                msg_iev_build_webix(
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
    const char *agent_id = kw_get_str(kw, "agent_id", "", 0);

    json_t *jn_filter = json_pack("{s:s, s:s}",
        "__gclass_name__", GCLASS_IEVENT_SRV_NAME,
        "__state__", "ST_SESSION"
    );
    dl_list_t *dl_childs = gobj_match_childs_tree(priv->gobj_input_side, 0, jn_filter);

    int some = 0;
    hgobj child; rc_instance_t *i_hs;
    i_hs = rc_first_instance(dl_childs, (rc_resource_t **)&child);
    while(i_hs) {
        json_t *jn_attrs = gobj_read_json_attr(child, "identity_card");
        if(!empty_string(agent_id)) {
            const char *id_ = kw_get_str(jn_attrs, "id", "", 0);
            if(strcmp(id_, agent_id)!=0) {
                i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
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

        i_hs = rc_next_instance(i_hs, (rc_resource_t **)&child);
    }

    rc_free_iter(dl_childs, TRUE, 0);

    if(!some) {
        gobj_send_event(src, "EV_DROP", 0, gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_list_groups(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    (*priv->prxMsgs)++;
    priv->rxMsgsec++;

    int result = 0;
    json_t *jn_data = 0;
    json_t *jn_comment = 0;

    do {
        /*----------------------------------------*
         *  Check AUTHZS
         *----------------------------------------*/
        const char *permission = "list-groups";
        if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
            jn_comment = json_sprintf("No permission to '%s'", permission);
            result = -1;
            break;
        }

        /*
         *  Check parameters
         */
        const char *topic_name = kw_get_str(kw, "topic_name", "", KW_REQUIRED);
        if(empty_string(topic_name)) {
            jn_comment = json_sprintf("What topic_name?");
            result = -1;
            break;
        }

        const char *username = kw_get_str(kw, "__temp__`__username__", "", KW_REQUIRED);
        json_t *user = gobj_get_node(
            priv->gobj_treedb_controlcenter,
            "users",
            json_pack("{s:s}",
                "id", username
            ),
            json_pack("{s:b}", "list_dict", 1),  // fkey,hook options
            src
        );
        if(!user) {
            jn_comment = json_sprintf("User not found: '%s'", username);
            result = -1;
            break;
        }

        if(strcmp(topic_name, "device_groups")==0 ) {
            jn_data = build_webix_tree_by_device_groups(gobj, user, &result);
            if(result < 0) {
                jn_comment = jn_data;
                jn_data = 0;
            }

        } else if(strcmp(topic_name, "user_groups")==0) {
            jn_data = build_webix_tree_by_user_groups(gobj, user, &result); // TODO
            if(result < 0) {
                jn_comment = jn_data;
                jn_data = 0;
            }

        } else {
            jn_comment = json_sprintf("Topic name must be 'device_groups' or 'user_groups'");
            result = -1;
            break;
        }

        json_decref(user);

    } while(0);

    /*
     *  Response
     */
    json_t *iev = iev_create2(
        event,
        msg_iev_build_webix2(gobj,
            result,
            jn_comment,
            0,
            jn_data?jn_data:json_array(),  // owned
            json_incref(kw),  // owned, increase for use below
            "__answer__"
        ),
        kw // owned
    );

    /*
     *  Inform
     */
    return gobj_send_event(
        src,
        "EV_SEND_IEV",
        iev,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_list_tracks(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    (*priv->prxMsgs)++;
    priv->rxMsgsec++;

    int result = 0;
    json_t *jn_data = 0;
    json_t *jn_comment = 0;

//    do {
//        /*----------------------------------------*
//         *  Check AUTHZS
//         *----------------------------------------*/
//        const char *permission = "list-tracks";
//        if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
//            jn_comment = json_sprintf("No permission to '%s'", permission);
//            result = -1;
//            break;
//        }
//
//        /*
//         *  Get track list
//         */
//        KW_INCREF(kw);
//        json_t *list = trmsg_open_list(
//            priv->tranger_tracks,
//            "raw_tracks",
//            kw
//        );
//        // WARNING aquí no podemos aplicar kw como filtro,
//        // tendría que venir dentro de kw en una key tipo "filter" de tercer nivel
//        jn_data = trmsg_data_tree(list, 0);
//
//        trmsg_close_list(priv->tranger_tracks, list);
//
//    } while(0);

    /*
     *  Response
     */
    json_t *iev = iev_create2(
        event,
        msg_iev_build_webix2(gobj,
            result,
            jn_comment,
            0,
            jn_data?jn_data:json_array(),  // owned
            json_incref(kw),  // owned, increase for use below
            "__answer__"
        ),
        kw // owned
    );

    /*
     *  Inform
     */
    return gobj_send_event(
        src,
        "EV_SEND_IEV",
        iev,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_treedb_node_create(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *treedb_name = kw_get_str(kw, "treedb_name", "", KW_REQUIRED);
    const char *topic_name = kw_get_str(kw, "topic_name", "", KW_REQUIRED);
    json_t *node_ = kw_get_dict(kw, "node", 0, KW_REQUIRED);

    if(strcmp(treedb_name, "treedb_purezadb")==0 &&
        strcmp(topic_name, "users")==0) {
        /*--------------------------------*
         *  Get user
         *  Create it if not exist
         *  Han creado el user en la tabla users de treedb_purezadb
         *  Puede que exista o no en la users de authzs
         *--------------------------------*/
        const char *username = kw_get_str(node_, "id", "", KW_REQUIRED);
        json_t *webix = gobj_command(
            priv->gobj_authz,
            "user-roles",
            json_pack("{s:s}",
                "username", username
            ),
            gobj
        );
        if(json_array_size(kw_get_dict_value(webix, "data", 0, KW_REQUIRED))==0) {
            gobj_send_event(
                priv->gobj_authz,
                "EV_ADD_USER",
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

    const char *treedb_name = kw_get_str(kw, "treedb_name", "", KW_REQUIRED);
    const char *topic_name = kw_get_str(kw, "topic_name", "", KW_REQUIRED);
    json_t *node_ = kw_get_dict(kw, "node", 0, KW_REQUIRED);

    if(strcmp(treedb_name, "treedb_purezadb")==0 &&
        strcmp(topic_name, "users")==0) {
        /*--------------------------------*
         *  Get user
         *  Create it if not exist
         *  Han creado el user en la tabla users de treedb_purezadb
         *  Puede que exista o no en la users de authzs
         *--------------------------------*/
        BOOL enabled = kw_get_bool(node_, "enabled", 0, KW_REQUIRED);
        const char *username = kw_get_str(node_, "id", "", KW_REQUIRED);
        json_t *webix = gobj_command(
            priv->gobj_authz,
            "user-roles",
            json_pack("{s:s}",
                "username", username
            ),
            gobj
        );

        if(json_array_size(kw_get_list(webix, "data", 0, KW_REQUIRED))==0) {
            gobj_send_event(
                priv->gobj_authz,
                "EV_ADD_USER",
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
                "EV_ADD_USER",
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

    const char *treedb_name = kw_get_str(kw, "treedb_name", "", KW_REQUIRED);
    const char *topic_name = kw_get_str(kw, "topic_name", "", KW_REQUIRED);
    json_t *node_ = kw_get_dict(kw, "node", 0, KW_REQUIRED);

    if(strcmp(treedb_name, "treedb_purezadb")==0 &&
        strcmp(topic_name, "users")==0) {
        /*--------------------------------*
         *  Get user
         *  Create it if not exist
         *  Han creado el user en la tabla users de treedb_purezadb
         *  Puede que exista o no en la users de authzs
         *--------------------------------*/
        const char *username = kw_get_str(node_, "id", "", KW_REQUIRED);
        gobj_send_event(
            priv->gobj_authz,
            "EV_REJECT_USER",
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
 {
    "username": "yuneta_agent@mulesol.es",
    "dst_service": "controlcenter",
    "user": {
        "id": "yuneta_agent@mulesol.es",
        "roles": [
            {
                "id": "controlcenter",
                "topic_name": "roles",
                "hook_name": "users"
            }
        ],
        "disabled": false,
        "properties": {},
        "time": 0,
        "__sessions": {
            "793e8631-8356-47ec-b811-e2efe2fd2ec8": {
                "id": "793e8631-8356-47ec-b811-e2efe2fd2ec8",
                "channel_gobj": 94438141520952
            }
        },
        "_geometry": {
            "x": 990,
            "y": 340,
            "width": 110,
            "height": 80,
            "__origin__": "me-6fcce598-0987-4c34-9fb0-0de8d998cd3d"
        },
        "__md_treedb__": {
            "treedb_name": "treedb_authzs",
            "topic_name": "users",
            "__rowid__": 45,
            "__t__": 1631448352,
            "__tm__": 0,
            "__tag__": 0,
            "__pure_node__": false
        }
    },
    "session": {
        "id": "793e8631-8356-47ec-b811-e2efe2fd2ec8",
        "channel_gobj": 94438141520952
    },
    "services_roles": {
        "controlcenter": [
            "controlcenter"
        ]
    },
    "jwt_payload": {
        "exp": 1977040064,
        "iat": 1631440064,
        "auth_time": 0,
        "jti": "efe5db43-2b30-43a6-9411-328303783d15",
        "iss": "http://localhost:8281/auth/realms/mulesol",
        "aud": "yunetacontrol",
        "sub": "f24512d4-2618-4e1f-9b64-10ae7d46e07d",
        "typ": "ID",
        "azp": "yunetacontrol",
        "session_state": "793e8631-8356-47ec-b811-e2efe2fd2ec8",
        "at_hash": "VoqtYD61IdSxvuDegzBJoA",
        "acr": "1",
        "email_verified": true,
        "name": "Yuneta Agent",
        "preferred_username": "yuneta_agent@mulesol.es",
        "given_name": "Yuneta Agent",
        "email": "yuneta_agent@mulesol.es"
    }
}

DEBUG: {
    "username": "ginsmar@mulesol.es",
    "dst_service": "controlcenter",
    "user": {
        "id": "ginsmar@mulesol.es",
        "roles": [
            {
                "id": "root",
                "topic_name": "roles",
                "hook_name": "users"
            }
        ],
        "disabled": false,
        "properties": {},
        "time": 0,
        "__sessions": {
            "38e11e29-823c-40c3-adb3-a45719364c9c": {
                "id": "38e11e29-823c-40c3-adb3-a45719364c9c",
                "channel_gobj": 94438141390792
            }
        },
        "_geometry": {
            "x": 400,
            "y": 350,
            "width": 110,
            "height": 80,
            "__origin__": "me-6fcce598-0987-4c34-9fb0-0de8d998cd3d"
        },
        "__md_treedb__": {
            "treedb_name": "treedb_authzs",
            "topic_name": "users",
            "__rowid__": 41,
            "__t__": 1631448225,
            "__tm__": 0,
            "__tag__": 0,
            "__pure_node__": false
        }
    },
    "session": {
        "id": "38e11e29-823c-40c3-adb3-a45719364c9c",
        "channel_gobj": 94438141390792
    },
    "services_roles": {
        "controlcenter": [
            "root"
        ],
        "treedb_controlcenter": [
            "root"
        ],
        "treedb_authzs": [
            "root"
        ]
    },
    "jwt_payload": {
        "exp": 1667026950,
        "iat": 1666970907,
        "jti": "73860e4c-d79d-483e-830b-64c9b65c5e1b",
        "iss": "https://localhost:8641/auth/realms/mulesol",
        "aud": [
            "realm-management",
            "account"
        ],
        "sub": "0a1e5c27-80f1-4225-943a-edfbc204972d",
        "typ": "Bearer",
        "azp": "yunetacontrol",
        "session_state": "38e11e29-823c-40c3-adb3-a45719364c9c",
        "acr": "1",
        "allowed-origins": [
            "*"
        ],
        "realm_access": {
            "roles": [
                "offline_access",
                "uma_authorization"
            ]
        },
        "resource_access": {
            "realm-management": {
                "roles": [
                    "manage-users",
                    "view-users",
                    "query-clients",
                    "manage-clients",
                    "query-groups",
                    "query-users"
                ]
            },
            "account": {
                "roles": [
                    "manage-account",
                    "manage-account-links",
                    "view-profile"
                ]
            }
        },
        "scope": "profile email",
        "sid": "38e11e29-823c-40c3-adb3-a45719364c9c",
        "email_verified": true,
        "name": "Ginés Martínez",
        "preferred_username": "ginsmar@mulesol.es",
        "locale": "en",
        "given_name": "Ginés",
        "family_name": "Martínez",
        "email": "ginsmar@mulesol.es"
    }
}

 ***************************************************************************/
PRIVATE int ac_user_login(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(kw, "username", "", KW_REQUIRED);
//     const char *dst_service = kw_get_str(kw, "dst_service", "", KW_REQUIRED);
//     json_t *user_ = kw_get_dict(kw, "user", 0, KW_REQUIRED);
//     json_t *session_ = kw_get_dict(kw, "session", 0, KW_REQUIRED);
//     json_t *services_roles_ = kw_get_dict(kw, "services_roles", 0, KW_REQUIRED);

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
//     const char *username = kw_get_str(kw, "username", "", KW_REQUIRED);
//     json_t *user_ = kw_get_dict(kw, "user", 0, KW_REQUIRED);
//     json_t *session_ = kw_get_dict(kw, "session", 0, KW_REQUIRED);

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

    const char *username = kw_get_str(kw, "username", "", KW_REQUIRED);
    const char *dst_service = kw_get_str(kw, "dst_service", "", KW_REQUIRED);

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

    uint64_t maxtxMsgsec = gobj_read_uint64_attr(gobj, "maxtxMsgsec");
    uint64_t maxrxMsgsec = gobj_read_uint64_attr(gobj, "maxrxMsgsec");
    if(priv->txMsgsec > maxtxMsgsec) {
        gobj_write_uint64_attr(gobj, "maxtxMsgsec", priv->txMsgsec);
    }
    if(priv->rxMsgsec > maxrxMsgsec) {
        gobj_write_uint64_attr(gobj, "maxrxMsgsec", priv->rxMsgsec);
    }

    gobj_write_uint64_attr(gobj, "txMsgsec", priv->txMsgsec);
    gobj_write_uint64_attr(gobj, "rxMsgsec", priv->rxMsgsec);

    priv->rxMsgsec = 0;
    priv->txMsgsec = 0;

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    // top input
    {"EV_MT_COMMAND_ANSWER",    EVF_PUBLIC_EVENT,   0,  0},
    {"EV_TTY_DATA",             EVF_PUBLIC_EVENT,   0, 0},
    {"EV_MT_STATS_ANSWER",      EVF_PUBLIC_EVENT,   0,  0},
    {"EV_WRITE_TTY",            0,  0,  0},
    {"EV_ON_OPEN",              0,  0,  0},
    {"EV_ON_CLOSE",             0,  0,  0},
    {"EV_TTY_OPEN",             EVF_PUBLIC_EVENT,   0, 0},
    {"EV_TTY_CLOSE",            EVF_PUBLIC_EVENT,   0, 0},

    {"EV_AUTHZ_USER_LOGIN",     0,  0,  0},
    {"EV_AUTHZ_USER_LOGOUT",    0,  0,  0},
    {"EV_AUTHZ_USER_NEW",       0,  0,  0},

    {"EV_TREEDB_NODE_CREATED",  0,  0,  0},
    {"EV_TREEDB_NODE_UPDATED",  0,  0,  0},
    {"EV_TREEDB_NODE_DELETED",  0,  0,  0},

    // bottom input
    {"EV_TIMEOUT",              0,  0,  0},
    {"EV_STOPPED",              0,  0,  0},
    // internal
    {NULL, 0, 0, ""}
};
PRIVATE const EVENT output_events[] = {
    {"EV_LIST_TRACKS",      EVF_PUBLIC_EVENT,  0,  0},
    {"EV_LIST_GROUPS",      EVF_PUBLIC_EVENT,  0,  0},
    {"EV_REALTIME_TRACK",   EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS,  0,  0},
    {NULL, 0, 0, ""}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_MT_STATS_ANSWER",          ac_stats_yuno_answer,       0},
    {"EV_MT_COMMAND_ANSWER",        ac_command_yuno_answer,     0},
    {"EV_TTY_DATA",                 ac_tty_mirror_data,         0},
    {"EV_WRITE_TTY",                ac_write_tty,               0},

    {"EV_TREEDB_NODE_CREATED",      ac_treedb_node_create,      0},
    {"EV_TREEDB_NODE_UPDATED",      ac_treedb_node_updated,     0},
    {"EV_TREEDB_NODE_DELETED",      ac_treedb_node_deleted,     0},

    {"EV_AUTHZ_USER_LOGIN",         ac_user_login,              0},
    {"EV_AUTHZ_USER_LOGOUT",        ac_user_logout,             0},
    {"EV_AUTHZ_USER_NEW",           ac_user_new,                0},

    {"EV_ON_OPEN",                  ac_on_open,                 0},
    {"EV_ON_CLOSE",                 ac_on_close,                0},
    {"EV_TTY_OPEN",                 ac_tty_mirror_open,         0},
    {"EV_TTY_CLOSE",                ac_tty_mirror_close,        0},
    {"EV_TIMEOUT",                  ac_timeout,                 0},
    {"EV_STOPPED",                  0,                          0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_CONTROLCENTER_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        mt_play,
        mt_pause,
        mt_writing,
        0, //mt_reading,
        mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
        0, //mt_command_parser,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_save_resource,
        0, //mt_delete_resource,
        0, //mt_future21
        0, //mt_future22
        0, //mt_get_resource
        0, //mt_state_changed,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_agents,
        0, //mt_future44,
        0, //mt_unlink_agents,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_agents,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    authz_table,  // acl
    s_user_trace_level,
    command_table,  // command_table
    0,  // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_controlcenter(void)
{
    return &_gclass;
}
