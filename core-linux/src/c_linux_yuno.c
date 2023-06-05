/****************************************************************************
 *          c_linux_yuno.c
 *
 *          GClass __yuno__
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>
#include "environment.h"
#include "c_timer.h"
#include "c_linux_yuno.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type---------name---------------flag------------default---------description---------- */
SDATA (DTP_STRING,  "url_udp_log",      SDF_PERSIST,    "",             "UDP Log url"),
SDATA (DTP_STRING,  "process",          SDF_RD,         "",             "Process name"),
SDATA (DTP_STRING,  "hostname",         SDF_RD,         "",             "Hostname"),
SDATA (DTP_INTEGER, "pid",              SDF_RD,         "",             "pid"),
SDATA (DTP_STRING,  "node_uuid",        SDF_RD,         "",             "uuid of node"),
SDATA (DTP_STRING,  "node_owner",       SDF_RD,         "",             "Owner of node"),
SDATA (DTP_STRING,  "realm_id",         SDF_RD,         "",             "Realm id where the yuno lives"),
SDATA (DTP_STRING,  "realm_owner",      SDF_RD,         "",             "Owner of realm"),
SDATA (DTP_STRING,  "realm_role",       SDF_RD,         "",             "Role of realm"),
SDATA (DTP_STRING,  "realm_name",       SDF_RD,         "",             "Name of realm"),
SDATA (DTP_STRING,  "realm_env",        SDF_RD,         "",             "Environment of realm"),
SDATA (DTP_STRING,  "yuno_id",          SDF_RD,         "",             "Yuno Id"),
SDATA (DTP_STRING,  "yuno_name",        SDF_RD,         "",             "Yuno name"),
SDATA (DTP_STRING,  "yuno_tag",         SDF_RD,         "",             "Tags of yuno"),
SDATA (DTP_STRING,  "yuneta_version",   SDF_RD,         YUNETA_VERSION, "Yuneta version"),
SDATA (DTP_DICT,    "trace_levels",     SDF_PERSIST,    "{}",           "Trace levels"),
SDATA (DTP_DICT,    "no_trace_levels",  SDF_PERSIST,    "{}",           "No trace levels"),
SDATA (DTP_INTEGER, "periodic",         SDF_RD,         "1000",         "Timeout periodic, in miliseconds"),
SDATA (DTP_INTEGER, "autokill",         SDF_RD,         "0",            "Timeout (>0) to autokill in seconds"),
SDATA (DTP_INTEGER, "timestamp",        SDF_PERSIST,    "0",            "Time of system, save in nvs each hour"),
SDATA (DTP_INTEGER, "start_time",       SDF_RD|SDF_STATS,"0",           "Yuno starting time"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
enum {
    TRACE_TRAFFIC           = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"traffic",             "Trace dump traffic"},
    {0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
//SDATAAUTHZ (ASN_SCHEMA, "open-console",     0,      0,      0,      "Permission to open console"),
//SDATAAUTHZ (ASN_SCHEMA, "close-console",    0,      0,      0,      "Permission to close console"),
//SDATAAUTHZ (ASN_SCHEMA, "list-consoles",    0,      0,      0,      "Permission to list consoles"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_timer;
    json_int_t periodic;
    json_int_t autokill;
} PRIVATE_DATA;

PRIVATE hgclass gclass = 0;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    time_t now;
    time(&now);
    gobj_write_integer_attr(gobj, "start_time", now);
    node_uuid();

    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    char timestamp[90];
    current_timestamp(timestamp, sizeof(timestamp));
    gobj_log_warning(0, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "start_time",
        "t",            "%lld", (long long int)now,
        "start_date",   "%s", timestamp,
        NULL
    );

    SET_PRIV(periodic,   gobj_read_integer_attr)
    SET_PRIV(autokill,   gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(periodic, gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(autokill, gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_timer);

    set_timeout_periodic(priv->gobj_timer, priv->periodic);

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    /*
     *  When yuno stops, it's the death of the app
     */
    clear_timeout(priv->gobj_timer);
    gobj_stop(priv->gobj_timer);
    gobj_stop_childs(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    /*
     *  This play order can come from yuneta_agent or autoplay config option
     *  Organize the gobj's play as you want.
     */

    /*
     *  The order to play the default service is in the action of networking on
     */
    gobj_play(gobj_default_service());

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    /*
     *  This pause order can come from yuneta_agent or from yuno stop
     *  Organize the gobj's pause as you want.
     */

    /*
     *  The order to pause the default service is too in the action of networking off
     */
    gobj_pause(gobj_default_service());
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
//     KW_INCREF(kw)
//     json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
//     return msg_iev_build_webix(
//         gobj,
//         0,
//         jn_resp,
//         0,
//         0,
//         kw  // owned
//     );
    return json_object();
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *  Send logs to log center by udp
 ***************************************************************************/
#ifdef ESP_PLATFORM
PRIVATE int udp_log(const char *fmt, va_list ap)
{
    trace_vjson(0, 0, "esp_log", fmt, ap);
    return 0;
}
#endif




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_periodic_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    static json_int_t i = 0;
    i++;

    if(priv->autokill > 0) {
        if(i >= priv->autokill) {
            gobj_trace_msg(gobj, "===========> SHUTDOWN <===========");
            gobj_shutdown();
            JSON_DECREF(kw)
            return -1;
        }
    }

    // Let others uses the periodic timer, save resources
    gobj_publish_event(gobj, EV_PERIODIC_TIMEOUT, 0);

    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_writing = mt_writing,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*---------------------------------------------*
 *          Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_YUNO);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    if(gclass) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_PERIODIC_TIMEOUT,       ac_periodic_timeout,    0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_PERIODIC_TIMEOUT,     EVF_OUTPUT_EVENT},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    gclass = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        lmt,
        tattr_desc,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0   // gclass_flag
    );
    if(!gclass) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_linux_yuno(void)
{
    return create_gclass(C_YUNO);
}
