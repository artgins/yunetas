/***********************************************************************
 *          C_MQTT_BROKER.C
 *          Mqtt_broker GClass.
 *
 *          Mqtt broker
 *
 *          Copyright (c) 2025,2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <limits.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>

#include "treedb_schema_mqtt_broker.c"
#include "tr2_queue.h"
#include "tr2q_mqtt.h"
#include "c_mqtt_broker.h"

#include "c_prot_mqtt2.h" // TODO remove when moved to kernel

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SUBS_KEY    "@subs"     /* Key for subscribers array in tree nodes */

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int broadcast_tranger_queues(hgobj gobj);
PRIVATE int open_database(hgobj gobj);
PRIVATE int close_database(hgobj gobj);
PRIVATE void collect_all_subscribers_recursive(hgobj gobj, json_t *node, json_t *result);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_devices(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_normal_subscribers(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_shared_subscribers(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_flatten_subscribers(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

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
PRIVATE sdata_desc_t pm_device[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "device_id",    0,              0,          "Device ID"),
SDATAPM (DTP_BOOLEAN,   "opened",       0,              "1",        "List only connected devices"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_subscribers[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_BOOLEAN,   "shared",       0,              "0",        "List shared subscribers, else normal"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------alias---items-------json_fn-------------description---------- */
SDATACM (DTP_SCHEMA,    "help",         a_help, pm_help,    cmd_help,           "Command's help"),
SDATACM (DTP_SCHEMA,    "list-devices", 0,      pm_device,  cmd_list_devices,   "List devices"),
SDATACM (DTP_SCHEMA,    "normal-subs",  0,      0,          cmd_normal_subscribers, "List normal subscribers"),
SDATACM (DTP_SCHEMA,    "shared-subs",  0,      0,          cmd_shared_subscribers, "List shared subscribers"),
SDATACM (DTP_SCHEMA,    "flatten-subs", 0,      pm_subscribers, cmd_flatten_subscribers, "Flatten subscribers"),

/*-CMD2---type----------name----------------flag----alias---items---------------json_fn-------------description--*/
SDATACM2 (DTP_SCHEMA,   "authzs",           0,      0,      pm_authzs,          cmd_authzs,         "Authorization's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag--------default-----description---------- */
SDATA (DTP_BOOLEAN, "enable_new_clients",0,         "0",        "Set true if you want auto-create new clients if they don't exist"),

SDATA (DTP_BOOLEAN, "mqtt_persistent_db",0,         "1",        "Set true if you want persistent database for Clients, Topics, Inflight and Queued Messages in mqtt broker side"),
SDATA (DTP_STRING,  "mqtt_service",     SDF_RD,     "",         "Mqtt service name, if it's empty then it will be the yuno_role"),
SDATA (DTP_STRING,  "mqtt_tenant",      SDF_RD,     "",         "Used for multi-tenant service, if it's empty then it will be the yuno_name"),

// TODO a 0 cuando funcionen bien los out schemas
SDATA (DTP_BOOLEAN, "use_internal_schema",SDF_PERSIST, "1",     "Use internal (hardcoded) schema"),

SDATA (DTP_INTEGER, "on_critical_error",SDF_RD,     "2",        "LOG_OPT_EXIT_ZERO exit on error (Zero to avoid restart)"),
SDATA (DTP_POINTER, "subscriber",       0,          0,          "Subscriber of output-events. If it's null then the subscriber is the parent."),
SDATA (DTP_INTEGER, "timeout",          SDF_RD,     "1000",     "Timeout"),
SDATA (DTP_POINTER, "user_data",        0,          0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,          0,          "more user data"),
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
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    int32_t timeout;
    BOOL enable_new_clients;

    hgobj gobj_authz;
    hgobj gobj_input_side;
    hgobj gobj_top_side;

    hgobj gobj_tranger_queues;
    json_t *tranger_queues;

    hgobj gobj_treedbs;
    hgobj gobj_treedb_mqtt_broker;      // service of treedb_mqtt_broker (create in gobj_treedbs)
    json_t *tranger_treedb_mqtt_broker;

    json_t *normal_subs;
    json_t *shared_subs;

    char treedb_mqtt_broker_name[80];
    char msg2db_alarms_name[80]; // TODO review, is used?

    char treedb_name[NAME_MAX];

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
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    priv->normal_subs = json_object();
    priv->shared_subs = json_object();

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,                   gobj_read_integer_attr)
    SET_PRIV(enable_new_clients,        gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,                 gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(enable_new_clients,    gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->normal_subs)
    JSON_DECREF(priv->shared_subs)
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

    /*-----------------------------------------*
     *  Persistent DB
     *  open_database cannot be in play/pause,
     *  EV_ON_CLOSE events are important
     *  to clear sessions/clients: yuno crash
     *-----------------------------------------*/
    BOOL mqtt_persistent_db = gobj_read_bool_attr(gobj, "mqtt_persistent_db");
    if(mqtt_persistent_db) {
        open_database(gobj);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------------*
     *  Persistent DB
     *  close_database cannot be in play/pause,
     *  EV_ON_CLOSE events are important
     *  to clear sessions/clients: yuno crash
     *-----------------------------------------*/
    BOOL mqtt_persistent_db = gobj_read_bool_attr(gobj, "mqtt_persistent_db");
    if(mqtt_persistent_db) {
        close_database(gobj);
    }

    gobj_unsubscribe_event(priv->gobj_authz, 0, 0, gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-------------------------*
     *      Start services
     *-------------------------*/
    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);

    priv->gobj_top_side = gobj_find_service("__top_side__", TRUE);
    gobj_subscribe_event(priv->gobj_top_side, 0, 0, gobj);

    /*-----------------------------*
     *  Broadcast timeranger
     *-----------------------------*/
    broadcast_tranger_queues(gobj);

    /*--------------------------------*
     *      Start
     *--------------------------------*/
    gobj_start_tree(priv->gobj_input_side);
    gobj_start_tree(priv->gobj_top_side);

    /*
     *  Periodic timer for tasks
     */
    set_timeout_periodic(priv->timer, priv->timeout); // La verdadera

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------*
     *      Stop top/input side
     *-----------------------------*/
    gobj_stop_tree(priv->gobj_top_side);
    gobj_stop_tree(priv->gobj_input_side);

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
    KW_INCREF(kw)
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
static const json_desc_t devices_desc[] = { // HACK must match with gobj_services()
// Name             Type        Defaults    Fillspace
{"client_id",       "string",   "",         "44"},  // First item is the pkey
{"peername",        "string",   "",         "24"},
{"clean_start",     "boolean",  "",         "7"},
{"protocol_name",   "string",   "",         "10"},
{"protocol_version","integer",  "",         "10"},
{0}
};

PRIVATE json_t *cmd_list_devices(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *device_id = kw_get_str(gobj, kw, "device_id", "", 0);
    BOOL opened = kw_get_bool(gobj, kw, "opened", 0, 0);
    json_t *jn_schema = json_desc_to_schema(devices_desc);

    json_t *jn_data = json_array();

    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", C_CHANNEL
    );
    if(opened) {
        json_object_set_new(jn_filter, "__state__", json_string(ST_OPENED));
    }

    json_t *dl_children = gobj_match_children_tree(priv->gobj_input_side, jn_filter);

    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        const char *id = gobj_read_str_attr(child, "client_id");
        if(empty_string(device_id) || strcmp(device_id, id)==0) {
            json_t *record = json_object();
            for(int i=0; devices_desc[i].name!=NULL; i++) {
                const char *name = devices_desc[i].name;
                json_object_set(
                    record,
                    name,
                    gobj_read_attr(child, name, gobj)
                );
            }

            json_array_append_new(jn_data, record);
        }
    }

    gobj_free_iter(dl_children);

    return msg_iev_build_response(gobj,
        0,
        0,
        jn_schema,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_normal_subscribers(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    // const char *device_id = kw_get_str(gobj, kw, "device_id", "", 0);

    // collect_all_subscribers_recursive(gobj, priv->normal_subs, jn_data);

    return msg_iev_build_response(gobj,
        0,
        0,
        0,
        json_incref(priv->normal_subs),
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_shared_subscribers(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return msg_iev_build_response(gobj,
        0,
        0,
        0,
        json_incref(priv->shared_subs),
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_flatten_subscribers(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL shared = kw_get_bool(gobj, kw, "shared", 0, 0);

    json_t *flatten = NULL;
    if(shared) {
        flatten = json_flatten_dict(priv->shared_subs);

    } else {
        flatten = json_flatten_dict(priv->normal_subs);

    }
    return msg_iev_build_response(gobj,
        0,
        0,
        0,
        flatten,
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int open_database(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------------------------------------------*
     *      Path of Treedb/Timeranger System
     *
     *  {yuneta_root_dir}/"store"/
     *      {C_MQTT_BROKER.mqtt_service}/
     *          {agent.environment.node_owner}/
     *              {agent.realm_id}/
     *                  {C_MQTT_BROKER.mqtt_tenant}
     *
     *  Example:
     *  "/yuneta/store/mqtt-broker-db/cia/demo.sample.com/tenant"
     *--------------------------------------------------------------*/

    /*---------------------------------------*
     *      Get Path
     *---------------------------------------*/
    const char *mqtt_service = gobj_read_str_attr(gobj, "mqtt_service");
    const char *mqtt_tenant = gobj_read_str_attr(gobj, "mqtt_tenant");
    if(empty_string(mqtt_service)) {
        mqtt_service = gobj_yuno_role();
    }
    if(empty_string(mqtt_tenant)) {
        mqtt_tenant = gobj_yuno_name();
    }

    char path[PATH_MAX];
    yuneta_realm_store_dir(
        path,
        sizeof(path),
        mqtt_service,
        gobj_yuno_realm_owner(),
        gobj_yuno_realm_id(),
        mqtt_tenant,  // tenant
        "",  // gclass-treedb controls the directories
        TRUE
    );

    /*-------------------------------------------*
     *          Create Treedb System
     *-------------------------------------------*/
    json_t *kw_treedbs = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i}",
        "path", path,
        "filename_mask", "%Y",  // to management treedbs we don't need multi-files (per day)
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
     *  Start treedbs
     */
    gobj_subscribe_event(priv->gobj_treedbs, 0, 0, gobj);
    gobj_start_tree(priv->gobj_treedbs);

    /*-------------------------------------------*
     *      Open treedb_mqtt_broker service
     *-------------------------------------------*/
    helper_quote2doublequote(treedb_schema_mqtt_broker);
    json_t *jn_treedb_schema_mqtt_broker = legalstring2json(treedb_schema_mqtt_broker, TRUE);
    if(parse_schema(jn_treedb_schema_mqtt_broker)<0) {
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

    const char *treedb_name_ = kw_get_str(gobj,
        jn_treedb_schema_mqtt_broker,
        "id",
        "treedb_mqtt_broker",
        KW_REQUIRED
    );
    snprintf(priv->treedb_mqtt_broker_name, sizeof(priv->treedb_mqtt_broker_name), "%s", treedb_name_);

    json_t *jn_resp = gobj_command(priv->gobj_treedbs,
        "open-treedb",
        json_pack("{s:s, s:s, s:i, s:s, s:o, s:b}",
            "__username__", gobj_read_str_attr(gobj_yuno(), "__username__"),
            "filename_mask", "%Y",
            "exit_on_error", 0,
            "treedb_name", priv->treedb_mqtt_broker_name,
            "treedb_schema", jn_treedb_schema_mqtt_broker,
            "use_internal_schema", use_internal_schema
        ),
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

    priv->gobj_treedb_mqtt_broker = gobj_find_service(priv->treedb_mqtt_broker_name, TRUE);
    gobj_subscribe_event(priv->gobj_treedb_mqtt_broker, 0, 0, gobj);

    // Get timeranger of treedb_mqtt_broker, will be used for alarms too
    priv->tranger_treedb_mqtt_broker = gobj_read_pointer_attr(priv->gobj_treedb_mqtt_broker, "tranger");

    /*---------------------------------------*
     *      Open Msg2db (ALARMS)
     *---------------------------------------*/
    helper_quote2doublequote(msg2db_schema_alarms);
    json_t *jn_msg2db_schema_alarms = legalstring2json(msg2db_schema_alarms, TRUE);
    if(parse_schema(jn_msg2db_schema_alarms)<0) {
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

    /*
     *  Use the same tranger as treedb_airedb, a "%Y" based.
     */
    const char *msg2db_name_ = kw_get_str(gobj,
        jn_msg2db_schema_alarms,
        "id",
        "msg2db_alarms",
        KW_REQUIRED
    );
    snprintf(priv->msg2db_alarms_name, sizeof(priv->msg2db_alarms_name), "%s", msg2db_name_);

    msg2db_open_db(
        priv->tranger_treedb_mqtt_broker,
        priv->msg2db_alarms_name,   // msg2db_name, got from jn_schema
        jn_msg2db_schema_alarms,    // owned
        "persistent"
    );

    /*---------------------------------------*
     *      Open qmsgs Timeranger
     *---------------------------------------*/
    yuneta_realm_store_dir(
        path,
        sizeof(path),
        mqtt_service,
        gobj_yuno_realm_owner(),
        gobj_yuno_realm_id(),
        mqtt_tenant,  // tenant
        "qmsgs",
        TRUE
    );

    json_t *kw_tranger_qmsgs = json_pack("{s:s, s:b, s:i}",
        "path", path,
        "master", 1,
        "on_critical_error", (int)(LOG_OPT_EXIT_ZERO)
    );
    priv->gobj_tranger_queues = gobj_create_service(
        "tranger_queues",
        C_TRANGER,
        kw_tranger_qmsgs,
        gobj
    );
    gobj_start(priv->gobj_tranger_queues);

    priv->tranger_queues = gobj_read_pointer_attr(priv->gobj_tranger_queues, "tranger");

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_database(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------*
     *      Close Msg2db Alarms
     *---------------------------------------*/
    msg2db_close_db(
        priv->tranger_treedb_mqtt_broker,
        priv->msg2db_alarms_name
    );

    /*---------------------------------------*
     *      Close treedb
     *---------------------------------------*/
    json_decref(gobj_command(priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s, s:s}",
            "__username__", gobj_read_str_attr(gobj_yuno(), "__username__"),
            "treedb_name", priv->treedb_mqtt_broker_name
        ),
        gobj
    ));

    /*-------------------------*
     *      Stop treedbs
     *-------------------------*/
    if(priv->gobj_treedbs) {
        gobj_unsubscribe_event(priv->gobj_treedbs, 0, 0, gobj);
        gobj_stop_tree(priv->gobj_treedbs);
        EXEC_AND_RESET(gobj_destroy, priv->gobj_treedbs)
    }

    /*-------------------------*
     *  Stop tranger_queues
     *-------------------------*/
    gobj_stop(priv->gobj_tranger_queues);
    EXEC_AND_RESET(gobj_destroy, priv->gobj_tranger_queues)
    priv->tranger_queues = 0;

    /*-----------------------------*
     *  Broadcast timeranger
     *-----------------------------*/
    broadcast_tranger_queues(gobj);

    return 0;
}

/***************************************************************************
 *  Broadcast htopic frame
 ***************************************************************************/
PRIVATE int cb_set_htopic_frame(
    hgobj child,
    void *user_data,
    void *user_data2,
    void *user_data3)
{
    const char *attr = user_data;
    void *value = user_data2;

    if(gobj_has_attr(child, attr)) {
        gobj_write_pointer_attr(child, attr, value);
    }
    return 0;
}
PRIVATE int broadcast_tranger_queues(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_walk_gobj_children_tree(
        priv->gobj_input_side,
        WALK_TOP2BOTTOM,
        cb_set_htopic_frame,
        "tranger_queues",
        priv->tranger_queues,
        NULL
    );

    return 0;
}

/***************************************************************************
 *  MQTT Subscription Management for Yunetas
 ***************************************************************************
 *  Local functions for managing MQTT subscriptions using JSON trees.
 *  Provides add, remove, and search operations with wildcard support.
 *
 *  Tree structure:
 *  {
 *    "topic_level": {
 *      "subtopic": {
 *        "@subs": {
 *          "client_id_1": {"qos": 1, "ids": [123], "options": 0},
 *          "client_id_2": {"qos": 2, "ids": [100,200], "options": 1}
 *      }
 *    }
 *  }
 ***************************************************************************/
PRIVATE void print_levels(char *prefix, char **levels) // For debugging
{
    printf("==> %s\n", prefix);
    for(int i = 0; levels[i] != NULL; i++) {
        printf("level %d: %s\n", i, levels[i]);
    }
}

/***************************************************************************
 *  strtok_hier - Hierarchical Topic Tokenizer
 *
 *  Splits strings by '/' (MQTT topic separator).
 *  Modifies the original string by inserting '\0' at '/' positions.
 ***************************************************************************/
PRIVATE char *strtok_hier(char *str, char **saveptr)
{
    if(str != NULL) {
        *saveptr = str;
    }

    if(*saveptr == NULL) {
        return NULL;
    }

    char *c = strchr(*saveptr, '/');

    if(c) {
        str = *saveptr;
        *saveptr = c + 1;
        c[0] = '\0';
    } else if(*saveptr) {
        str = *saveptr;
        *saveptr = NULL;
    }

    return str;
}

/***************************************************************************
 *  topic_tokenize - Parse MQTT topic into array of levels
 *
 *  Parameters:
 *      topic       - Input topic string (e.g., "sport/tennis/#")
 *      local_topic - Output: duplicated string (caller must free with gbmem_free)
 *      levels      - Output: NULL-terminated array (caller must free with gbmem_free)
 *      sharename   - Output: shared subscription name (NULL if not shared)
 *
 *  Returns:
 *      0 on success, -1 on error
 *
 *  Output Examples:
 *  +-----------------------------+--------------------------------+-----------+
 *  | Input subtopic              | topics[] array                 | sharename |
 *  +-----------------------------+--------------------------------+-----------+
 *  | "sport/tennis"              | ["", "sport", "tennis", NULL]  | NULL      |
 *  | "sport/tennis/#"            | ["", "sport", "tennis", "#",   | NULL      |
 *  |                             |  NULL]                         |           |
 *  | "$SYS/broker/load"          | ["$SYS", "broker", "load",     | NULL      |
 *  |                             |  NULL]                         |           |
 *  | "$share/group1/sport/tennis"| ["", "sport", "tennis", NULL]  | "group1"  |
 *  +-----------------------------+--------------------------------+-----------+
 *
 *  Key Design Notes:
 *  - Regular topics get an empty string "" prefix for uniform tree traversal
 *  - System topics ($SYS, etc.) do NOT get the prefix (start with '$')
 *  - Shared subscriptions ($share/name/topic) extract the group name and
 *    return the actual topic with the "" prefix
 *
 *  Memory Management:
 *  - Caller must free both *local_sub and *topics on success
 *  - topics[] pointers reference memory inside local_sub (don't free individually)
 ***************************************************************************/
PRIVATE int topic_tokenize(
    const char *topic,
    char **local_topic,
    char ***levels,
    const char **sharename
)
{
    size_t len = strlen(topic);
    if(len == 0) {
        return -1;
    }

    /*----------------------------------------------------------------------*
     *  Duplicate the input string
     *  We need a working copy because strtok_hier modifies it
     *  in-place by inserting '\0' characters at '/' positions
     *----------------------------------------------------------------------*/
    *local_topic = gbmem_strdup(topic);
    if((*local_topic) == NULL) {
        // Error already logged
        return -1;
    }

    /*----------------------------------------------------------------------*
     *  Count topic levels
     *          Count '/' separators to determine array size needed
     *
     *          Example: "sport/tennis/player"
     *                         ^      ^
     *                         |      +-- 2nd '/'
     *                         +-- 1st '/'
     *                   count = 3 (two separators + 1)
     *----------------------------------------------------------------------*/
    int count = 0;
    char *saveptr = *local_topic;
    while(saveptr) {
        saveptr = strchr(&saveptr[1], '/'); /* Start search from position 1 */
        count++;
    }

    /*----------------------------------------------------------------------*
     *  Allocate levels array
     *          Size = count + 3 to accommodate:
     *            - Potential empty string prefix for regular topics
     *            - Potential $share and sharename slots
     *            - NULL terminator
     *----------------------------------------------------------------------*/
    *levels = gbmem_malloc((size_t)(count + 3) * sizeof(char *));
    if((*levels) == NULL) {
        // Error already logged
        gbmem_free(*local_topic);
        *local_topic = NULL;
        return -1;
    }
    memset(*levels, 0, (size_t)(count + 3) * sizeof(char *));

    /*----------------------------------------------------------------------*
     *  Add empty string prefix for regular topics
     *          Regular topics (not starting with '$') get an empty string
     *          prepended. This normalizes the array structure so that
     *          subscription tree traversal works uniformly.
     *
     *          Why? The subscription tree root has children for:
     *            - "" (empty) -> regular topics branch
     *            - "$SYS"     -> system topics branch
     *            - "$share"   -> shared subscriptions (processed later)
     *
     *          Example results:
     *            "sport/tennis"    -> ["", "sport", "tennis", NULL]
     *            "$SYS/broker"     -> ["$SYS", "broker", NULL]
     *----------------------------------------------------------------------*/
    int level_index = 0;
    if((*local_topic)[0] != '$') {
        (*levels)[level_index] = "";
        level_index++;
    }

    /*----------------------------------------------------------------------*
     *  Tokenize the topic string
     *          Split by '/' and store pointers in the topics array
     *
     *          After this loop for "sport/tennis/player":
     *            topics[0] = ""        (from step 5)
     *            topics[1] = "sport"
     *            topics[2] = "tennis"
     *            topics[3] = "player"
     *            topics[4] = NULL      (from calloc initialization)
     *----------------------------------------------------------------------*/
    char *token = strtok_hier((*local_topic), &saveptr);
    while(token) {
        (*levels)[level_index] = token;
        level_index++;
        token = strtok_hier(NULL, &saveptr);
    }

    /*----------------------------------------------------------------------*
     *  Handle shared subscriptions ($share/groupname/topic/...)
     *
     *          MQTT 5.0 shared subscriptions format:
     *            $share/{ShareName}/{TopicFilter}
     *
     *          Example: "$share/consumer-group/sport/tennis/#"
     *            - ShareName: "consumer-group"
     *            - TopicFilter: "sport/tennis/#"
     *
     *          Processing:
     *            1. Validate format (minimum 3 levels, non-empty topic)
     *            2. Extract sharename
     *            3. Remove $share and sharename from array
     *            4. Add empty prefix (treat as regular topic)
     *
     *          Before: ["$share", "consumer-group", "sport", "tennis", "#"]
     *          After:  ["", "sport", "tennis", "#", NULL]
     *                  sharename = "consumer-group"
     *----------------------------------------------------------------------*/
    if(strcmp((*levels)[0], "$share") == 0) {
        /*
         *  Validate shared subscription format:
         *  - Must have at least 3 parts: $share, sharename, topic
         *  - Topic filter cannot be empty (count==3 with empty [2])
         */
        if(count < 3 || (count == 3 && strlen((*levels)[2]) == 0)) {
            gbmem_free(*local_topic);
            gbmem_free(*levels);
            *local_topic = NULL;
            *levels = NULL;
            return -1;
        }

        /*
         *  Extract the share group name if caller wants it
         */
        if(sharename) {
            (*sharename) = (*levels)[1];
        }

        /*
         *  Shift array to remove $share and sharename:
         *
         *  Before: [0]="$share" [1]="group" [2]="sport" [3]="tennis" [4]=NULL
         *
         *  Shift loop (i from 1 to count-2):
         *    i=1: [1] = [2] -> [1]="sport"
         *    i=2: [2] = [3] -> [2]="tennis"
         *    i=3: [3] = [4] -> [3]=NULL
         *
         *  After:  [0]="$share" [1]="sport" [2]="tennis" [3]=NULL
         *
         *  Then fix [0] and ensure NULL termination:
         *  Final:  [0]="" [1]="sport" [2]="tennis" [3]=NULL
         */
        for(int i = 1; i < count - 1; i++) {
            (*levels)[i] = (*levels)[i + 1];
        }
        (*levels)[0] = "";
        (*levels)[count - 1] = NULL;
    }

    return 0;
}

/***************************************************************************
 *  get_or_create_node - Navigate/create tree path
 *
 *  Traverses the JSON tree following the levels array.
 *  Creates intermediate nodes if they don't exist.
 *
 *  Parameters:
 *      root   - Root JSON object of the tree
 *      levels - NULL-terminated array of topic levels
 *
 *  Returns:
 *      JSON object at the leaf node, or NULL on error
 ***************************************************************************/
PRIVATE json_t *get_or_create_node(json_t *root, char **levels)
{
    json_t *current = root;

    /*
     *  Skip levels[0] (empty string for regular topics, "$SYS" for system)
     *  The root selection (normal_subs vs shared_subs) is done by caller
     */
    for(int i = 1; levels[i] != NULL; i++) {
        json_t *child = json_object_get(current, levels[i]);
        if(!child) {
            /*
             *  Node doesn't exist, create it
             */
            child = json_object();
            if(!child) {
                return NULL;
            }
            if(json_object_set_new(current, levels[i], child) < 0) {
                json_decref(child);
                return NULL;
            }
        }
        current = child;
    }

    return current;
}

/***************************************************************************
 *  collect_subscribers - Collect client_ids from @subs dict
 *
 *  Parameters:
 *      node        - JSON node that may contain @subs
 *      subscribers - JSON dict to append client_ids to
 ***************************************************************************/
PRIVATE void collect_subscribers(hgobj gobj, json_t *node, json_t *subscribers)
{
    json_t *subs = json_object_get(node, SUBS_KEY);
    if(!subs) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "@subs is NULL",
            NULL
        );
        return;
    }

    const char *client_id;
    json_t *sub_info;
    json_object_foreach(subs, client_id, sub_info) {
        json_t *existing = json_object_get(subscribers, client_id);
        if(existing) {
            /*
             *  Client already matched another subscription
             *  - Keep the highest QoS
             *  - Append subscription ID to list
             */
            int existing_qos = (int)json_integer_value(json_object_get(existing, "qos"));
            int new_qos = (int)json_integer_value(json_object_get(sub_info, "qos"));
            if(new_qos > existing_qos) {
                json_object_set_new(existing, "qos", json_integer(new_qos));
            }
            json_int_t new_id = json_integer_value(json_object_get(sub_info, "id"));
            if(new_id > 0) {
                json_t *ids = json_object_get(existing, "ids");
                json_array_append_new(ids, json_integer(new_id));
            }
        } else {
            /*
             *  First match for this client
             */
            json_t *entry = json_object();
            int qos = (int)json_integer_value(json_object_get(sub_info, "qos"));
            int options = (int)json_integer_value(json_object_get(sub_info, "options"));
            json_object_set_new(entry, "qos", json_integer(qos));
            json_object_set_new(entry, "options", json_integer(options));
            json_t *ids = json_array();
            json_object_set_new(entry, "ids", ids);

            int id = (int)json_integer_value(json_object_get(sub_info, "id"));
            if(id > 0) {
                json_array_append_new(ids, json_integer(id));
            }
            json_object_set_new(subscribers, client_id, entry);
        }
    }
}

/***************************************************************************
 *  search_recursive - Recursive wildcard-aware search
 *
 *  Searches the subscription tree matching against published topic levels.
 *  Handles '+' (single-level) and '#' (multi-level) wildcards.
 *
 *  Parameters:
 *      node        - Current JSON node in subscription tree
 *      pub_levels  - Array of published topic levels (NOT wildcards)
 *      level_index - Current index in pub_levels
 *      subscribers - Collect the subscribers
 ***************************************************************************/
PRIVATE void search_recursive( // sub__search
    hgobj gobj,
    json_t *node,
    char **pub_levels,
    int level_index,
    json_t *subscribers
)
{
    const char *current_level = pub_levels[level_index];

    /*-----------------------------------------------------------*
     *  Check '#' wildcard (matches rest of topic at any point)
     *-----------------------------------------------------------*/
    json_t *multi_wildcard = json_object_get(node, "#");
    if(multi_wildcard) {
        collect_subscribers(gobj, multi_wildcard, subscribers);
    }

    /*------------------------------------------------------------------*
     *  End of published topic - collect subscribers from current node
     *------------------------------------------------------------------*/
    if(current_level == NULL) {
        collect_subscribers(gobj, node, subscribers);
        return;
    }

    /*---------------------------------------------*
     *  Check '+' wildcard (matches single level)
     *---------------------------------------------*/
    json_t *wildcard_child = json_object_get(node, "+");
    if(wildcard_child) {
        search_recursive(gobj, wildcard_child, pub_levels, level_index + 1, subscribers);
    }

    /*---------------------*
     *  Check exact match
     *---------------------*/
    json_t *child = json_object_get(node, current_level);
    if(child) {
        search_recursive(gobj, child, pub_levels, level_index + 1, subscribers);
    }
}

/***************************************************************************
 *  sub__add - Add a subscription to the tree
 *  Parameters:
 *      gobj                    - GObj instance (for PRIVATE_DATA access)
 *      topic                   - Subscription topic (may contain wildcards)
 *      client_id               - Client identifier
 *      qos                     - Quality of Service level (0, 1, or 2)
 *      subscription_id         - MQTT 5.0 subscription identifier (0 if not used)
 *      subscription_options    - MQTT 5.0 subscription options:
 *                                  Bit 0-1: Max QoS
 *                                  Bit 2:   No Local
 *                                  Bit 3:   Retain As Published
 *                                  Bit 4-5: Retain Handling
 *
 *  Returns:
 *      0 on success,
 *      1 already exists,
 *      -1 on error
 *
 *  Example:
 *      sub__add(gobj, "home/+/temperature", "client_001", 1, 123, 0);
 *
 *  Tree Result:
 *      {
 *        "home": {
 *          "+": {
 *            "temperature": {
 *              "@subs": {
 *                "client_001": {"qos": 1, "id": 123, "options": 0}
 *              }
 *            }
 *          }
 *        }
 *      }
 *
 *  share messages
 *  --------------
 *  The $share/groupname/ prefix is only for subscriptions, never for publishing.
 *
 *  How It Works
 *

    +------------------+----------------------------------------+
    | Action           | Topic                                  |
    +------------------+----------------------------------------+
    | SUBSCRIBE        | $share/sensors/home/+/temperature      |
    | PUBLISH          | home/livingroom/temperature            |
    +------------------+----------------------------------------+

┌─────────────────────────────────────────────────────────────────────────┐
│                            MQTT BROKER                                  │
│                                                                         │
│  Subscription Tree (shared_subs):                                       │
│    "sensors" ──► "home" ──► "+" ──► "temperature"                       │
│                                         │                               │
│                                         └──► @subs: {client_A, client_B}│
└─────────────────────────────────────────────────────────────────────────┘
         ▲                                           │
         │ SUBSCRIBE                                 │ DELIVER (to ONE)
         │ $share/sensors/home/+/temperature         │
         │                                           ▼
    ┌─────────┐                               ┌─────────────┐
    │Client A │                               │Client A OR B│
    │Client B │                               └─────────────┘
    └─────────┘

         ▲
         │ PUBLISH
         │ home/livingroom/temperature
         │
    ┌─────────┐
    │Publisher│  (knows nothing about $share)
    └─────────┘

    Test Commands

        # Terminal 1: Subscribe (WITH $share)
        mosquitto_sub -h localhost -t "\$share/sensors/home/+/temperature" -v

        # Terminal 2: Subscribe (WITH $share)
        mosquitto_sub -h localhost -t "\$share/sensors/home/+/temperature" -v

        # Terminal 3: Publish (WITHOUT $share - just the actual topic)
        mosquitto_pub -h localhost -t "home/livingroom/temperature" -m "22.5"

    Summary

    Packet Type     Uses $share/group/?     Example
    SUBSCRIBE       Yes                     $share/sensors/home/+/temperature
    UNSUBSCRIBE     Yes                     $share/sensors/home/+/temperature
    PUBLISH         No                      home/livingroom/temperature

 ***************************************************************************/
PRIVATE int sub__add(
    hgobj gobj,
    const char *topic,
    const char *client_id,
    uint8_t qos,
    uint32_t subscription_id,
    uint8_t subscription_options
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*------------------*
     *  Tokenize topic
     *------------------*/
    char *local_topic = NULL;
    char **levels = NULL;
    const char *sharename = NULL;
    if(topic_tokenize(topic, &local_topic, &levels, &sharename) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Failed to tokenize topic",
            "client_id",    "%s", client_id,
            "topic",        "%s", topic,
            NULL
        );
        return -1;
    }

    /*------------------------------------------*
     *  Select root based on subscription type
     *------------------------------------------*/
    json_t *root;
    if(sharename) {
        root = priv->shared_subs;
    } else {
        root = priv->normal_subs;
    }

    /*-----------------------------*
     *  Navigate/create tree path
     *-----------------------------*/
    json_t *node = get_or_create_node(root, levels);
    if(!node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Failed to create tree node",
            "client_id",    "%s", client_id,
            "topic",        "%s", topic,
            NULL
        );
        GBMEM_FREE(local_topic)
        GBMEM_FREE(levels)
        return -1;
    }

    /*----------------------------*
     *  Get or create @subs dict
     *----------------------------*/
    json_t *subs = json_object_get(node, SUBS_KEY);
    if(!subs) {
        subs = json_object();
        if(!subs) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "Failed to create subs dict",
                NULL
            );
            GBMEM_FREE(local_topic)
            GBMEM_FREE(levels)
            return -1;
        }
        if(json_object_set_new(node, SUBS_KEY, subs) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "Failed to create subs",
                NULL
            );
            JSON_DECREF(subs)
            GBMEM_FREE(local_topic)
            GBMEM_FREE(levels)
            return -1;
        }
    }

    /*-------------------------------------------------------*
     *  Add or update subscriber (O(1) operation with dict)
     *-------------------------------------------------------*/
    json_t *sub_info = json_pack("{s:i, s:I, s:i}",
        "qos", (int)qos,
        "id", (json_int_t)subscription_id,
        "options", (int)subscription_options
    );
    if(!sub_info) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Failed to create subscription info",
            NULL
        );
        GBMEM_FREE(local_topic)
        GBMEM_FREE(levels)
        return -1;
    }

    /*
     *  json_object_set_new replaces if key exists (update case)
     *  or adds new entry (new subscription case)
     */
    int ret = 0;

    if(json_object_get(subs, client_id)) {
        ret = 1; // already exists
    }
    if(json_object_set_new(subs, client_id, sub_info) < 0) {
        json_decref(sub_info);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Failed to create sub_info",
            NULL
        );
        GBMEM_FREE(local_topic)
        GBMEM_FREE(levels)
        return -1;
    }

    GBMEM_FREE(local_topic);
    GBMEM_FREE(levels);
    return ret;
}

/***************************************************************************
 *  get_node - Navigate tree path (read-only)
 *
 *  Traverses the JSON tree following the levels array.
 *  Does NOT create nodes if they don't exist.
 *
 *  Parameters:
 *      root   - Root JSON object of the tree
 *      levels - NULL-terminated array of topic levels
 *
 *  Returns:
 *      JSON object at the leaf node, or NULL if path doesn't exist
 ***************************************************************************/
PRIVATE json_t *get_node(json_t *root, char **levels)
{
    json_t *current = root;

    for(int i = 1; levels[i] != NULL; i++) {
        json_t *child = json_object_get(current, levels[i]);
        if(!child) {
            return NULL;
        }
        current = child;
    }

    return current;
}

/***************************************************************************
 *  collect_all_subscribers_recursive - Collect from node and all descendants
 *
 *  Used when '#' wildcard matches - collects all subscribers
 *  from current node and ALL children recursively.
 *
 *  Parameters:
 *      node   - Current JSON node
 *      result - JSON array to append client_ids to
 ***************************************************************************/
PRIVATE void collect_all_subscribers_recursive(hgobj gobj, json_t *node, json_t *result)
{
    const char *key;
    json_t *child;

    /*
     *  Collect from current node
     */
    if(kw_has_key(node, SUBS_KEY)) {
        collect_subscribers(gobj, node, result);
    }

    /*
     *  Recurse into children (skip @subs key)
     */
    json_object_foreach(node, key, child) {
        if(strcmp(key, SUBS_KEY) != 0) {
            collect_all_subscribers_recursive(gobj, child, result);
        }
    }
}

/***************************************************************************
 *  prune_empty_branches - Remove empty nodes from tree
 *
 *  Walks back up the tree removing nodes that have no children
 *  and no subscribers.
 *
 *  Parameters:
 *      root   - Root JSON object of the tree
 *      levels - NULL-terminated array of topic levels
 ***************************************************************************/
PRIVATE void prune_empty_branches(json_t *root, char **levels)
{
    int depth;

    /*
     *  Count depth
     */
    for(depth = 1; levels[depth] != NULL; depth++) {
    }

    /*
     *  Walk backwards from leaf to root
     */
    for(int i = depth - 1; i >= 1; i--) {
        /*
         *  Get parent node
         */
        json_t *parent = root;
        for(int j = 1; j < i; j++) {
            parent = json_object_get(parent, levels[j]);
            if(!parent) {
                return;
            }
        }

        /*
         *  Get current node
         */
        json_t *node = json_object_get(parent, levels[i]);
        if(!node) {
            return;
        }

        /*
         *  Check if node is empty:
         *  - No children except @subs
         *  - @subs is empty or missing
         */
        json_t *subs = json_object_get(node, SUBS_KEY);
        size_t child_count = json_object_size(node);

        if(subs) {
            child_count--;  /* Don't count @subs as a child */
        }

        if(child_count == 0 && (!subs || json_object_size(subs) == 0)) {
            json_object_del(parent, levels[i]);
        } else {
            /*
             *  Node has children or subscribers, stop pruning
             */
            break;
        }
    }
}

/***************************************************************************
 *  sub__remove - Remove a subscription from the tree
 *
 *  Parameters:
 *      gobj        - GObj instance (for PRIVATE_DATA access)
 *      topic       - Subscription topic to remove
 *      client_id   - Client identifier
 *
 *  Returns:
 *      0 on success,
 *      MQTT_RC_NO_SUBSCRIPTION_EXISTED if subscription didn't exist,
 *      -1 on error
 *
 *  Example:
 *      sub__remove(gobj, "home/+/temperature", "client_001");
 ***************************************************************************/
PRIVATE int sub__remove(
    hgobj gobj,
    const char *topic,
    const char *client_id,
    mqtt5_reason_codes_t *reason
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char *local_topic = NULL;
    char **levels = NULL;
    const char *sharename = NULL;
    json_t *root;

    if(!topic || !client_id) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "topic or client_id is NULL",
            NULL
        );
        return -1;
    }

    /*------------------*
     *  Tokenize topic
     *------------------*/
    if(topic_tokenize(topic, &local_topic, &levels, &sharename) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Failed to tokenize topic",
            "client_id",    "%s", client_id,
            "topic",        "%s", topic,
            NULL
        );
        return -1;
    }

    /*------------------------------------------*
     *  Select root based on subscription type
     *------------------------------------------*/
    if(sharename) {
        root = priv->shared_subs;
    } else {
        root = priv->normal_subs;
    }

    /*--------------------------------------------------*
     *  Navigate to node (don't create if not exists)
     *--------------------------------------------------*/
    json_t *node = get_node(root, levels);
    if(!node) {
        /*
         *  Topic path doesn't exist, nothing to remove
         */
        *reason = MQTT_RC_NO_SUBSCRIPTION_EXISTED;
        goto cleanup;
    }

    /*-------------------*
     *  Get @subs dict
     *-------------------*/
    json_t *subs = json_object_get(node, SUBS_KEY);
    if(!subs || !json_object_get(subs, client_id)) {
        /*
         *  No subscribers at this node
         */
        *reason = MQTT_RC_NO_SUBSCRIPTION_EXISTED;
        goto cleanup;
    }

    /*------------------------------------------------*
     *  Remove subscriber (O(1) operation with dict)
     *------------------------------------------------*/
    json_object_del(subs, client_id);

    /*-----------------------------------------*
     *  If @subs is now empty, remove the key
     *-----------------------------------------*/
    if(json_object_size(subs) == 0) {
        json_object_del(node, SUBS_KEY);
    }

    /*-------------------------*
     *  Prune empty branches
     *-------------------------*/
    prune_empty_branches(root, levels);

cleanup:
    if(local_topic) {
        gbmem_free(local_topic);
    }
    if(levels) {
        gbmem_free(levels);
    }

    return 0;
}

/***************************************************************************
 *  sub__remove_client - Remove ALL subscriptions for a client
 *
 *  Used when a client disconnects to clean up all its subscriptions.
 *
 *  Parameters:
 *      gobj        - GObj instance (for PRIVATE_DATA access)
 *      client_id   - Client identifier to remove
 *
 *  Returns:
 *      Number of subscriptions removed, -1 on error
 *
 *  Example:
 *      int removed = sub__remove_client(gobj, "client_001");
 ***************************************************************************/
PRIVATE int sub__remove_client_recursive(json_t *node, const char *client_id, int *count)
{
    const char *key;
    json_t *child;
    void *tmp;

    /*
     *  Check @subs in current node (O(1) lookup in dict)
     */
    json_t *subs = json_object_get(node, SUBS_KEY);
    if(subs) {
        if(json_object_get(subs, client_id)) {
            json_object_del(subs, client_id);
            (*count)++;

            if(json_object_size(subs) == 0) {
                json_object_del(node, SUBS_KEY);
            }
        }
    }

    /*
     *  Recurse into children (use safe iteration for potential deletion)
     */
    json_object_foreach_safe(node, tmp, key, child) {
        if(strcmp(key, SUBS_KEY) != 0) {
            sub__remove_client_recursive(child, client_id, count);

            /*
             *  Remove empty child nodes
             */
            subs = json_object_get(child, SUBS_KEY);
            size_t child_count = json_object_size(child);
            if(subs) {
                child_count--;
            }
            if(child_count == 0 && (!subs || json_object_size(subs) == 0)) {
                json_object_del(node, key);
            }
        }
    }

    return 0;
}
PRIVATE int sub__remove_client(hgobj gobj, const char *client_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int count = 0;

    /*
     *  Remove from normal_subs
     */
    sub__remove_client_recursive(priv->normal_subs, client_id, &count);

    /*
     *  Remove from shared_subs
     */
    sub__remove_client_recursive(priv->shared_subs, client_id, &count);

    return count;
}

/***************************************************************************
 *  Subscription: search if the topic has a retain message and process
 ***************************************************************************/
PRIVATE int retain__queue(
    hgobj gobj,
    const char *sub,
    uint8_t sub_qos,
    uint32_t subscription_identifier
)
{
    if(strncmp(sub, "$share/", strlen("$share/"))==0) {
        return 0;
    }
    // TODO
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void session_expiry__check(void) //TODO
{
    // struct session_expiry_list *item, *tmp;
    // struct mosquitto *context;
    //
    // if(db.now_real_s <= last_check) return;
    //
    // last_check = db.now_real_s;
    //
    // DL_FOREACH_SAFE(expiry_list, item, tmp){
    //     if(item->context->session_expiry_time < db.now_real_s){
    //
    //         context = item->context;
    //         session_expiry__remove(context);
    //
    //         if(context->id){
    //             log__printf(NULL, MOSQ_LOG_NOTICE, "Expiring client %s due to timeout.", context->id);
    //         }
    //         G_CLIENTS_EXPIRED_INC();
    //
    //         /* Session has now expired, so clear interval */
    //         context->session_expiry_interval = 0;
    //         /* Session has expired, so will delay should be cleared. */
    //         context->will_delay_interval = 0;
    //         will_delay__remove(context);
    //         context__send_will(context);
    //         context__add_to_disused(context);
    //     }else{
    //         return;
    //     }
    // }
}

/***************************************************************************
 *  Send a message to client because his subscription

    Example of sub:
        {
            "qos": 2,
            "options": 0,
            "ids": [
                123
            ]
        }

    Example of msg:
        {
            "topic": "home/DEV/temperature",
            "tm": 1768037018,
            "mid": 1,
            "qos": 1,
            "expiry_interval": 0,
            "retain": false,
            "properties": {},
            "gbuffer": 107924988703504,
            "dup": false,
            "state": 0,
            "__temp__": {
                "channel": "input-2",
                "channel_gobj": 107924988772496
            }
        }

 ***************************************************************************/
PRIVATE int subs__send(
    hgobj gobj,
    const char *client_id,
    json_t *sub,        // not owned
    json_t *kw_mqtt_msg // not owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------------------------------------*
     *  Dispatch the message:
     *  If a client is in a subscription, possibilities are:
     *      - not connected and with a persistent session
     *      - connected, with or without a persistent session
     *
     *  Solution:
     *      - if session does not exit, it's an internal error
     *      - if session exists and disconnected: open the queue and append the message
     *      - if session exists and connected: send the message through an event
     *--------------------------------------------------------*/

    /*----------------------------*
     *  Find the client session
     *----------------------------*/
    json_t *session = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "sessions",
        json_pack("{s:s}", "id", client_id),
        NULL,
        gobj
    );
    if(!session) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Session of subs's client not found",
            "client",       "%s", client_id,
            NULL
        );
        return -1;
    }

    /*----------------------------------------------*
     *  Get parameters of message and subscription
     *----------------------------------------------*/
    const char *topic = kw_get_str(gobj, kw_mqtt_msg, "topic", "", KW_REQUIRED);
    BOOL retain = kw_get_bool(gobj, kw_mqtt_msg, "retain", 0, KW_REQUIRED);
    int qos = (int)kw_get_int(gobj, kw_mqtt_msg, "qos", 0, KW_REQUIRED);
    json_int_t tm = kw_get_int(gobj, kw_mqtt_msg, "tm", 0, KW_REQUIRED);
    json_int_t expiry_interval = kw_get_int(gobj, kw_mqtt_msg, "expiry_interval", 0, KW_REQUIRED);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(
        gobj, kw_mqtt_msg, "gbuffer", 0, KW_REQUIRED
    );

    int options = (int)kw_get_int(gobj, sub, "options", 0, KW_REQUIRED);
    BOOL retain_as_published = options & 0x08;
    uint8_t client_qos = (int)kw_get_int(gobj, sub, "qos", 0, KW_REQUIRED);
    json_t *ids = kw_get_list(gobj, sub, "ids", 0, 0); // TODO don't save ids if no id, save mem/perf

    /*------------------------------*
     *  Check for ACL topic access
     *------------------------------*/
    // TODO
    // rc2 = mosquitto_acl_check(
    //     leaf->context,
    //     topic,
    //     stored->payloadlen,
    //     stored->payload,
    //     stored->qos,
    //     stored->retain,
    //     MOSQ_ACL_READ
    // );
    // if(rc2 == MOSQ_ERR_ACL_DENIED) {
    //     return MOSQ_ERR_SUCCESS;
    // }

    /*---------------------------------*
     *  Fix parameters of msg to send
     *---------------------------------*/
    BOOL client_retain;
    uint16_t mid;
    uint8_t msg_qos;

    if(qos > client_qos) {
        msg_qos = client_qos;
    } else {
        msg_qos = qos;
    }
    if(msg_qos) {
        mid = 1; // TODO mqtt__mid_generate(leaf->context);
    } else {
        mid = 0;
    }

    if(retain_as_published) {
        /*
         *  Subscription option RETAIN_AS_PUBLISHED
         *  It controls whether the broker preserves the original retain flag
         *  when forwarding to subscribers, or clears it
         */
        client_retain = retain;
    } else {
        client_retain = FALSE;
    }

    /*-----------------------*
     *  Create the message
     *-----------------------*/
    json_t *properties = NULL;
    if(json_array_size(ids)) {
        // TODO
        // mosquitto_property_add_varint(
        //     &properties,
        //     MQTT_PROP_SUBSCRIPTION_IDENTIFIER,
        //     leaf->identifier
        // );
    }

    json_t *new_msg = new_mqtt_message(
        gobj,
        mid,    // TODO out of here? it must be the __rowid__ ???
        topic,
        gbuffer_incref(gbuf),    // owned
        msg_qos,
        client_retain,
        FALSE, // dup,
        properties,         // owned
        expiry_interval,
        tm
    );

    /*------------------------------------------------------------*
     *  Get the channel, if NULL then the client is disconnected
     *------------------------------------------------------------*/
    hgobj _gobj_channel = (hgobj)(uintptr_t)kw_get_int(
        gobj, session, "_gobj_channel", 0, KW_REQUIRED
    );

    if(!_gobj_channel) {
        /*------------------------------------------------*
         *  client/session is disconnected
         *      open queue, append message, close queue
         *------------------------------------------------*/
        if(msg_qos == 0) {
            /*
             *  If qos is 0 and client is disconnected, the message is lost
             */
            KW_DECREF(new_msg)
            JSON_DECREF(session)
            return 0;
        }

        /*
         *  Output messages
         */
        char queue_name[NAME_MAX];
        build_queue_name(
            queue_name,
            sizeof(queue_name),
            client_id,
            mosq_md_out
        );

        tr2_queue_t *trq_out_msgs = tr2q_open(
            priv->tranger_queues,
            queue_name,
            "tm",
            0,  // system_flag
            0,  // max_inflight_messages TODO
            gobj_read_integer_attr(gobj, "backup_queue_size")
        );

        user_flag_t user_flag = {0};
        user_flag_set_origin(&user_flag, mosq_mo_client);
        user_flag_set_direction(&user_flag, mosq_md_out);
        user_flag_set_qos_level(&user_flag, msg_qos);
        user_flag_set_retain(&user_flag, client_retain);
        user_flag_set_dup(&user_flag, 0);
        user_flag_set_state(&user_flag, mosq_ms_invalid);

        tr2q_append(
            trq_out_msgs,
            tm,             // __t__ if 0 then the time will be set by TimeRanger with now time
            new_msg,        // owned
            user_flag.value // extra flags in addition to TRQ_MSG_PENDING
        );

        tr2q_close(trq_out_msgs);
        JSON_DECREF(session)
        return 0;
    }

    /*---------------------------------*
     *  client/session is connected
     *      Send it the message
     *---------------------------------*/

    //message__release_to_inflight(gobj, mosq_md_out);

    kw_set_subdict_value(
        gobj,
        new_msg,
        "__temp__",
        "channel_gobj",
        json_integer((json_int_t)(uintptr_t)_gobj_channel)
    );

    gobj_send_event(priv->gobj_input_side, EV_SEND_MESSAGE, new_msg, gobj);

    JSON_DECREF(session)
    return 0;
}

