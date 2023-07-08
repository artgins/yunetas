/****************************************************************************
 *          c_esp_yuno.c
 *
 *          GClass __yuno__
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#ifdef ESP_PLATFORM
    #include <esp_event.h>
    #include <esp_netif.h>
    #include <esp_netif_sntp.h>
    #include <esp_mac.h>
    #include <esp_log.h>
    #include <esp_sntp.h>
    #include <driver/gpio.h>
    #include <rom/gpio.h>
#endif
#include <time.h>
#include <log_udp_handler.h>    // log upd is open when wifi/ethernet is connected
#include <gobj_environment.h>
#include <kwid.h>
#include "c_esp_ethernet.h"
#include "c_esp_wifi.h"
#include "c_timer.h"
#include "c_esp_yuno.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define OLIMEX_LED_PIN      33  // TODO put in config

/***************************************************************
 *              Prototypes
 ***************************************************************/
#ifdef ESP_PLATFORM
static void event_loop_callback(
    void *event_handler_arg,
    esp_event_base_t base,
    int32_t id,
    void *event_data
);
#endif

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
SDATA (DTP_STRING,  "url_esp_ota",      SDF_PERSIST,    "",             "ESP OTA url"), // TODO OTA
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
#ifdef ESP_PLATFORM
    esp_event_loop_handle_t ev_loop_post_messages;  // Event loop for post messages
#endif
    hgobj gobj_wifi;
    hgobj gobj_ethernet;
    hgobj gobj_timer;
    json_int_t periodic;
    json_int_t autokill;
} PRIVATE_DATA;

PRIVATE hgclass gclass = 0;
#ifdef ESP_PLATFORM
ESP_EVENT_DEFINE_BASE(GOBJ_END);
#endif




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

#ifdef ESP_PLATFORM
    /*-----------------------------------*
     *  Initialize default loop, netif
     *-----------------------------------*/
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    /*--------------------------------*
     *      Create own event loop
     *--------------------------------*/
    esp_event_loop_args_t loop_handle_args = {
        .queue_size = 16,
        .task_name = "yuneta_loop", // task will be created
        .task_priority = tskIDLE_PRIORITY,
        .task_stack_size = 8*1024,  // esp32 stack size
        .task_core_id = 1
    };
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_handle_args, &priv->ev_loop_post_messages));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        priv->ev_loop_post_messages,    // event loop handle
        ESP_EVENT_ANY_BASE,         // event base
        ESP_EVENT_ANY_ID,           // event id
        event_loop_callback,        // event handler
        gobj,                       // event_handler_arg = yuno
        NULL                        // event handler instance, useful to unregister callback
    ));

    /*---------------------------*
     *      Set date/time
     *---------------------------*/
    if(esp_reset_reason() == ESP_RST_POWERON || 1) {
        int64_t timestamp = gobj_read_integer_attr(gobj, "timestamp");
        struct timeval get_nvs_time = {
            .tv_sec = timestamp,
        };
        settimeofday(&get_nvs_time, NULL);
    }
#endif

    time_t now;
    time(&now);
    gobj_write_integer_attr(gobj, "start_time", now);
    node_uuid();

    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->gobj_wifi = gobj_create_service("wifi", C_WIFI, 0, gobj);
    gobj_subscribe_event(priv->gobj_wifi, NULL, NULL, gobj);

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

#ifdef ESP_PLATFORM
    gpio_pad_select_gpio(OLIMEX_LED_PIN);   // TODO put in config
    gpio_set_direction(OLIMEX_LED_PIN, GPIO_MODE_OUTPUT);
#endif

    gobj_start(priv->gobj_timer);

    /*
     *  Start wifi/ethernet and wait to connect some network before playing the default_service
     */
    gobj_start(priv->gobj_wifi);

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

#ifdef ESP_PLATFORM
    esp_event_post_to(
        priv->ev_loop_post_messages,
        GOBJ_END,
        0,
        0,
        0,
        portMAX_DELAY
    );
