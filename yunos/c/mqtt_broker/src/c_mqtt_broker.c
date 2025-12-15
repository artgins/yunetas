/***********************************************************************
 *          C_MQTT_BROKER.C
 *          Mqtt_broker GClass.
 *
 *          Mqtt broker
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>

#include "c_mqtt_broker.h"
#include "treedb_schema_mqtt_broker.c"

#include "c_prot_mqtt2.h" // TODO remove when moved to kernel

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int broadcast_queues_tranger(hgobj gobj);
PRIVATE int open_devices_qmsgs(hgobj gobj);
PRIVATE int close_devices_qmsgs(hgobj gobj);
PRIVATE int process_msg(
    hgobj gobj,
    json_t *kw,  // NOT owned
    json_t *device_resource // NOT owned
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

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

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---items-------json_fn-------------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help, pm_help,    cmd_help,           "Command's help"),

/*-CMD2---type----------name----------------flag----alias---items---------------json_fn-------------description--*/
SDATACM2 (DTP_SCHEMA,   "authzs",           0,      0,      pm_authzs,          cmd_authzs,         "Authorization's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag--------default-----description---------- */
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

    hgobj gobj_authz;
    hgobj gobj_input_side;
    hgobj gobj_top_side;

    hgobj gobj_tranger_queues;
    json_t *tranger_queues;

    hgobj gobj_treedbs;
    hgobj gobj_treedb_mqtt_broker;      // service of treedb_mqtt_broker (create in gobj_treedbs)
    json_t *tranger_treedb_mqtt_broker;

    char treedb_mqtt_broker_name[80];
    char msg2db_alarms_name[80];

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

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,                   gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,                 gobj_read_integer_attr)
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

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_unsubscribe_event(priv->gobj_authz, 0, 0, gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
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
     *      Persistent DB
     *---------------------------------------*/
    BOOL mqtt_persistent_db = gobj_read_bool_attr(gobj, "mqtt_persistent_db");
    if(mqtt_persistent_db) {
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
    }

    /*-------------------------*
     *      Start services
     *-------------------------*/
    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    priv->gobj_top_side = gobj_find_service("__top_side__", TRUE);
    gobj_subscribe_event(priv->gobj_top_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_top_side);

    /*--------------------------------*
     *  Open device qmsgs
     *--------------------------------*/
    //open_devices_qmsgs(gobj);

    /*-----------------------------*
     *  Broadcast timeranger
     *-----------------------------*/
    // TODO first starting childs?
    broadcast_queues_tranger(gobj);

    /*
     *  Periodic timer for tasks
     */
    set_timeout_periodic(priv->timer, priv->timeout); // La verdadera

    // TODO
    // priv->trq_msgs = trq_open(
    //     priv->tranger_queues,
    //     topic_name,
    //     gobj_read_str_attr(gobj, "tkey"),
    //     tranger2_str2system_flag(gobj_read_str_attr(gobj, "system_flag")),
    //     gobj_read_integer_attr(gobj, "backup_queue_size")
    // );

    // TODO trq_load(priv->trq_msgs);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------*
     *  Close device qmsgs
     *---------------------------------*/
    //close_devices_qmsgs(gobj);

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
     *      Stop top/input side
     *-----------------------------*/
    gobj_unsubscribe_event(priv->gobj_top_side, 0, 0, gobj);
    EXEC_AND_RESET(gobj_stop_tree, priv->gobj_top_side)

    gobj_unsubscribe_event(priv->gobj_input_side, 0, 0, gobj);
    EXEC_AND_RESET(gobj_stop_tree, priv->gobj_input_side)

    /*-----------------------------*
     *  Broadcast timeranger
     *-----------------------------*/
    broadcast_queues_tranger(gobj);

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




                    /***************************
                     *      Local Methods
                     ***************************/




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
PRIVATE int broadcast_queues_tranger(hgobj gobj)
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




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Identity_card on from
 *      mqtt clients (__input_side__)
 *      top clients (__top_side__)
 *

    {
        "username": "DVES_USER",
        "services_roles": {
            "treedb_mqtt_broker": [
                "device"
            ]
        },
        "session_id": "d1515457-f1a1-45d3-a7fd-3f335ae50037",
        "peername": "127.0.0.1:47298",
        "client_id": "DVES_40AC66",
        "assigned_id": false,       #^^ if assigned_id is true the client_id is temporary.
        "clean_start": true,
        "session_expiry_interval": 0,
        "max_qos": 2,
        "protocol_version": 2,
        "will": true,
        "will_retain": true, #^^ these will fields are optionals
        "will_qos": 1,
        "will_topic": "tele/tasmota_40AC66/LWT",
        "will_delay_interval": 0,
        "will_expiry_interval": 0,
        "gbuffer": 95091873745312,  #^^ it contents the will payload
        "__temp__": {
            "channel": "input-1",
            "channel_gobj": 98214347824800
        }
    }

 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES || 1) { // TODO remove || 1
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

    /*--------------------------------------------------------------*
     *  Create a client, must be checked in upper level.
     *  This must be done *after* any security checks.
     *  With assigned_id the id is random!, not a persistent id
     *  MQTT Client ID <==> topic in Timeranger
     *  (HACK client_id is really a device_id in mqtt IoT devices)
     *--------------------------------------------------------------*/
    BOOL clean_start = kw_get_bool(gobj, kw, "clean_start", 0, KW_REQUIRED);
    BOOL assigned_id = kw_get_bool(gobj, kw, "assigned_id", 0, KW_REQUIRED);
    BOOL will = kw_get_bool(gobj, kw, "will", 0, KW_REQUIRED);
    if(will) {
        // TODO read the remain will fields
    }

    const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);
    const char *client_id = kw_get_str(gobj, kw, "client_id", "", KW_REQUIRED);

    /*----------------------------------------------------------------*
     *  Open the topic (client_id) or create it if it doesn't exist
     *----------------------------------------------------------------*/
    if(!assigned_id) {
        json_t *jn_response = gobj_command(
            priv->gobj_tranger_queues,
            "open-topic",
            json_pack("{s:s, s:s}",
                "topic_name", client_id,
                "__username__", username
            ),
            gobj
        );

        result = COMMAND_RESULT(gobj, jn_response);
        if(result < 0) {
            JSON_DECREF(jn_response)
            jn_response = gobj_command(
                priv->gobj_tranger_queues,
                "create-topic", // idempotent function
                json_pack("{s:s, s:s}",
                    "topic_name", client_id,
                    "__username__", username
                ),
                gobj
            );
            result = COMMAND_RESULT(gobj, jn_response);
        }
        if(result < 0) {
            const char *comment = COMMAND_COMMENT(gobj, jn_response);
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_AUTH,
                "msg",          "%s", comment?comment:"cannot create/open topic (client)",
                "username",     "%s", username,
                NULL
            );
            JSON_DECREF(jn_response)
            KW_DECREF(kw);
            return -1;
        }

        json_t *topic = COMMAND_DATA(gobj, jn_response);

        print_json2("XXX topic", topic); // TODO TEST

        JSON_DECREF(jn_response)
    }

    // TODO if session already exists with below conditions return 1!
    // if(priv->clean_start == FALSE && prev_session_expiry_interval > 0) {
    //     if(priv->protocol_version == mosq_p_mqtt311 || priv->protocol_version == mosq_p_mqtt5) {
    //         connect_ack |= 0x01;
    //          result = 1;
    //     }
    //     // copia client session TODO
    // }

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

    if(gobj_trace_level(gobj) & TRACE_MESSAGES || 1) { // TODO remove || 1
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

    // TODO do will job ?

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Messasge from
 *      mqtt clients (__input_side__)
 *      top clients (__top_side__)
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES || 1) { // TODO remove || 1
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_MESSAGE %s", gobj_short_name(src)
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
//    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // printf("Timeout\n");

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
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_TREEDB_NODE_CREATED,    0},
        {EV_TREEDB_NODE_UPDATED,    0},
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