/***************************************************************************
 *  Enqueue message to subscribers
 *
 *  Searches for all subscriptions that match the given published topic.
 *  The published topic must NOT contain wildcards.
 *  Subscription wildcards ('+' and '#') are matched appropriately.
 *
 *  Return # of subscribers
 ***************************************************************************/
PRIVATE size_t sub__messages_queue(
    hgobj gobj,
    json_t *kw_mqtt_msg // not owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *topic = kw_get_str(gobj, kw_mqtt_msg, "topic", "", KW_REQUIRED);
    BOOL retain = kw_get_bool(gobj, kw_mqtt_msg, "retain", 0, KW_REQUIRED);

    if(empty_string(topic)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "topic is NULL",
            NULL
        );
        return -1;
    }

    /*------------------------------------------------*
     *  Published topics must not contain wildcards
     *------------------------------------------------*/
    if(strchr(topic, '+') || strchr(topic, '#')) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Published topic cannot contain wildcards",
            "topic",        "%s", topic,
            NULL
        );
        return -1;
    }

    /*-------------------*
     *  Tokenize topic
     *-------------------*/
    char *local_topic = NULL;
    char **levels = NULL;
    const char *sharename = NULL;
    if(topic_tokenize(topic, &local_topic, &levels, &sharename) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Failed to tokenize topic",
            "topic",        "%s", topic,
            NULL
        );
        return -1;
    }

    /*---------------------------*
     *  Count the messages sent
     *---------------------------*/
    size_t total_sent = 0;

    /*----------------------------------------------------------------------*
     *  Search in normal_subs (non-shared subscriptions)
     *----------------------------------------------------------------------*/
    json_t *normal_subscribers = json_object();
    search_recursive(gobj, priv->normal_subs, levels, 1, normal_subscribers);

    const char *client_id; json_t *sub;
    json_object_foreach(normal_subscribers, client_id, sub) {
        if(subs__send(gobj, client_id, sub, kw_mqtt_msg)==0) {
            total_sent++;
        }
    }

    /*----------------------------------------------------------------------*
     *  Search in shared_subs (shared subscriptions)
     *  Note: For shared subs, only one client per group receives the message
     *----------------------------------------------------------------------*/
    json_t *shared_subscribers = json_object();
    search_recursive(gobj, priv->shared_subs, levels, 1, shared_subscribers);

    json_object_foreach(shared_subscribers, client_id, sub) {
        if(subs__send(gobj, client_id, sub, kw_mqtt_msg)==0) {
            total_sent++;
            break; // shared subs, only one sent (always the first, sorry)
        }
    }

    json_decref(normal_subscribers);
    json_decref(shared_subscribers);

    if(retain) { // TODO implement retain
        //     if(retain__store(topic, *stored, split_topics)<0) {
        //         ret = -1;
        //     }
    }