#endif

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->ev_loop_post_messages) {
        esp_event_loop_delete(priv->ev_loop_post_messages);
        priv->ev_loop_post_messages = 0;
    }
#endif
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
    // NOT here! gobj_play(gobj_default_service());

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
 *  Two types of data can be passed in to the event handler:
 *      - the handler specific data
 *      - the event-specific data.
 *
 *  - The handler specific data (handler_args) is a pointer to the original data,
 *  therefore, the user should ensure that the memory location it points to
 *  is still valid when the handler executes.
 *
 *  - The event-specific data (event_data) is a pointer to a deep copy of the original data,
 *  and is managed automatically.
***************************************************************************/
#ifdef ESP_PLATFORM
static void event_loop_callback(
    void *event_handler_arg,
    esp_event_base_t base,
    int32_t id,
    void *event_data
) {
    if(base == GOBJ_END) {
        gobj_destroy(gobj_yuno());
        gobj_end();
        // esp_restart(); TODO repon
        return;
    }

    hgobj yuno = event_handler_arg;
    json_t *jn = *((json_t **) event_data);

    hgobj dst = (hgobj)(size_t)kw_get_int(yuno, jn, "dst", 0, KW_REQUIRED);
    gobj_event_t event = (gobj_event_t)(size_t)kw_get_int(yuno, jn, "event", 0, KW_REQUIRED);
    json_t *kw = kw_get_dict(yuno, jn, "kw", 0, KW_REQUIRED);
    hgobj src = (hgobj)(size_t)kw_get_int(yuno, jn, "src", 0, KW_REQUIRED);

    gobj_send_event(dst, event, json_incref(kw), src);
    json_decref(jn);
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
#ifdef ESP_PLATFORM
PRIVATE void time_sync_notification_cb(struct timeval *tv)
{
    gobj_log_warning(0, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "time_sync_notification",
        "tv_sec",       "%lld", (long long int)tv->tv_sec,
        "tv_usec",      "%lld", (long long int)tv->tv_usec,
        NULL
    );

    gobj_post_event(gobj_yuno(), EV_YUNO_TIME_ON, 0, gobj_yuno());
}
#endif

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
 *  Now we have network IP by wifi
 *  Sockets can be open
 ***************************************************************************/
PRIVATE int ac_wifi_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  Wait to have time to play the default service
     */
#ifdef ESP_PLATFORM
    #define SNTP_TIME_SERVER "pool.ntp.org"
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_TIME_SERVER);
    config.sync_cb = time_sync_notification_cb;     // Note: This is only needed if we want
    esp_netif_sntp_init(&config);

    /*
     *  Set upd handler
     */
    //gobj_log_del_handler(NULL); // Delete all handlers TODO repon cuando funcione

    udpc_t udpc = udpc_open(
        gobj_read_str_attr(gobj, "url_udp_log"),
        NULL,   // bindip
        8*1024, // bfsize
        0,      // udp_frame_size
        0,      // output_format
        FALSE   // exit on failure
    );
    if(udpc) {
        gobj_log_add_handler("udp", "udp", LOG_OPT_ALL, udpc);
        //esp_log_set_vprintf(udp_log); TODO set in prod
    }
    gpio_set_level(OLIMEX_LED_PIN, 1); // TODO put in config

#endif

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Now we have NOT network IP by wifi
 *  Sockets must be close
 ***************************************************************************/
PRIVATE int ac_wifi_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  save current time in nvs
     */
    time_t now;
    time(&now);
    gobj_write_integer_attr(gobj, "timestamp", now);
    gobj_save_persistent_attrs(gobj, json_string("timestamp"));

#ifdef ESP_PLATFORM
    gpio_set_level(OLIMEX_LED_PIN, 0); // TODO put in config
    esp_netif_sntp_deinit();
#endif

