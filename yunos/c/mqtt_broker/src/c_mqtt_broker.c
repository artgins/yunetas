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

#include "c_mqtt_broker.h"

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
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "authzs",           0,                  0,              pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag--------default-----description---------- */
SDATA (DTP_STRING,      "tranger_path",     SDF_RD,     "/yuneta/store/mqtt-broker/(^^__node_owner__^^)",         "tranger path"),
SDATA (DTP_STRING,      "tranger_database", SDF_RD,     "(^^__yuno_role__^^)^(^^__yuno_name__^^)",         "tranger database"),
SDATA (DTP_STRING,      "filename_mask",    SDF_RD|SDF_REQUIRED,"%Y-%m-%d",    "Organization of tables (file name format, see strftime())"),
SDATA (DTP_INTEGER,     "on_critical_error",SDF_RD,     "0x0010",   "LOG_OPT_TRACE_STACK"),

SDATA (DTP_POINTER,     "subscriber",       0,          0,          "Subscriber of output-events. If it's null then the subscriber is the parent."),
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,     "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,          0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,          0,          "more user data"),
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
    hgobj gobj_input_side;

    hgobj gobj_tranger_broker;
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
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

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
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // set_timeout(priv->timer, priv->timeout);

    /*-------------------------*
     *      Start services
     *-------------------------*/
    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    /*--------------------------------*
     *      Tranger database
     *--------------------------------*/
    const char *path = gobj_read_str_attr(gobj, "tranger_path");
    if(empty_string(path)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger path EMPTY",
            NULL
        );
        return -1;
    }

    const char *database = gobj_read_str_attr(gobj, "tranger_database");
    if(empty_string(database)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger database EMPTY",
            NULL
        );
        return -1;
    }

    /*---------------------------------*
     *      Open Timeranger
     *---------------------------------*/
    json_t *kw_tranger = json_pack("{s:s, s:s, s:s, s:b, s:I, s:i}",
        "path", path,
        "database", database,
        "filename_mask", gobj_read_str_attr(gobj, "filename_mask"),
        "master", 1,
        "subscriber", (json_int_t)(uintptr_t)gobj,
        "on_critical_error", (int)gobj_read_integer_attr(gobj, "on_critical_error")
    );
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "tranger_%s", gobj_name(gobj));
    priv->gobj_tranger_broker = gobj_create_service(
        name,
        C_TRANGER,
        kw_tranger,
        gobj
    );
    gobj_start(priv->gobj_tranger_broker);

    // TODO
    // priv->trq_msgs = trq_open(
    //     priv->tranger,
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

    /*---------------------------------------*
     *      Stop services
     *---------------------------------------*/
    if(priv->gobj_input_side) {
        if(gobj_is_playing(priv->gobj_input_side)) {
            gobj_pause(priv->gobj_input_side);
        }
        gobj_stop_tree(priv->gobj_input_side);
    }

    /*----------------------------------*
     *      Close Timeranger
     *----------------------------------*/
    // TODO
    // EXEC_AND_RESET(trq_close, priv->trq_msgs);

    gobj_stop(priv->gobj_tranger_broker);

    EXEC_AND_RESET(gobj_destroy, priv->gobj_tranger_broker);

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
PRIVATE int open_queue(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Identity_card on from
 *      mqtt clients (__input_side__)
 *
    {
        "client_id": "DVES_40AC66",
        "assigned_id": false,       #^^ if assigned_id is true the client_id is temporary.
        "username": "DVES_USER",
        "password": "DVES_PASS",
        "clean_start": true,
        "protocol_version": 2,
        "protocol_name": "MQTT",
        "keepalive": 30,
        "session_expiry_interval": 0,
        "max_qos": 2,

        "will": true,
        "will_retain": true,
        "will_qos": 1,
        "will_topic": "tele/tasmota_40AC66/LWT",    #^^ these will fields are optionals
        "will_delay_interval": 0,
        "will_expiry_interval": 0,
        "gbuffer": 95091873745312,  #^^ it contents the will payload

        "__temp__": {
            "channel": "input-1",
            "channel_gobj": 95091872991280
        }
    }
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_OPEN %s", gobj_short_name(src)
        );
    }

    /*---------------------------------------------*
     *  MQTT Client ID <==> topic in Timeranger
     *---------------------------------------------*/
    const char *username = kw_get_str(gobj, kw, "username", "", KW_REQUIRED);
    const char *password = kw_get_str(gobj, kw, "password", "", KW_REQUIRED);
    const char *client_id = kw_get_str(gobj, kw, "client_id", "", KW_REQUIRED);
    BOOL clean_start = kw_get_bool(gobj, kw, "clean_start", 0, KW_REQUIRED);
    BOOL will = kw_get_bool(gobj, kw, "will", 0, KW_REQUIRED);
    if(will) {
        // TODO read the remain will fields
    }

    /*----------------------------------------------------------------*
     *  Open the topic (client_id) or create it if it doesn't exist
     *----------------------------------------------------------------*/
    json_t *jn_response = gobj_command(
        priv->gobj_tranger_broker,
        "open-topic",
        json_pack("{s:s}",
            "topic_name",
            client_id
        ),
        gobj
    );

    int result =  COMMAND_RESULT(gobj, jn_response);
    if(result < 0) {
        JSON_DECREF(jn_response)
        jn_response = gobj_command(
            priv->gobj_tranger_broker,
            "create-topic", // idempotent function
            json_pack("{s:s}",
                "topic_name",
                client_id
            ),
            gobj
        );
        result =  COMMAND_RESULT(gobj, jn_response);
    }
    if(result < 0) {
        const char *comment = COMMAND_COMMENT(gobj, jn_response);
        JSON_DECREF(jn_response)
        KW_DECREF(kw);
        return -1;
    }

    json_t *topic = COMMAND_DATA(gobj, jn_response);


    JSON_DECREF(jn_response)
    KW_DECREF(kw);
    return result;
}

/***************************************************************************
 *  Identity_card off from
 *      mqtt clients (__input_side__)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    // PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(
            gobj,
            kw, // not own
            "ON_CLOSE %s", gobj_short_name(src)
        );
    }

    // TODO do will job ?

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
        {EV_ON_OPEN,                ac_on_open,              0},
        {EV_ON_CLOSE,               ac_on_close,             0},
        {EV_TIMEOUT,                ac_timeout,              0},
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
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_TIMEOUT,                0},
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