cleanup:
    if(local_topic) {
        gbmem_free(local_topic);
    }
    if(levels) {
        gbmem_free(levels);
    }

    return total_sent;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Identity_card on from
 *      mqtt clients (__input_side__)
 *      top clients (__top_side__)
 *

    {
        "id": "DVES_40AC66",
        "assigned_id": false, #^^ if assigned_id is true the client_id is temporary.
        "clean_start": true,
        "username": "DVES_USER",
        "services_roles": {
            "treedb_mqtt_broker": [
                "device"
            ]
        },
        "session_id": "2107a4a7-1ebb-4aba-85e8-a8ce9310528e",
        "peername": "127.0.0.1:44990",
        "protocol_version": 4,
        "session_expiry_interval": 0,
        "keep_alive": 30,
        "will": true,
        "will_retain": true,
        "will_qos": 1,
        "will_topic": "tele/tasmota_40AC66/LWT",
        "will_delay_interval": 0,
        "will_expiry_interval": 0,
        "gbuffer": 104300976196336,  #^^ it contents the will payload
        "__temp__": {
            "channel": "input-1",
            "channel_gobj": 104300975960288
        }
    }

    {
        "id": "client1",
        "assigned_id": false,
        "clean_start": false,
        "username": "yuneta",
        "services_roles": {
            "treedb_mqtt_broker": []
        },
        "session_id": "",
        "peername": "127.0.0.1:56770",
        "protocol_version": 5,
        "session_expiry_interval": -1,
        "keep_alive": 60,
        "will": false,
        "connect_properties": {
            "session-expiry-interval": {
                "identifier": 17,
                "name": "session-expiry-interval",
                "type": 3,
                "value": 4294967295
            },
            "receive-maximum": {
                "identifier": 33,
                "name": "receive-maximum",
                "type": 2,
                "value": 20
            }
        },
        "__temp__": {
            "channel": "input-1",
            "channel_gobj": 107416771591136
        }
    }


 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj gobj_channel = (hgobj)(uintptr_t)kw_get_int(
        gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED
    );

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_OPEN %s", gobj_short_name(src)
        );
    }

    if(src == priv->gobj_top_side) {
        KW_DECREF(kw);
        return 0;
    }

    if(src != priv->gobj_input_side) {
         gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "on_open NOT from input_size",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    /*--------------------------------------*
     *  From here it's a mqtt2 connection
     *--------------------------------------*/

    /*--------------------------------------------------------------*
     *              Open/Create CLIENT
     *
     *  This must be done *after* any security checks.
     *  With assigned_id the id is random!, not a persistent id
     *  MQTT Client ID <==> topic in Timeranger
     *  (HACK client_id is really a device_id in mqtt IoT devices)
     *--------------------------------------------------------------*/
    const char *client_id = kw_get_str(gobj, kw, "id", "", KW_REQUIRED);
    BOOL assigned_id = kw_get_bool(gobj, kw, "assigned_id", 0, KW_REQUIRED);
    if(strncmp(client_id, "auto-", strlen("auto-"))==0) {
        /*
         *  Particular case: Mosquito client without client_id
         */
        assigned_id = TRUE;
        json_object_set_new(kw, "assigned_id", json_true());
    }

    json_t *client = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "clients",
        json_pack("{s:s}", "id", client_id),
        NULL,
        gobj
    );
    if(!client) {
        /*
         *  Client NOT exist, refuse or create it if enable_new_clients is true
         */
        if(!priv->enable_new_clients) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Client not exist and no auto-create",
                "client_id",    "%s", client_id,
                NULL
            );
            KW_DECREF(kw);
            return -1;
        }

        /*
         *  Create new client as auto-create
         */
            // gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
            // if(gbuf) {
            //     gbuffer_t *gbuf_base64 = gbuffer_encode_base64(gbuffer_incref(gbuf));
            //     char *b64 = gbuffer_cur_rd_pointer(gbuf_base64);
            //     json_object_set_new(jn_will, "will_payload", json_string(b64));
            //     gbuffer_decref(gbuf_base64);
            // }

        json_object_set_new(kw, "auto_created", json_true());

        client = gobj_create_node(
            priv->gobj_treedb_mqtt_broker,
            "clients",
            kw_incref(kw),
            NULL,
            gobj
        );
        if(!client) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Cannot create new client",
                "client_id",    "%s", client_id,
                NULL
            );
            KW_DECREF(kw);
            return -1;
        }
    }

    /*----------------------------------------------------------------*
     *              Open/Create SESSION
     *----------------------------------------------------------------*/
    int result = 0;
    int prev_mid = 0;
    BOOL clean_start = kw_get_bool(gobj, kw, "clean_start", TRUE, KW_REQUIRED);
    int protocol_version = (int)kw_get_int(gobj, kw, "protocol_version", 0, KW_REQUIRED);
    if(assigned_id) {
        clean_start = TRUE;
    }

    json_t *session = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "sessions",
        json_pack("{s:s}", "id", client_id),
        NULL,
        gobj
    );

    // TODO process WILL

    if(session) {
print_json2("=====>1 SESSION", session); // TODO TEST
        /*-------------------------------------------------------------*
         *              Exists a previous session
         *-------------------------------------------------------------*/
        int prev_protocol_version = (int)kw_get_int(
            gobj,
            session,
            "protocol_version",
            0,
            KW_REQUIRED
        );
        uint32_t prev_session_expiry_interval = (uint32_t)kw_get_int(
            gobj,
            session,
            "session_expiry_interval",
            0,
            KW_REQUIRED
        );
        BOOL prev_clean_start = (int)kw_get_bool(
            gobj,
            session,
            "clean_start",
            TRUE,
            KW_REQUIRED
        );
        prev_mid = (int)kw_get_int(
            gobj,
            session,
            "mid",
            0,
            KW_REQUIRED
        );
        hgobj prev_gobj_channel = (hgobj)(uintptr_t)kw_get_int(
            gobj,
            session,
            "_gobj_channel",
            0,
            KW_REQUIRED
        );

        if((prev_protocol_version == mosq_p_mqtt5 && prev_session_expiry_interval == 0) ||
            (prev_protocol_version != mosq_p_mqtt5 && prev_clean_start == TRUE) ||
            (clean_start == TRUE)
        ) {
            // TODO context__send_will(found_context);
        }

        // TODO
        // session_expiry__remove(found_context);
        // will_delay__remove(found_context);
        // will__clear(found_context);

        BOOL delete_prev_session = TRUE;

        if(!clean_start) {
            /*-----------------------------------*
             *      Reuse the session
             *-----------------------------------*/
            if(prev_session_expiry_interval > 0) {
                // TODO check if prev_session_expiry_interval expired
                if(protocol_version == mosq_p_mqtt311 || protocol_version == mosq_p_mqtt5) {
                    result = 1; // ack=1 Resume existing session
                    delete_prev_session = FALSE;
                }
            }
        }
        if(delete_prev_session) {
            /*-----------------------------------*
             *  Delete it if clean_start TRUE
             *-----------------------------------*/
            gobj_delete_node(
                priv->gobj_treedb_mqtt_broker,
                "sessions",
                json_incref(session),  // owned
                json_pack("{s:b}", "force", 1),
                gobj
            );
            // TODO clean queues
        }

        JSON_DECREF(session);

        /*
         *  Disconnect previous session
         */
        if(prev_gobj_channel) {
            json_t *kw_disconnect = json_object();
            int reason_code = 0;
            if(delete_prev_session) {
                reason_code = (prev_protocol_version == mosq_p_mqtt5)?MQTT_RC_SESSION_TAKEN_OVER:0;
            }
            json_object_set_new(
                kw_disconnect,
                "reason_code",
                json_integer(reason_code)
            );
            gobj_send_event(prev_gobj_channel, EV_DROP, kw_disconnect, gobj);
        }
    }

    // TODO
    // rc = acl__find_acls(context);
    /*-----------------------------*
     *  Check acl acl__find_acls
     *-----------------------------*/
    // TODO
    //connection_check_acl(context, &context->msgs_in.inflight);
    //connection_check_acl(context, &context->msgs_in.queued);
    //connection_check_acl(context, &context->msgs_out.inflight);
    //connection_check_acl(context, &context->msgs_out.queued);

    if(prev_mid) {
        json_object_set_new(kw, "mid", json_integer(prev_mid));
    }
    json_object_set_new(kw, "_gobj_channel", json_integer((json_int_t)gobj_channel));
    json_object_set_new(kw, "in_session", json_true());

    session = gobj_update_node(
        priv->gobj_treedb_mqtt_broker,
        "sessions",
        kw_incref(kw),
        json_pack("{s:b}", "create", 1),
        gobj
    );

    if(!session) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Cannot create new session",
            "client_id",    "%s", client_id,
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    JSON_DECREF(session)
    JSON_DECREF(client)
    KW_DECREF(kw);
    return result;
}

