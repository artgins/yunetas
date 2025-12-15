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
PRIVATE int broadcast_queues_timeranger(hgobj gobj);
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
    json_t *realtime_qmsgs;
    json_t *track_list;

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

    /*-------------------------------------------*
     *          Create Treedb System
     *-------------------------------------------*/
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
    broadcast_queues_timeranger(gobj);

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
    broadcast_queues_timeranger(gobj);

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
 *
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,       // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,   // in a rt_mem will be the relative rowid, in rt_disk the absolute rowid
    md2_record_ex_t *md_record,
    json_t *jn_record   // must be owned, can be null if only_md
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(list, "gobj"));

//    char temp[128];
//    tranger2_print_md1_record(temp, sizeof(temp), key, md_record, TRUE);
//    printf("=================> load_record_callback():\n   %s\n", temp);

    // if(gobj_gclass_name(gobj) != C_DB_HISTORY) {
    //     gobj_log_error(gobj, 0,
    //         "function",     "%s", __FUNCTION__,
    //         "msgset",       "%s", MSGSET_INTERNAL_ERROR,
    //         "msg",          "%s", "What gclass from?",
    //         "src",          "%s", gobj_short_name(gobj),
    //         NULL
    //     );
    //     return 0;
    // }
    //
    // json_t *device_resource = load_device(gobj, key, TRUE);
    //
    // json_int_t last_saved_t = kw_get_int(gobj, device_resource, "__t__", 0, 0);
    // json_int_t cur_t = (json_int_t)md_record->__t__;
    // if(cur_t > last_saved_t) {
    //     if(!jn_record) {
    //         jn_record = tranger2_read_record_content( // return is yours
    //             tranger,
    //             topic,
    //             key,
    //             md_record
    //         );
    //     }
    //     //printf("====> PROCESS %d > last_save_t %d\n", (int)cur_t, (int)last_saved_t);
    //
    //     process_msg(gobj, jn_record, device_resource);
    //     json_object_set_new(device_resource, "__t__", json_integer(cur_t));
    //     save_device(gobj, key, device_resource);
    // } else {
    //     // Message already processed
    //     //printf("====> TRASH %d <= last_save_t %d\n", (int)cur_t, (int)last_saved_t);
    // }
    //
    // json_decref(device_resource);

    json_decref(jn_record);
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
PRIVATE int broadcast_queues_timeranger(hgobj gobj)
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
 *
 ***************************************************************************/
