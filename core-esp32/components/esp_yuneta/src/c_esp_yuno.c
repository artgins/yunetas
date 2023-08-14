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
#include <command_parser.h>
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
PRIVATE json_t *cmd_view_gclass_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_service_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_write_attr(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_attrs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_attrs2(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_gclass_name[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gclass_name",  0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "gclass",       0,              0,          "gclass-name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_wr_attr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "__default_service__", "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "attribute",    0,              0,          "attribute name"),
SDATAPM (DTP_STRING,    "value",        0,              0,          "value"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_gobj_def_name[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "__default_service__", "named-gobj or full gobj name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_services[] = {"services", "list-services", 0};
PRIVATE const char *a_read_attrs[] = {"read-attrs", 0};
PRIVATE const char *a_read_attrs2[] = {"read-attrs2", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------------------alias---items-------json_fn---------description*/
SDATACM (DTP_SCHEMA,    "help",                     a_help, pm_help,    cmd_help,       "Command's help"),

SDATACM (DTP_SCHEMA,    "view-gclass-register",     0,      0,          cmd_view_gclass_register,"View gclass's register"),
SDATACM (DTP_SCHEMA,    "view-service-register",    a_services,pm_gclass_name,cmd_view_service_register,"View service's register"),

SDATACM (DTP_SCHEMA,    "write-attr",               0,      pm_wr_attr, cmd_write_attr, "Write a writable attribute)"),
SDATACM (DTP_SCHEMA,    "view-attrs",               a_read_attrs,pm_gobj_def_name, cmd_view_attrs,      "View gobj's attrs"),
SDATACM (DTP_SCHEMA,    "view-attrs2",              a_read_attrs2,pm_gobj_def_name, cmd_view_attrs2,    "View gobj's attrs with details"),

SDATA_END()
};

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

SDATA (DTP_STRING,  "yuno_role",        SDF_RD,         "",             "Yuno Role"),
SDATA (DTP_STRING,  "yuno_id",          SDF_RD,         "",             "Yuno Id. Set by agent"),
SDATA (DTP_STRING,  "yuno_name",        SDF_RD,         "",             "Yuno name. Set by agent"),
SDATA (DTP_STRING,  "yuno_tag",         SDF_RD,         "",             "Tags of yuno. Set by agent"),

SDATA (DTP_STRING,  "yuno_version",     SDF_RD,         "",             "Yuno version (APP_VERSION)"),
SDATA (DTP_STRING,  "yuneta_version",   SDF_RD,         YUNETA_VERSION, "Yuneta version"),

SDATA (DTP_STRING,  "appName",          SDF_RD,         "",             "App name, must match the role"),
SDATA (DTP_STRING,  "appDesc",          SDF_RD,         "",             "App Description"),
SDATA (DTP_STRING,  "appDate",          SDF_RD,         "",             "App date/time"),

SDATA (DTP_DICT,    "trace_levels",     SDF_PERSIST,    "{}",           "Trace levels"),
SDATA (DTP_DICT,    "no_trace_levels",  SDF_PERSIST,    "{}",           "No trace levels"),
SDATA (DTP_INTEGER, "periodic",         SDF_RD,         "1000",         "Timeout periodic, in miliseconds"),
SDATA (DTP_INTEGER, "autokill",         SDF_RD,         "0",            "Timeout (>0) to autokill in seconds"),
SDATA (DTP_INTEGER, "timestamp",        SDF_PERSIST,    "0",            "Time of system, save in nvs each hour"),
SDATA (DTP_INTEGER, "start_time",       SDF_RD|SDF_STATS,"0",           "Yuno starting time"),
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

    json_t *attrs = gobj_hsdata(gobj);
    gobj_trace_json(gobj, attrs, "yuno's attrs");

    /*------------------------*
     *  Create childs
     *------------------------*/
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
    // WARNING: play of default service NOT here! gobj_play(gobj_default_service());

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
     json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
     return build_command_response(
         gobj,
         0,
         jn_resp,
         0,
         0
     );
}

/***************************************************************************
 *  Show register
 ***************************************************************************/
PRIVATE json_t *cmd_view_gclass_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_response = build_command_response(
        gobj,
        0,
        0,
        0,
        gobj_repr_gclass_register()
    );
    JSON_DECREF(kw)
    return jn_response;
}

/***************************************************************************
 *  Show register
 ***************************************************************************/
PRIVATE json_t *cmd_view_service_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gclass_name = kw_get_str(
        gobj,
        kw,
        "gclass_name",
        kw_get_str(gobj, kw, "gclass", "", 0),
        0
    );

    json_t *jn_response = build_command_response(
        gobj,
        0,
        0,
        0,
        gobj_repr_service_register(gclass_name)
    );
    JSON_DECREF(kw)
    return jn_response;
}

/***************************************************************************
 *  Write a boolean attribute
 ***************************************************************************/
PRIVATE json_t *cmd_write_attr(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gobj_name_ = kw_get_str( // __default_service__
        gobj,
        kw,
        "gobj_name",
        kw_get_str(gobj, kw, "gobj", "", 0),
        0
    );
    if(empty_string(gobj_name_)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("what gobj?"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    hgobj gobj2write = gobj_find_service(gobj_name_, FALSE);
    if(!gobj2write) {
        gobj2write = gobj_find_gobj(gobj_name_);
        if (!gobj2write) {
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("gobj not found: '%s'", gobj_name_),   // jn_comment
                0,      // jn_schema
                0       // jn_data
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    const char *attribute = kw_get_str(gobj, kw, "attribute", 0, 0);
    if(empty_string(attribute)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("what attribute?"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    if(!gobj_has_attr(gobj2write, attribute)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: attr not found: '%s'",
                gobj_short_name(gobj2write),
                attribute
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    if(!gobj_is_writable_attr(gobj2write, attribute)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: attr not writable: '%s'",
                gobj_short_name(gobj2write),
                attribute
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    const char *svalue = kw_get_str(gobj, kw, "value", 0, 0);
    if(!svalue) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("what value?"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    int ret = -1;
    int type = gobj_attr_type(gobj2write, attribute);
    switch(type) {
        case DTP_BOOLEAN:
            {
                BOOL value;
                if(strcasecmp(svalue, "true")==0) {
                    value = 1;
                } else if(strcasecmp(svalue, "false")==0) {
                    value = 0;
                } else {
                    value = atoi(svalue);
                }
                ret = gobj_write_bool_attr(gobj2write, attribute, value);
            }
            break;

        case DTP_STRING:
            {
                ret = gobj_write_str_attr(gobj2write, attribute, svalue);
            }
            break;

        case DTP_JSON:
            {
                json_t *jn_value = anystring2json(svalue, strlen(svalue), FALSE);
                if(!jn_value) {
                    json_t *kw_response = build_command_response(
                        gobj,
                        -1,     // result
                        json_sprintf(
                            "%s: cannot encode value string to json: '%s'",
                            gobj_short_name(gobj2write),
                            svalue
                        ),
                        0,      // jn_schema
                        0       // jn_data
                    );
                    JSON_DECREF(kw)
                    return kw_response;
                }
                ret = gobj_write_new_json_attr(gobj2write, attribute, jn_value);
            }
            break;

        case DTP_INTEGER:
            {
                if(DTP_IS_INTEGER(type)) {
                    int64_t value;
                    value = strtoll(svalue, 0, 10);
                    ret = gobj_write_integer_attr(gobj2write, attribute, value);
                } else if(DTP_IS_REAL(type)) {
                    double value;
                    value = strtod(svalue, 0);
                    ret = gobj_write_real_attr(gobj2write, attribute, value);
                } else if(DTP_IS_BOOLEAN(type)) {
                    double value;
                    value = strtod(svalue, 0);
                    ret = gobj_write_bool_attr(gobj2write, attribute, value?1:0);
                }
            }
            break;
        default:
            break;
    }

    if(ret<0) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: Can't write attr: '%s'",
                gobj_short_name(gobj2write),
                attribute
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }
    gobj_save_persistent_attrs(gobj2write, json_string(attribute));

    json_t *kw_response = build_command_response(
        gobj,
        0,     // result
        json_sprintf(
            "%s: %s=%s done",
            gobj_short_name(gobj2write),
            attribute,
            svalue
        ),
        0,      // jn_schema
        gobj_read_attrs(gobj2write, SDF_PERSIST|SDF_RD|SDF_WR|SDF_STATS|SDF_RSTATS|SDF_PSTATS, gobj)
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Show a gobj's attrs
 ***************************************************************************/
PRIVATE json_t *cmd_view_attrs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gobj_name_ = kw_get_str( // __default_service__
        gobj,
        kw,
        "gobj_name",
        kw_get_str(gobj, kw, "gobj", "", 0),
        0
    );
    if(empty_string(gobj_name_)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("what gobj?"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    hgobj gobj2read = gobj_find_service(gobj_name_, FALSE);
    if(!gobj2read) {
        gobj2read = gobj_find_gobj(gobj_name_);
        if (!gobj2read) {
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("gobj not found: '%s'", gobj_name_),   // jn_comment
                0,      // jn_schema
                0       // jn_data
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        0,      // jn_comment
        0,      // jn_schema
        gobj_read_attrs(gobj2read, SDF_PERSIST|SDF_RD|SDF_WR|SDF_STATS|SDF_RSTATS|SDF_PSTATS, gobj)
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Show a gobj's attrs with detail
 ***************************************************************************/
PRIVATE json_t *cmd_view_attrs2(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gobj_name_ = kw_get_str( // __default_service__
        gobj,
        kw,
        "gobj_name",
        kw_get_str(gobj, kw, "gobj", "", 0),
        0
    );
    if(empty_string(gobj_name_)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("what gobj?"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    hgobj gobj2read = gobj_find_service(gobj_name_, FALSE);
    if(!gobj2read) {
        gobj2read = gobj_find_gobj(gobj_name_);
        if (!gobj2read) {
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("gobj not found: '%s'", gobj_name_),   // jn_comment
                0,      // jn_schema
                0       // jn_data
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        0,      // jn_comment
        0,      // jn_schema
        attr2json(gobj2read)
    );
    JSON_DECREF(kw)
    return kw_response;
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

    gpio_set_level(OLIMEX_LED_PIN, 1); // TODO put in config

    /*-----------------------------------*
     *      Start udp handler
     *-----------------------------------*/
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
        //esp_log_set_vprintf(udp_log); // TODO ???
    }
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
    //esp_log_set_vprintf(vprintf); // TODO ???
#endif

    /*-----------------------------------*
     *      Stop udp handler
     *-----------------------------------*/
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
            gobj_trace_msg(gobj, "❌❌❌❌ SHUTDOWN ❌❌❌❌");
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
        {EV_TIMEOUT_PERIODIC,     EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
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