/***************************************************************************
 *  Identity_card off from
 *      mqtt clients (__input_side__)
 *      top clients (__top_side__)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj gobj_channel = (hgobj)(uintptr_t)kw_get_int(
        gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED
    );

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_CLOSE %s", gobj_short_name(src)
        );
    }

    if(src == priv->gobj_top_side) {
        KW_DECREF(kw);
        return 0;
    }

    if(src != priv->gobj_input_side) {
        gobj_log_error(gobj, 0,
           "function",     "%s", __FUNCTION__,
           "msgset",       "%s", MSGSET_INTERNAL_ERROR,
           "msg",          "%s", "on_close NOT from input_size",
           "src",          "%s", gobj_full_name(src),
           NULL
       );
        KW_DECREF(kw);
        return -1;
    }

    /*------------------------------*
     *  Recover client and session
     *------------------------------*/
    const char *client_id = gobj_read_str_attr(gobj_channel, "client_id");
    json_t *client = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "clients",
        json_pack("{s:s}", "id", client_id),
        NULL,
        gobj
    );

    BOOL assigned_id = kw_get_bool(gobj, client, "assigned_id", FALSE, KW_REQUIRED);

    json_t *session = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "sessions",
        json_pack("{s:s}", "id", client_id),
        NULL,
        gobj
    );
    BOOL clean_start = (int)kw_get_bool(
        gobj,
        session,
        "clean_start",
        TRUE,
        KW_REQUIRED
    );

    /*-----------------------------------------*
     *  If not persistent session delete it
     *  and if it's dynamic client delete too
     *-----------------------------------------*/
    if(clean_start) {
        /*
         *  Delete not persistent session
         */
        gobj_delete_node(
            priv->gobj_treedb_mqtt_broker,
            "sessions",
            json_pack("{s:s}", "id", client_id),  // owned
            json_pack("{s:b}", "force", 1), // owned
            gobj
        );

        /*
         *  Dynamic client
         */
        if(assigned_id) {
            /*
             *  Delete volatil client
             */
            gobj_delete_node(
                priv->gobj_treedb_mqtt_broker,
                "clients",
                json_pack("{s:s}", "id", client_id),  // owned
                json_pack("{s:b}", "force", 1), // owned
                gobj
            );
        }

        sub__remove_client(gobj, client_id);

    } else {
        /*
         *  Persistent session
         */
        hgobj prev_gobj_channel = (hgobj)(uintptr_t)kw_get_int(
            gobj,
            session,
            "_gobj_channel",
            0,
            KW_REQUIRED
        );
        if(gobj_channel == prev_gobj_channel) {
            /*
             *  Session can be connected by other channel
             */
            json_object_set_new(session, "_gobj_channel", json_integer((json_int_t)0));
            json_object_set_new(session, "in_session", json_false());
            json_decref(gobj_update_node(
                priv->gobj_treedb_mqtt_broker,
                "sessions",
                json_incref(session),  // owned
                json_pack("{s:b}", "volatil", 1),
                gobj
            ));
        }
    }

    // TODO do will job ?

    JSON_DECREF(session)
    JSON_DECREF(client)
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Message from
 *      mqtt clients (__input_side__)
 ***************************************************************************/