PRIVATE int open_devices_qmsgs(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *match_cond = json_pack("{s:i, s:b, s:I}",
        // TODO check
        "from_t", -3600*24, // recupera desde el último día, la app no puede estar parada mas tiempo
        "only_md", 1,
        "load_record_callback", (json_int_t)(size_t)load_record_callback
    );

    priv->realtime_qmsgs = tranger2_open_list( // TODO esto puede tardar mucho
        priv->tranger_queues,
        "raw_qmsgs",
        match_cond,     // owned
        json_pack("{s:I}",   // extra
            "gobj", (json_int_t)(size_t)gobj
        ),
        gobj_short_name(gobj), // rt_id
        TRUE,   // rt_by_disk
        ""      // creator
    );
    if(!priv->realtime_qmsgs) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "tranger2_open_list() failed",
            "topic_name",   "%s", "raw_qmsgs",
            NULL
        );
    }

    priv->track_list = trmsg_open_list(
        priv->tranger_queues,
        "raw_qmsgs",  // topic
        json_pack("{s:i, s:i, s:i}",  // match_cond
            "max_key_instances", 20,    /* sync with max_chart_qmsgs in ui_device_sonda.js */
            "from_rowid", -30,
            "to_rowid", 0
        ),
        NULL,       // extra
        NULL,       // rt_id
        FALSE,      // rt_by_disk
        NULL        // creator
    );
    if(!priv->track_list) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "trmsg_open_list() failed",
            "topic_name",   "%s", "raw_qmsgs",
            NULL
        );
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_devices_qmsgs(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->realtime_qmsgs) {
        tranger2_close_list(
            priv->tranger_queues,
            priv->realtime_qmsgs
        );
        priv->realtime_qmsgs = 0;
    }

    if(priv->track_list) {
        trmsg_close_list(priv->tranger_queues, priv->track_list);
        priv->track_list = 0;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int process_msg(
    hgobj gobj,
    json_t *kw,  // NOT owned
    json_t *device_resource // NOT owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *device_id = kw_get_str(gobj, kw, "id", "", KW_REQUIRED);
    if(empty_string(device_id)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Message without id",
            NULL
        );
        gobj_trace_json(gobj, kw, "Message without id");
        return -1;
    }

    /*--------------------------------*
     *  Get device of track
     *  Create it if not exist
     *--------------------------------*/
    const char *yuno = kw_get_str(gobj, kw, "yuno", "", 0);
    json_t *device = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "devices",
        json_incref(kw),
        json_pack("{s:b}", "fkey_only_id", TRUE),  // fkey,hook options
        gobj
    );

    if(!device) {
        /*
         *  Nuevo device, crea registro
         */
        time_t t;
        time(&t);
        BOOL enabled_new_devices = gobj_read_bool_attr(gobj, "enabled_new_devices");
        json_t *jn_device = json_pack("{s:s, s:s, s:b, s:{}, s:s, s:I}",
            "id", device_id,
            "name", device_id,
            "enabled", enabled_new_devices,
            "properties",
            "yuno", yuno,
            "tm", (json_int_t)t
        );

        device = gobj_create_node(
            priv->gobj_treedb_mqtt_broker,
            "devices",
            jn_device,
            json_pack("{s:b}", "fkey_only_id", TRUE),  // fkey,hook options
            gobj
        );
    }

    if(!device) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Device ???",
            NULL
        );
        return -1;
    }

    /*---------------------------------*
     *  Get schema of device_types
     *  To check template version
     *---------------------------------*/
    json_t *jn_desc = gobj_topic_desc(
        priv->gobj_treedb_mqtt_broker,
        "device_types"
    );
    json_t *template_settings = kwid_get(gobj,
        jn_desc,
        KW_VERBOSE,
        "cols`template_settings`template"
    );

    json_int_t schema_template_settings_version = kw_get_int(
        gobj, template_settings, "template_version`default", 0, KW_WILD_NUMBER
    );

    /*-----------------------------*
     *  Get type of device
     *  Create it if not exist
     *-----------------------------*/
    char type_id[NAME_MAX];
    snprintf(type_id, sizeof(type_id), "%s", yuno);
    change_char(type_id, '^', '-');

    json_t *device_type = gobj_get_node(
        priv->gobj_treedb_mqtt_broker,
        "device_types",
        json_pack("{s:s}", "id", type_id),
        0,
        gobj
    );

    if(!device_type) {
        /*
         *  Nuevo device, crea registro
         */
        /*
         *  HACK trigger point: default to create a new device type
         */
        time_t t;
        time(&t);
        json_t *jn_device_type = json_pack("{s:s, s:s, s:s, s:s, s:{}, s:O, s:I}",
            "id", type_id,
            "name", type_id,
            "description", "",
            "icon", "",
            "properties",
            "template_settings", template_settings?template_settings:json_null(),
            "tm", (json_int_t)t
        );

        device_type = gobj_create_node(
            priv->gobj_treedb_mqtt_broker,
            "device_types",
            jn_device_type,
            0,
            gobj
        );
    } else {
        /*
         *  Check if settings template has a new version
         */
        json_int_t device_type_template_settings_version = kw_get_int(
            gobj, device_type, "template_settings`template_version`default", 0, KW_WILD_NUMBER
        );
        if(schema_template_settings_version > device_type_template_settings_version) {
            JSON_DECREF(device_type)

            time_t t;
            time(&t);
            device_type = gobj_update_node(
                priv->gobj_treedb_mqtt_broker,
                "device_types",
                json_pack("{s:s, s:O, s:I}",
                    "id", type_id,
                    "template_settings", template_settings?template_settings:json_null(),
                    "tm", (json_int_t)t
                ),
                0,  // fkey,hook options
                gobj
            );
        }
    }

    /*
     *  Set type if not defined
     */
    json_t *device_type_ = kw_get_list(gobj, device, "device_type", 0, KW_REQUIRED);
    if(json_array_size(device_type_)==0) {
        gobj_link_nodes(
            priv->gobj_treedb_mqtt_broker,
            "devices",                  // const char *hook,
            "device_types",             // const char *parent_topic_name,
            json_incref(device_type),   // json_t *parent_record,  // owned
            "devices",                  // const char *child_topic_name,
            json_incref(device),        // json_t *child_record,  // owned
            gobj
        );
    }

    /*
     *  Set settings if not defined, or if it has a minor version
     */
    json_t *settings = kw_get_dict(gobj, device, "settings", 0, KW_REQUIRED);
    json_int_t template_settings_version = kw_get_int(gobj, settings, "template_version", 0, 0);

    if(json_object_size(settings)==0 ||
            schema_template_settings_version > template_settings_version) {
        JSON_INCREF(settings);
        json_t *jn_settings = create_template_record("settings", template_settings, settings);

        JSON_DECREF(device)
        device = gobj_update_node( // Return is YOURS
            priv->gobj_treedb_mqtt_broker,
            "devices",
            json_pack("{s:s, s:o}",
                "id", device_id,
                "settings", jn_settings
            ),
            0,  // fkey,hook options
            gobj
        );
    }
    JSON_DECREF(jn_desc)

    /*------------------------------------------------------*
     *  Actualiza nombre si ha cambiado en el dispositivo
     *------------------------------------------------------*/
    const char *old_name = kw_get_str(gobj, device, "name", "", 0);
    if(empty_string(old_name)) {
        old_name = device_id;
    }

    /*--------------------------------*
     *      Build track message
     *--------------------------------*/
    /*
     *  Pon el name del device
     */
    json_object_set_new(
        kw,
        "name",
        json_string(old_name)
    );

    /*--------------------------------------*
     *  Get the first group of the device
     *  to get the language
     *--------------------------------------*/
    const char *language = "es";
    json_t *group = 0;
    json_t *device_groups = kw_get_list(gobj, device, "device_groups", 0, KW_REQUIRED);
    if(device_groups && json_array_size(device_groups)) {
        const char *group_id = json_string_value(json_array_get(device_groups, 0));
        group = gobj_get_node(
            priv->gobj_treedb_mqtt_broker,
            "device_groups",
            json_pack("{s:s}",
                "id", group_id
            ),
            json_pack("{s:b}",
                "only_id", 1
            ),
            gobj
        );
        if(group) {
            language = kw_get_str(gobj, group, "language", language, KW_REQUIRED);
        }
        // TODO override the language if user to send the email has a language defined
    }

    // /*--------------------------------*
    //  *      Check alarms
    //  *--------------------------------*/
    // int n_alarms = check_new_alarms(
    //     gobj,
    //     device, // not owned
    //     kw,    // not owned
    //     language,
    //     FALSE
    // );
    //
    // /*
    //  *  Adjunta nº alarmas
    //  */
    // json_object_set_new(
    //     kw,
    //     "n_alarms",
    //     json_integer(n_alarms)
    // );
    //
    // /*--------------------------------*
    //  *      Publish the trace
    //  *--------------------------------*/
    // gobj_publish_event(gobj, EV_REALTIME_TRACK, json_incref(kw));

    /*---------------------*
     *      Free device
     *---------------------*/
    JSON_DECREF(group)
    JSON_DECREF(device)
    JSON_DECREF(device_type)

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