//    esp_log_set_vprintf(vprintf); // TODO repon cuando funcione
//    gobj_log_del_handler(NULL); // Delete all handlers; it does the close()
    gobj_log_del_handler("udp");

    /*
     *  Pause default service
     */
    gobj_pause(gobj_default_service());

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Now we have network IP by wifi
 *  Sockets can be open
 ***************************************************************************/
PRIVATE int ac_time_on(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  Save the time in nvs
     */
    time_t now;
    time(&now);
    gobj_write_integer_attr(gobj, "timestamp", now);
    gobj_save_persistent_attrs(gobj, json_string("timestamp"));

    /*
     *  Play default service
     */
    if(!gobj_is_playing(gobj_default_service())) {
        gobj_play(gobj_default_service());
    }

    JSON_DECREF(kw)
    return 0;
}

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
    gobj_publish_event(gobj, EV_TIMEOUT_PERIODIC, 0);
    if(gobj_current_state(gobj) == ST_YUNO_NETWORK_ON) {
#ifdef ESP_PLATFORM
        sntp_sync_status_t status = sntp_get_sync_status();
        if(status == SNTP_SYNC_STATUS_COMPLETED) {
            gobj_send_event(gobj, EV_YUNO_TIME_ON, 0, gobj);
        }
#endif
    }

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
GOBJ_DEFINE_STATE(ST_YUNO_NETWORK_OFF);
GOBJ_DEFINE_STATE(ST_YUNO_NETWORK_ON);
GOBJ_DEFINE_STATE(ST_YUNO_TIME_ON);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_YUNO_TIME_ON);

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
    ev_action_t st_network_off[] = {
        {EV_WIFI_ON_OPEN,           ac_wifi_on_open,        ST_YUNO_NETWORK_ON},
        {EV_TIMEOUT_PERIODIC,       ac_periodic_timeout,    0},
        {0,0,0}
    };
    ev_action_t st_network_on[] = {
        {EV_WIFI_ON_CLOSE,          ac_wifi_on_close,       ST_YUNO_NETWORK_OFF},
        {EV_YUNO_TIME_ON,           ac_time_on,             ST_YUNO_TIME_ON},
        {EV_TIMEOUT_PERIODIC,       ac_periodic_timeout,    0},
        {0,0,0}
    };
    ev_action_t st_time_on[] = {
        {EV_WIFI_ON_CLOSE,          ac_wifi_on_close,       ST_YUNO_NETWORK_OFF},
        {EV_YUNO_TIME_ON,           ac_time_on,             0},
        {EV_TIMEOUT_PERIODIC,       ac_periodic_timeout,    0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_YUNO_NETWORK_OFF,   st_network_off},
        {ST_YUNO_NETWORK_ON,    st_network_on},
        {ST_YUNO_TIME_ON,       st_time_on},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT_PERIODIC,     EVF_OUTPUT_EVENT},
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
PUBLIC int register_c_esp_yuno(void)
{
    return create_gclass(C_YUNO);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_post_event(
    hgobj dst,
    gobj_event_t event,
    json_t *kw,  // owned
    hgobj src
) {
    hgobj yuno = gobj_yuno();
    json_error_t error;
    json_t *jn = json_pack_ex(&error, 0, "{s:I, s:I, s:o, s:I}",
        "dst", (json_int_t)(size_t)dst,
        "event", (json_int_t)(size_t)event,
        "kw", kw?kw:json_object(),
        "src", (json_int_t)(size_t)src
    );
    if(!jn) {
        gobj_log_error(yuno, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "json_pack_ex() FAILED",
            "error",        "%s", error.text,
            NULL
        );
        return -1;
    }

#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(yuno);
    esp_err_t err = esp_event_post_to(
        priv->ev_loop_post_messages,
        event,
        0,
        &jn,
        sizeof(json_t *),
        portMAX_DELAY
    );
    if(err != ESP_OK) {
        gobj_log_error(yuno, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "esp_event_post_to() FAILED",
            "esp_error",    "%s", esp_err_to_name(err),
            NULL
        );
    }
#else
    JSON_DECREF(jn)
#endif

    return 0;
}