PRIVATE int ac_mqtt_subscribe(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_MESSAGE %s", gobj_short_name(src)
        );
    }

    if(src != priv->gobj_input_side) {
        gobj_log_error(gobj, 0,
           "function",     "%s", __FUNCTION__,
           "msgset",       "%s", MSGSET_INTERNAL_ERROR,
           "msg",          "%s", "on_close NOT from input_size",
           "src",          "%s", gobj_full_name(src),
           NULL
       );
        KW_DECREF(kw);
        return -1;
    }

    const char *client_id = kw_get_str(gobj, kw, "client_id", "", KW_REQUIRED);
    mosquitto_protocol_t protocol_version = kw_get_int(
        gobj, kw, "protocol_version", 0, KW_REQUIRED
    );
    json_t *jn_list = kw_get_list(gobj, kw, "subs", NULL, KW_REQUIRED);
    gbuffer_t *gbuf_payload = (gbuffer_t *)(uintptr_t)kw_get_int(
        gobj, kw, "gbuffer", 0, KW_REQUIRED
    );

    int idx; json_t *jn_sub;
    json_array_foreach(jn_list, idx, jn_sub) {
        const char *sub = kw_get_str(gobj, jn_sub, "sub", NULL, KW_REQUIRED);
        int subscription_identifier = (int)kw_get_int(
            gobj, jn_sub, "subscription_identifier", 0, KW_REQUIRED
        );
        int qos = (int)kw_get_int(gobj, jn_sub, "qos", 0, KW_REQUIRED);
        int subscription_options = (int)kw_get_int(
            gobj, jn_sub, "subscription_options", 0, KW_REQUIRED
        );
        int retain_handling = (int)kw_get_int(gobj, jn_sub, "retain_handling", 0, KW_REQUIRED);

        mqtt5_reason_codes_t reason = qos;
        BOOL allowed = TRUE;
        // allowed = mosquitto_acl_check(context, sub, 0, NULL, qos, FALSE, MOSQ_ACL_SUBSCRIBE); TODO
        if(!allowed) {
            // TODO
            if(protocol_version == mosq_p_mqtt5) {
                reason = MQTT_RC_NOT_AUTHORIZED;
            } else if(protocol_version == mosq_p_mqtt311) {
                reason = 0x80;
            }
        } else {
            int rc = sub__add( // WARNING return 1 if subs already exists
                gobj,
                sub,
                client_id,
                qos,
                subscription_identifier,
                subscription_options
            );
            if(rc<0) {
                KW_DECREF(kw);
                return -1;
            }

            if(protocol_version == mosq_p_mqtt311 || protocol_version == mosq_p_mqtt31) {
                retain__queue(gobj, sub, qos, 0);
            } else {
                if((retain_handling == MQTT_SUB_OPT_SEND_RETAIN_ALWAYS) ||
                    (rc == 0 && retain_handling == MQTT_SUB_OPT_SEND_RETAIN_NEW)
                ) {
                    retain__queue(gobj, sub, qos, subscription_identifier);
                }
            }
        }

        /*
         *  Add the reason code to response
         */
        gbuffer_append_char(gbuf_payload, reason);
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, priv->normal_subs, "subs-normal");
        gobj_trace_json(gobj, priv->shared_subs, "subs-shared");
    }

    // TODO
    // if(priv->current_out_packet == NULL) {
    //     rc = db__message_write_queued_out(gobj);
    //     if(rc) {
    //         return rc;
    //     }
    //     rc = db__message_write_inflight_out_latest(gobj);
    //     if(rc) {
    //         return rc;
    //     }
    // }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Message from
 *      mqtt clients (__input_side__)
 ***************************************************************************/
PRIVATE int ac_mqtt_unsubscribe(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_MESSAGE %s", gobj_short_name(src)
        );
    }

    if(src != priv->gobj_input_side) {
        gobj_log_error(gobj, 0,
           "function",     "%s", __FUNCTION__,
           "msgset",       "%s", MSGSET_INTERNAL_ERROR,
           "msg",          "%s", "on_close NOT from input_size",
           "src",          "%s", gobj_full_name(src),
           NULL
       );
        KW_DECREF(kw);
        return -1;
    }

    const char *client_id = kw_get_str(gobj, kw, "client_id", "", KW_REQUIRED);
    json_t *jn_list = kw_get_list(gobj, kw, "subs", NULL, KW_REQUIRED);
    gbuffer_t *gbuf_payload = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);

    int idx; json_t *jn_sub;
    json_array_foreach(jn_list, idx, jn_sub) {
        const char *sub = kw_get_str(gobj, jn_sub, "sub", NULL, KW_REQUIRED);

        // /* ACL check */
        mqtt5_reason_codes_t reason = 0;
        BOOL allowed = TRUE;
        // allowed = mosquitto_acl_check(context, sub, 0, NULL, 0, FALSE, MOSQ_ACL_UNSUBSCRIBE); TODO
        if(!allowed) {
            reason = MQTT_RC_NOT_AUTHORIZED;
        } else {
            if(sub__remove(gobj, sub, client_id, &reason)<0) {
                // Error already logged
                KW_DECREF(kw);
                return -1;
            }
        }

        /*
         *  Add the reason code to response, but it will be used only in mqtt5 version
         */
        gbuffer_append_char(gbuf_payload, reason);
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, priv->normal_subs, "subs-normal");
        gobj_trace_json(gobj, priv->shared_subs, "subs-shared");
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Message from
 *      top clients (__top_side__)
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_MESSAGE %s", gobj_short_name(src)
        );
    }

    if(src != priv->gobj_top_side) {
        gobj_log_error(gobj, 0,
           "function",     "%s", __FUNCTION__,
           "msgset",       "%s", MSGSET_INTERNAL_ERROR,
           "msg",          "%s", "on_close NOT from input_size",
           "src",          "%s", gobj_full_name(src),
           NULL
       );
        KW_DECREF(kw);
        return -1;
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Broker, message from
 *      mqtt clients (__input_side__)
 *  Must returns the number of subscribers found
 ***************************************************************************/
PRIVATE int ac_mqtt_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "MQTT_MESSAGE %s", gobj_short_name(src)
        );
    }

    if(src != priv->gobj_input_side) {
        gobj_log_error(gobj, 0,
           "function",     "%s", __FUNCTION__,
           "msgset",       "%s", MSGSET_INTERNAL_ERROR,
           "msg",          "%s", "on_close NOT from input_size",
           "src",          "%s", gobj_full_name(src),
           NULL
       );
        KW_DECREF(kw);
        return -1;
    }

    /*-----------------------------------*
     *      Mqtt message
     *-----------------------------------*/
    int subscribers_found = (int)sub__messages_queue(
        gobj,
        kw // not owned
    );

    /*-----------------------------------*
     *      Publish to yuneta gobjs
     *-----------------------------------*/
    gobj_publish_event(gobj, EV_MQTT_MESSAGE, kw_incref(kw));

    KW_DECREF(kw);
    return subscribers_found;
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

    // TODO search sessions of a client deleted, disconnect and delete them
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
    // PRIVATE_DATA *priv = gobj_priv_data(gobj);
    //
    // const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);

    /*--------------------------------*
     *  Get user
     *  Create it if not exist
     *--------------------------------*/
    // json_t *user = gobj_get_node(
    //     priv->gobj_treedb_controlcenter,
    //     "users",
    //     json_pack("{s:s}",
    //         "id", username
    //     ),
    //     0,
    //     gobj
    // );
    // if(!user) {
    //     time_t t;
    //     time(&t);
    //     BOOL enabled_new_users = gobj_read_bool_attr(gobj, "enabled_new_users");
    //     json_t *jn_user = json_pack("{s:s, s:b, s:I}",
    //         "id", username,
    //         "enabled", enabled_new_users,
    //         "time", (json_int_t)t
    //     );
    //
    //     user = gobj_create_node(
    //         priv->gobj_treedb_controlcenter,
    //         "users",
    //         jn_user,
    //         0,
    //         gobj
    //     );
    // }
    // json_decref(user);

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
    // PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);
    // const char *dst_service = kw_get_str(gobj, kw, "dst_service", "", KW_REQUIRED);
    //
    // if(strcmp(dst_service, gobj_name(gobj))==0) {
    //     time_t t;
    //     time(&t);
    //     BOOL enabled_new_users = gobj_read_bool_attr(gobj, "enabled_new_users");
    //     if(enabled_new_users) {
    //         json_t *jn_user = json_pack("{s:s, s:b, s:I}",
    //             "id", username,
    //             "enabled", enabled_new_users,
    //             "time", (json_int_t)t
    //         );
    //
    //         json_decref(gobj_create_node(
    //             priv->gobj_treedb_controlcenter,
    //             "users",
    //             jn_user,
    //             0,
    //             gobj
    //         ));
    //     }
    // }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    // TODO
    // retain__expire();
    // queue_plugin_msgs();
    // context__free_disused();
    // keepalive__check();

    // session_expiry__check();
    // will_delay__check();

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
GOBJ_DEFINE_GCLASS(C_MQTT_BROKER);

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
        {EV_MQTT_MESSAGE,           ac_mqtt_message,        0},
        {EV_MQTT_SUBSCRIBE,         ac_mqtt_subscribe,      0},
        {EV_MQTT_UNSUBSCRIBE,       ac_mqtt_unsubscribe,    0},
        {EV_ON_MESSAGE,             ac_on_message,          0},
        {EV_ON_OPEN,                ac_on_open,             0},
        {EV_ON_CLOSE,               ac_on_close,            0},
        {EV_TREEDB_NODE_CREATED,    ac_treedb_node_create,  0},
        {EV_TREEDB_NODE_UPDATED,    ac_treedb_node_updated, 0},
        {EV_TREEDB_NODE_DELETED,    ac_treedb_node_deleted, 0},

        {EV_AUTHZ_USER_LOGIN,       ac_user_login,          0},
        {EV_AUTHZ_USER_LOGOUT,      ac_user_logout,         0},
        {EV_AUTHZ_USER_NEW,         ac_user_new,            0},
        {EV_TIMEOUT_PERIODIC,       ac_timeout,             0},
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
        {EV_ON_MESSAGE,             0},
        {EV_MQTT_MESSAGE,           EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_MQTT_SUBSCRIBE,         0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_MQTT_UNSUBSCRIBE,       0},
        {EV_TREEDB_NODE_UPDATED,    0},
        {EV_TREEDB_NODE_CREATED,    0},
        {EV_TREEDB_NODE_DELETED,    0},
        {EV_AUTHZ_USER_LOGIN,       0},
        {EV_AUTHZ_USER_LOGOUT,      0},
        {EV_AUTHZ_USER_NEW,         0},
        {EV_TIMEOUT_PERIODIC,       0},
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
PUBLIC int register_c_mqtt_broker(void)
{
    return create_gclass(C_MQTT_BROKER);
}
