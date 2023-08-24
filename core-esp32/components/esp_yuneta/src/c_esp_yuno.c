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
#include <grp.h>
#include <log_udp_handler.h>    // log upd is open when wifi/ethernet is connected
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
PRIVATE hgclass get_gclass_from_gobj(const char *gobj_name);

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
PRIVATE int set_user_gclass_traces(hgobj gobj);
PRIVATE int set_user_gclass_no_traces(hgobj gobj);
PRIVATE int set_user_trace_filter(hgobj gobj);
PRIVATE int set_user_gobj_traces(hgobj gobj);
PRIVATE int set_user_gobj_no_traces(hgobj gobj);

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_gclass_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_service_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_write_attr(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_attrs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_attrs_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_view_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_mem(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_gclass(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_gobj(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_gobj_tree(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t* cmd_enable_gobj(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_disable_gobj(hgobj gobj, const char* cmd, json_t* kw, hgobj src);

PRIVATE json_t* cmd_list_persistent_attrs(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_remove_persistent_attrs(hgobj gobj, const char* cmd, json_t* kw, hgobj src);

PRIVATE json_t *cmd_info_global_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_global_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_global_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_info_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_gclass_no_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_no_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_get_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_gobj_no_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_no_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_set_trace_filter(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_trace_filter(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_reset_all_traces(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_deep_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_reset_log_counters(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_log_counters(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t* cmd_add_log_handler(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_del_log_handler(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_list_log_handlers(hgobj gobj, const char* cmd, json_t* kw, hgobj src);

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
PRIVATE sdata_desc_t pm_list_persistent_attrs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj"),
SDATAPM (DTP_STRING,    "gobj",         0,              0,          "named-gobj or full gobj"),
SDATAPM (DTP_STRING,    "attribute",    0,              0,          "Attribute to list/remove"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_remove_persistent_attrs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj"),
SDATAPM (DTP_STRING,    "gobj",         0,              0,          "named-gobj or full gobj"),
SDATAPM (DTP_STRING,    "attribute",    0,              0,          "Attribute to list/remove"),
SDATAPM (DTP_BOOLEAN,   "__all__",      0,              0,          "Remove all persistent attrs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_global_tr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "level",        0,              0,          "attribute name"),
SDATAPM (DTP_STRING,    "set",          0,              0,          "value"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_gclass_tr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gclass_name",  0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "gclass",       0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "level",        0,              0,          "attribute name"),
SDATAPM (DTP_STRING,    "set",          0,              0,          "value"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_reset_all_tr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj",         0,              "",         "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gclass",       0,              0,          "gclass-name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_gobj_root_name[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "__yuno__", "named-gobj or full gobj name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_gobj_tree[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "__yuno__", "named-gobj or full gobj name"),
SDATAPM (DTP_JSON,      "options",      0,              "[\"state\",\"running\"]",       "json list with strings, empty all"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_gobj_tr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "",         "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "level",        0,              0,          "attribute name"),
SDATAPM (DTP_STRING,    "set",          0,              0,          "value"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_set_trace_filter[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gclass_name",  0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "gclass",       0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "attr",         0,              "",         "Attribute of gobj to filter"),
SDATAPM (DTP_STRING,    "value",        0,              "",         "Value of attribute to filer"),
SDATAPM (DTP_STRING,    "set",          0,              0,          "value"),
SDATAPM (DTP_BOOLEAN,   "all",          0,              0,          "Remove all filters"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_deep_trace[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "set",          0,              0,          "value"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_add_log_handler[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              0,          "Handler log name"),
SDATAPM (DTP_STRING,    "type",         0,              0,          "Handler log type"),
SDATAPM (DTP_STRING,    "options",      0,              0,          "Handler log options"),
SDATAPM (DTP_STRING,    "url",          0,              0,          "Url for log 'udp' type"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_del_log_handler[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              0,          "Handler name"),
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
PRIVATE const char *a_pers_attrs[] = {"persistent-attrs", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------------------alias---items-------json_fn---------description*/
SDATACM (DTP_SCHEMA,    "help",                     a_help, pm_help,    cmd_help,       "Command's help"),

SDATACM (DTP_SCHEMA,    "reset-log-counters",       0,      0,          cmd_reset_log_counters,     "Reset log counters"),
SDATACM (DTP_SCHEMA,    "view-log-counters",        0,      0,          cmd_view_log_counters,      "View log counters"),
SDATACM (DTP_SCHEMA,    "add-log-handler",          0,      pm_add_log_handler,cmd_add_log_handler, "Add log handler"),
SDATACM (DTP_SCHEMA,    "delete-log-handler",       0,      pm_del_log_handler,cmd_del_log_handler, "Delete log handler"),
SDATACM (DTP_SCHEMA,    "list-log-handlers",        0,      0,          cmd_list_log_handlers,      "List log handlers"),

SDATACM (DTP_SCHEMA,    "view-gclass-register",     0,      0,          cmd_view_gclass_register,   "View gclass's register"),
SDATACM (DTP_SCHEMA,    "view-service-register",    a_services,pm_gclass_name,cmd_view_service_register,"View service's register"),

SDATACM (DTP_SCHEMA,    "write-attr",               0,      pm_wr_attr, cmd_write_attr,             "Write a writable attribute)"),
SDATACM (DTP_SCHEMA,    "view-attrs",               a_read_attrs,pm_gobj_def_name, cmd_view_attrs,  "View gobj's attrs"),
SDATACM (DTP_SCHEMA,    "view-attrs-schema",        a_read_attrs2,pm_gobj_def_name, cmd_attrs_schema,"View gobj's attrs schema"),

SDATACM (DTP_SCHEMA,    "view-config",              0,      0,          cmd_view_config,            "View final json configuration"),
SDATACM (DTP_SCHEMA,    "view-mem",                 0,      0,          cmd_view_mem,               "View yuno memory"),

SDATACM (DTP_SCHEMA,    "view-gclass",              0,      pm_gclass_name, cmd_view_gclass,        "View gclass description"),
SDATACM (DTP_SCHEMA,    "view-gobj",                0,      pm_gobj_def_name, cmd_view_gobj,        "View gobj"),

SDATACM (DTP_SCHEMA,    "view-gobj-tree",           0,      pm_gobj_tree,cmd_view_gobj_tree,   "View gobj tree"),

SDATACM (DTP_SCHEMA,    "enable-gobj",              0,      pm_gobj_def_name,cmd_enable_gobj,       "Enable named-gobj, exec own mt_enable() or gobj_start_tree()"),
SDATACM (DTP_SCHEMA,    "disable-gobj",             0,      pm_gobj_def_name,cmd_disable_gobj,      "Disable named-gobj, exec own mt_disable() or gobj_stop_tree()"),

SDATACM (DTP_SCHEMA,    "list-persistent-attrs",    a_pers_attrs,pm_list_persistent_attrs,cmd_list_persistent_attrs,  "List persistent attributes of yuno"),
SDATACM (DTP_SCHEMA,    "remove-persistent-attrs",  0,      pm_remove_persistent_attrs,cmd_remove_persistent_attrs,  "List persistent attributes of yuno"),

SDATACM (DTP_SCHEMA,    "info-global-trace",        0,      0,              cmd_info_global_trace,  "Info of global trace levels"),
SDATACM (DTP_SCHEMA,    "info-gclass-trace",        0,      pm_gclass_name, cmd_info_gclass_trace,  "Info of class's trace levels"),

SDATACM (DTP_SCHEMA,    "get-global-trace",         0,      0,              cmd_get_global_trace,   "Get global trace levels"),
SDATACM (DTP_SCHEMA,    "set-global-trace",         0,      pm_set_global_tr,cmd_set_global_trace,  "Set global trace level"),

SDATACM (DTP_SCHEMA,    "get-gclass-trace",         0,      pm_gclass_name, cmd_get_gclass_trace,   "Get gclass' trace"),
SDATACM (DTP_SCHEMA,    "set-gclass-trace",         0,      pm_set_gclass_tr,cmd_set_gclass_trace,  "Set trace of a gclass)"),
SDATACM (DTP_SCHEMA,    "get-gclass-no-trace",      0,      pm_gclass_name, cmd_get_gclass_no_trace,"Get no gclass' trace"),
SDATACM (DTP_SCHEMA,    "set-gclass-no-trace",      0,      pm_set_gclass_tr,cmd_set_no_gclass_trace,"Set no-trace of a gclass)"),

SDATACM (DTP_SCHEMA,    "get-gobj-trace",           0,      pm_gobj_root_name, cmd_get_gobj_trace,   "Get gobj's trace and his childs"),
SDATACM (DTP_SCHEMA,    "set-gobj-trace",           0,      pm_set_gobj_tr, cmd_set_gobj_trace,      "Set trace of a named-gobj"),
SDATACM (DTP_SCHEMA,    "get-gobj-no-trace",        0,      pm_gobj_root_name, cmd_get_gobj_no_trace,"Get no gobj's trace  and his childs"),
SDATACM (DTP_SCHEMA,    "set-gobj-no-trace",        0,      pm_set_gobj_tr, cmd_set_no_gobj_trace,   "Set no-trace of a named-gobj"),

SDATACM (DTP_SCHEMA,    "set-trace-filter",         0,      pm_set_trace_filter, cmd_set_trace_filter,"Set a gclass trace filter"),
SDATACM (DTP_SCHEMA,    "get-trace-filter",         0,      0, cmd_get_trace_filter, "Get trace filters"),

SDATACM (DTP_SCHEMA,    "reset-all-traces",         0,      pm_reset_all_tr, cmd_reset_all_traces,    "Reset all traces of a named-gobj of gclass"),
SDATACM (DTP_SCHEMA,    "set-deep-trace",           0,      pm_set_deep_trace,cmd_set_deep_trace,   "   Set deep trace, all traces active"),

SDATA_END()
};

/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type---------name---------------flag------------default---------description---------- */
SDATA (DTP_STRING,  "url_udp_log",      SDF_PERSIST,    "",             "UDP Log url"),
SDATA (DTP_STRING,  "process",          SDF_RD,         "",             "Process name"),
SDATA (DTP_STRING,  "hostname",         SDF_PERSIST,    "",             "Hostname"),
SDATA (DTP_STRING,  "__username__",     SDF_RD,         "",             "Username"),
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
SDATA (DTP_STRING,  "yuno_release",     SDF_RD,         "",             "Yuno Release. Set by agent"),

SDATA (DTP_STRING,  "yuno_version",     SDF_RD,         "",             "Yuno version (APP_VERSION)"),
SDATA (DTP_STRING,  "yuneta_version",   SDF_RD,         YUNETA_VERSION, "Yuneta version"),

SDATA (DTP_STRING,  "appName",          SDF_RD,         "",             "App name, must match the role"),
SDATA (DTP_STRING,  "appDesc",          SDF_RD,         "",             "App Description"),
SDATA (DTP_STRING,  "appDate",          SDF_RD,         "",             "App date/time"),

SDATA (DTP_INTEGER, "launch_id",        SDF_RD,         "0",            "Launch Id. Set by agent"),
SDATA (DTP_STRING,  "start_date",       SDF_RD|SDF_STATS, "",           "Yuno starting date"),
SDATA (DTP_INTEGER, "uptime",           SDF_RD|SDF_STATS, "0",          "Yuno living time"),
SDATA (DTP_INTEGER, "start_time",       SDF_RD|SDF_STATS,"0",           "Yuno starting time"),
SDATA (DTP_INTEGER, "timestamp",        SDF_PERSIST,    "0",            "Time of system, save in nvs each hour"),
SDATA (DTP_JSON,    "required_services",SDF_RD,         "{}",           "Required services"),
SDATA (DTP_INTEGER, "watcher_pid",      SDF_RD,         "0",            "Watcher pid"),

SDATA (DTP_INTEGER, "deep_trace",       SDF_WR|SDF_STATS,"0",           "Deep trace (in esp32 not persist)"),
SDATA (DTP_DICT,    "trace_levels",     SDF_PERSIST,    "{}",           "Trace levels"),
SDATA (DTP_DICT,    "no_trace_levels",  SDF_PERSIST,    "{}",           "No trace levels"),
SDATA (DTP_INTEGER, "periodic",         SDF_RD,         "1000",         "Timeout periodic, in miliseconds"),
SDATA (DTP_INTEGER, "timeout_stats",    SDF_RD,         "1",            "timeout (seconds) for publishing stats"),
SDATA (DTP_INTEGER, "autokill",         SDF_RD,         "0",            "Timeout (>0) to autokill in seconds"),

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

PRIVATE hgclass __gclass__ = 0;
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

    /*--------------------------*
     *     Yuneta user
     *--------------------------*/
    BOOL is_yuneta = FALSE;
#ifdef __linux__
    struct passwd *pw = getpwuid(getuid());
    if(strcmp(pw->pw_name, "yuneta")==0) {
        gobj_write_str_attr(gobj, "__username__", "yuneta");
        is_yuneta = TRUE;
    } else {
        struct group *grp = getgrnam("yuneta");
        if(grp && grp->gr_mem) {
            char **gr_mem = grp->gr_mem;
            while(*gr_mem) {
                if(strcmp(*gr_mem, pw->pw_name)==0) {
                    gobj_write_str_attr(gobj, "__username__", "yuneta");
                    is_yuneta = TRUE;
                    break;
                }
                gr_mem++;
            }
        }
    }
#endif
#ifdef ESP_PLATFORM
    gobj_write_str_attr(gobj, "__username__", "yuneta");
    is_yuneta = TRUE;
#endif
    if(!is_yuneta) {
        gobj_log_error(gobj, LOG_OPT_EXIT_ZERO,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "User or group 'yuneta' is needed to run a yuno",
            NULL
        );
        printf("User or group 'yuneta' is needed to run a yuno\n");
        exit(0);
    }

    /*------------------------*
     *  Traces
     *------------------------*/
    set_user_gclass_traces(gobj);
    set_user_gclass_no_traces(gobj);
    set_user_trace_filter(gobj);
    if(gobj_read_integer_attr(gobj, "deep_trace")) {
        gobj_set_deep_tracing((int)gobj_read_integer_attr(gobj, "deep_trace"));
    }

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

    /*
     *  Set user traces
     *  Here set gclass/gobj traces
     */
    set_user_gobj_traces(gobj);
    set_user_gobj_no_traces(gobj);

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
        gobj_gclass_register()
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
        gobj_service_register(gclass_name)
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

    json_t *jn_data = json_object();
    json_object_set_new(
        jn_data,
        gobj_short_name(gobj2read),
        gobj_read_attrs(gobj2read, SDF_PERSIST|SDF_RD|SDF_WR|SDF_STATS|SDF_RSTATS|SDF_PSTATS, gobj)
    );

    hgobj gobj_bottom = gobj_bottom_gobj(gobj2read);
    while(gobj_bottom) {
        json_object_set_new(
            jn_data,
            gobj_short_name(gobj_bottom),
            gobj_read_attrs(gobj_bottom, SDF_PERSIST|SDF_RD|SDF_WR|SDF_STATS|SDF_RSTATS|SDF_PSTATS, gobj)
        );
        gobj_bottom = gobj_bottom_gobj(gobj_bottom);
    }

    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        0,      // jn_comment
        0,      // jn_schema
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Show a gobj's attrs with detail
 ***************************************************************************/
PRIVATE json_t *cmd_attrs_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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
        get_attrs_schema(gobj2read)
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_view_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_data = json_object(); // NO CONFIG in esp32, yuneta_json_config();

    json_t *kw_response = build_command_response(
        gobj,
        0,          // result
        0,          // jn_comment
        0,          // jn_schema
        jn_data     // jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_view_mem(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_data = json_object();
#ifdef ESP_PLATFORM
    #include <esp_system.h>
    size_t size = esp_get_free_heap_size();
    json_object_set_new(jn_data, "HEAP free", json_integer(size));
#endif

    json_object_set_new(jn_data, "max_system_memory", json_integer((json_int_t)get_max_system_memory()));
    json_object_set_new(jn_data, "cur_system_memory", json_integer((json_int_t)get_cur_system_memory()));

    json_t *kw_response = build_command_response(
        gobj,
        0,          // result
        0,          // jn_comment
        0,          // jn_schema
        jn_data     // jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Show a gclass description
 ***************************************************************************/
PRIVATE json_t *cmd_view_gclass(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gclass_name_ = kw_get_str(
        gobj,
        kw,
        "gclass_name",
        kw_get_str(gobj, kw, "gclass", "", 0),
        0
    );

    hgclass gclass = 0;
    if(!empty_string(gclass_name_)) {
        gclass = gclass_find_by_name(gclass_name_);
        if(!gclass) {
            gclass = get_gclass_from_gobj(gclass_name_);
            if(!gclass) {
                json_t *kw_response = build_command_response(
                    gobj,
                    -1,     // result
                    json_sprintf("what gclass is '%s'?", gclass_name_),
                    0,      // jn_schema
                    0       // jn_data
                );
                JSON_DECREF(kw)
                return kw_response;
            }
        }
    }

    json_t *jn_data = gclass2json(gclass);

    json_t *kw_response = build_command_response(
        gobj,
        0,          // result
        0,          // jn_comment
        0,          // jn_schema
        jn_data     // jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Show a gobj
 ***************************************************************************/
PRIVATE json_t *cmd_view_gobj(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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

    json_t *jn_data = gobj2json(gobj2read, json_object());

    json_t *kw_response = build_command_response(
        gobj,
        0,          // result
        0,          // jn_comment
        0,          // jn_schema
        jn_data     // jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_view_gobj_tree(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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

    json_t *jn_options = kw_get_list(gobj, kw, "options", 0, 0);
    json_t *jn_data = view_gobj_tree(gobj2read, json_incref(jn_options));

    json_t *kw_response = build_command_response(
        gobj,
        0,          // result
        0,          // jn_comment
        0,          // jn_schema
        jn_data     // jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_enable_gobj(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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

    gobj_enable(gobj2read);

    json_t *kw_response = build_command_response(
        gobj,
        0,          // result
        json_sprintf("%s enabled.", gobj_short_name(gobj2read)),
        0,          // jn_schema
        0       // jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_disable_gobj(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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

    gobj_disable(gobj2read);

    json_t *kw_response = build_command_response(
        gobj,
        0,          // result
        json_sprintf("%s disabled.", gobj_short_name(gobj2read)),
        0,          // jn_schema
        0       // jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_persistent_attrs(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *gobj_name_ = kw_get_str( // __default_service__
        gobj,
        kw,
        "gobj_name",
        kw_get_str(gobj, kw, "gobj", "", 0),
        0
    );

    hgobj gobj2read = gobj_find_service(gobj_name_, FALSE);
    if(!gobj2read) {
        gobj2read = gobj_find_gobj(gobj_name_);
    }

    const char *attribute = kw_get_str(gobj, kw, "attribute", 0, 0);
    json_t *jn_attrs = attribute?json_string(attribute):0;

    /*
     *  Inform
     */
    json_t *jn_data = gobj_list_persistent_attrs(gobj2read, jn_attrs);
    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        0,      // jn_comment
        0,      // jn_schema
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_remove_persistent_attrs(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
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

    const char *attribute = kw_get_str(gobj, kw, "attribute", 0, 0);
    BOOL all = kw_get_bool(gobj, kw, "__all__", 0, KW_WILD_NUMBER);

    if(empty_string(attribute) && !all) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("what attribute?"),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    int ret = gobj_remove_persistent_attrs(gobj2read, all?NULL:json_string(attribute));

    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        ret,    // result
        0,      // jn_comment
        0,      // jn_schema
        gobj_list_persistent_attrs(gobj2read, 0) // owned
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Get the gclass of a gobj
 ***************************************************************************/
PRIVATE hgclass get_gclass_from_gobj(const char *gobj_name)
{
    hgobj gobj2view = gobj_find_service(gobj_name, FALSE);
    if(!gobj2view) {
        gobj2view = gobj_find_gobj(gobj_name);
        if(!gobj2view) {
            return 0;
        }
    }
    return gclass_find_by_name(gobj_gclass_name(gobj2view));
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_global_trace(
    hgobj gobj,
    const char *level,
    BOOL set,
    BOOL persistent
)
{
    json_t *jn_trace_levels = gobj_read_json_attr(gobj, "trace_levels");

    if(!kw_has_key(jn_trace_levels, "__global_trace__")) {
        /*
         *  Create new row, only if new_bitmask is != 0
         */
        if(set) {
            /*
             *  Create row
             */
            json_object_set_new(jn_trace_levels, "__global_trace__", json_array());
            json_t *jn_levels = json_object_get(jn_trace_levels, "__global_trace__");
            json_array_append_new(jn_levels, json_string(level));
        }
    } else {
        json_t *jn_levels = json_object_get(jn_trace_levels, "__global_trace__");
        if(!set) {
            /*
             *  Delete level
             */
            int idx = json_list_str_index(jn_levels, level, FALSE);
            if(idx != -1) {
                json_array_remove(jn_levels, idx);
            }
        } else {
            /*
             *  Add level
             */
            int idx = json_list_str_index(jn_levels, level, FALSE);
            if(idx == -1) {
                json_array_append_new(jn_levels, json_string(level));
            }
        }
    }

    if(persistent) {
        return gobj_save_persistent_attrs(gobj, json_string("trace_levels"));
    } else {
        return 0;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_trace_filter(hgobj gobj, hgclass gclass_)
{
    json_t *jn_trace_levels = gobj_read_json_attr(gobj, "trace_levels");
    json_t *jn_trace_filters = gobj_get_trace_filter(gclass_);

    if(jn_trace_filters) {
        kw_set_subdict_value(
            gobj,
            jn_trace_levels,
            "__trace_filter__",
            gclass_gclass_name(gclass_),
            json_incref(jn_trace_filters)
        );
    } else {
        kw_delete_subkey(
            gobj,
            jn_trace_levels,
            "__trace_filter__",
            gclass_gclass_name(gclass_)
        );
    }

    return gobj_save_persistent_attrs(gobj, json_string("trace_levels"));
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_user_trace(
    hgobj gobj,
    const char *name,
    const char *level,
    BOOL set,
    BOOL persistent
)
{
    json_t *jn_trace_levels = gobj_read_json_attr(gobj, "trace_levels");

    if(!kw_has_key(jn_trace_levels, name)) {
        /*
         *  Create new row, only if new_bitmask is != 0
         */
        if(set) {
            /*
             *  Create row
             */
            json_object_set_new(jn_trace_levels, name, json_array());
            json_t *jn_levels = json_object_get(jn_trace_levels, name);
            json_array_append_new(jn_levels, json_string(level));
        }
    } else {
        json_t *jn_levels = json_object_get(jn_trace_levels, name);
        if(!set) {
            /*
             *  Delete level
             */
            int idx = json_list_str_index(jn_levels, level, FALSE);
            if(idx != -1) {
                json_array_remove(jn_levels, idx);
            }
        } else {
            /*
             *  Add level
             */
            int idx = json_list_str_index(jn_levels, level, FALSE);
            if(idx == -1) {
                json_array_append_new(jn_levels, json_string(level));
            }
        }
    }

    if(persistent) {
        return gobj_save_persistent_attrs(gobj, json_string("trace_levels"));
    } else {
        return 0;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_user_no_trace(
    hgobj gobj,
    const char *name,
    const char *level,
    BOOL set,
    BOOL persistent
)
{
    json_t *jn_trace_no_levels = gobj_read_json_attr(gobj, "no_trace_levels");

    if(!kw_has_key(jn_trace_no_levels, name)) {
        /*
         *  Create new row, only if new_bitmask is != 0
         */
        if(set) {
            /*
             *  Create row
             */
            json_object_set_new(jn_trace_no_levels, name, json_array());
            json_t *jn_levels = json_object_get(jn_trace_no_levels, name);
            json_array_append_new(jn_levels, json_string(level));
        }
    } else {
        json_t *jn_no_levels = json_object_get(jn_trace_no_levels, name);
        if(!set) {
            /*
             *  Delete no-level
             */
            int idx = json_list_str_index(jn_no_levels, level, FALSE);
            if(idx != -1) {
                json_array_remove(jn_no_levels, idx);
            }
        } else {
            /*
             *  Add no-level
             */
            int idx = json_list_str_index(jn_no_levels, level, FALSE);
            if(idx == -1) {
                json_array_append_new(jn_no_levels, json_string(level));
            }
        }
    }

    if(persistent) {
        return gobj_save_persistent_attrs(gobj, json_string("no_trace_levels"));
    } else {
        return 0;
    }
}

/***************************************************************************
 *  Info of global gclass' trace levels
 ***************************************************************************/
PRIVATE json_t *cmd_info_global_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        0,      // jn_comment
        0,      // jn_schema
        gobj_repr_global_trace_levels()
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  View global gclass' trace levels
 ***************************************************************************/
PRIVATE json_t *cmd_get_global_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        0,      // jn_comment
        0,      // jn_schema
        gobj_get_global_trace_level()
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Set glocal trace level
 ***************************************************************************/
PRIVATE json_t *cmd_set_global_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *level = kw_get_str(gobj, kw, "level", 0, 0);
    if(empty_string(level)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: what level?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    const char *trace_value = kw_get_str(gobj, kw, "set", 0, 0);
    if(empty_string(trace_value)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: bitmask set or re-set?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    BOOL trace;
    if(strcasecmp(trace_value, "true")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "false")==0 || strcasecmp(trace_value, "reset")==0) {
        trace = 0;
    } else {
        trace = atoi(trace_value);
    }

    int ret = gobj_set_global_trace(level, trace);
    if(ret==0) {
        save_global_trace(gobj, level, trace?1:0, TRUE);
    }

    json_t *jn_data = gobj_get_global_trace_level();

    json_t *kw_response = build_command_response(
        gobj,
        ret,
        json_sprintf("%s", (ret<0)? gobj_log_last_message():""),
        0,      // jn_schema
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Info of gclass' trace levels
 ***************************************************************************/
PRIVATE json_t *cmd_info_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gclass_name_ = kw_get_str(
        gobj,
        kw,
        "gclass_name",
        kw_get_str(gobj, kw, "gclass", "", 0),
        0
    );
    if(!empty_string(gclass_name_)) {
        hgclass gclass_ = gclass_find_by_name(gclass_name_);
        if(!gclass_) {
            gclass_ = get_gclass_from_gobj(gclass_name_);
            if(!gclass_) {
                json_t *kw_response = build_command_response(
                    gobj,
                    -1,     // result
                    json_sprintf("what gclass is '%s'?", gclass_name_),
                    0,      // jn_schema
                    0       // jn_data
                );
                JSON_DECREF(kw)
                return kw_response;
            }
            gclass_name_ = gclass_gclass_name(gclass_);
        }
    }

    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        0,      // jn_comment
        0,      // jn_schema
        gobj_repr_gclass_trace_levels(gclass_name_)
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  View gclass trace
 ***************************************************************************/
PRIVATE json_t *cmd_get_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gclass_name_ = kw_get_str(
        gobj,
        kw,
        "gclass_name",
        kw_get_str(gobj, kw, "gclass", "", 0),
        0
    );

    hgclass gclass = 0;
    if(!empty_string(gclass_name_)) {
        gclass = gclass_find_by_name(gclass_name_);
        if(!gclass) {
            gclass = get_gclass_from_gobj(gclass_name_);
            if(!gclass) {
                json_t *kw_response = build_command_response(
                    gobj,
                    -1,     // result
                    json_sprintf("what gclass is '%s'?", gclass_name_),
                    0,      // jn_schema
                    0       // jn_data
                );
                JSON_DECREF(kw)
                return kw_response;
            }
        }
    }

    /*
     *  Inform
     */
    json_t *jn_data = gobj_get_gclass_trace_level_list(gclass);
    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        json_sprintf("%d gclass with some trace", (int)json_array_size(jn_data)),
        0,      // jn_schema
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  View gclass no-trace
 ***************************************************************************/
PRIVATE json_t *cmd_get_gclass_no_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gclass_name_ = kw_get_str(
        gobj,
        kw,
        "gclass_name",
        kw_get_str(gobj, kw, "gclass", "", 0),
        0
    );

    hgclass gclass = 0;
    if(!empty_string(gclass_name_)) {
        gclass = gclass_find_by_name(gclass_name_);
        if(!gclass) {
            gclass = get_gclass_from_gobj(gclass_name_);
            if(!gclass) {
                json_t *kw_response = build_command_response(
                    gobj,
                    -1,     // result
                    json_sprintf("what gclass is '%s'?", gclass_name_),
                    0,      // jn_schema
                    0       // jn_data
                );
                JSON_DECREF(kw)
                return kw_response;
            }
        }
    }

    /*
     *  Inform
     */
    json_t *jn_data = gobj_get_gclass_trace_no_level_list(gclass);
    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        json_sprintf("%d gclass with some no trace", (int)json_array_size(jn_data)),
        0,      // jn_schema
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Set gclass trace level
 ***************************************************************************/
PRIVATE json_t *cmd_set_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gclass_name_ = kw_get_str(
        gobj,
        kw,
        "gclass_name",
        kw_get_str(gobj, kw, "gclass", "", 0),
        0
    );
    hgclass gclass = gclass_find_by_name(gclass_name_);
    if(!gclass) {
        gclass = get_gclass_from_gobj(gclass_name_);
        if(!gclass) {
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("what gclass is '%s'?", gclass_name_),
                0,      // jn_schema
                0       // jn_data
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    const char *level = kw_get_str(gobj, kw, "level", 0, 0);
    if(empty_string(level)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: what level?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    const char *trace_value = kw_get_str(gobj, kw, "set", 0, 0);
    if(empty_string(trace_value)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: bitmask set or re-set?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    BOOL trace;
    if(strcasecmp(trace_value, "true")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "false")==0 || strcasecmp(trace_value, "reset")==0) {
        trace = 0;
    } else {
        trace = atoi(trace_value);
    }

    int ret = gobj_set_gclass_trace(gclass, level, trace);
    if(ret == 0 || trace == 0) {
        save_user_trace(gobj, gclass_name_, level, trace?1:0, TRUE);
    }

    json_t *jn_data = gobj_get_gclass_trace_level(gclass);

    json_t *kw_response = build_command_response(
        gobj,
        ret,
        json_sprintf("%s", (ret<0)? gobj_log_last_message():""),
        0,      // jn_schema
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Set no gclass trace level
 ***************************************************************************/
PRIVATE json_t *cmd_set_no_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gclass_name_ = kw_get_str(
        gobj,
        kw,
        "gclass_name",
        kw_get_str(gobj, kw, "gclass", "", 0),
        0
    );
    hgclass gclass = gclass_find_by_name(gclass_name_);
    if(!gclass) {
        gclass = get_gclass_from_gobj(gclass_name_);
        if(!gclass) {
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("what gclass is '%s'?", gclass_name_),
                0,      // jn_schema
                0       // jn_data
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    const char *level = kw_get_str(gobj, kw, "level", 0, 0);
    if(empty_string(level)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: what level?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    const char *trace_value = kw_get_str(gobj, kw, "set", 0, 0);
    if(empty_string(trace_value)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: bitmask set or re-set?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    BOOL trace;
    if(strcasecmp(trace_value, "true")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "false")==0 || strcasecmp(trace_value, "reset")==0) {
        trace = 0;
    } else {
        trace = atoi(trace_value);
    }

    int ret = gobj_set_gclass_no_trace(gclass, level, trace);
    if(ret == 0 || trace == 0) {
        save_user_no_trace(gobj, gclass_name_, level, trace?1:0, TRUE);
    }

    json_t *jn_data = gobj_get_gclass_trace_no_level(gclass);
    json_t *kw_response = build_command_response(
        gobj,
        ret,
        0,
        0,
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  View gobj trace
 ***************************************************************************/
PRIVATE json_t *cmd_get_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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

    /*
     *  Inform
     */
    json_t *jn_data = gobj_get_gobj_trace_level_tree(gobj2read);
    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        json_sprintf("%d gobjs with some trace", (int)json_array_size(jn_data)),
        0,      // jn_schema
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  View gobj no-trace
 ***************************************************************************/
PRIVATE json_t *cmd_get_gobj_no_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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

    /*
     *  Inform
     */
    json_t *jn_data = gobj_get_gobj_trace_no_level_tree(gobj2read);
    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        json_sprintf("%d gobjs with some no trace", (int)json_array_size(jn_data)),
        0,      // jn_schema
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Set gobj trace level
 ***************************************************************************/
PRIVATE json_t *cmd_set_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gobj_name_ = kw_get_str(
        gobj,
        kw,
        "gobj_name",
        kw_get_str(gobj, kw, "gobj", "", 0),
        0
    );
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

    const char *level = kw_get_str(gobj, kw, "level", 0, 0);
    if(empty_string(level)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: what level?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    const char *trace_value = kw_get_str(gobj, kw, "set", 0, 0);
    if(empty_string(trace_value)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: bitmask set or re-set?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    BOOL trace;
    if(strcasecmp(trace_value, "true")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "false")==0 || strcasecmp(trace_value, "reset")==0) {
        trace = 0;
    } else {
        trace = atoi(trace_value);
    }

    KW_INCREF(kw);
    int ret = gobj_set_gobj_trace(gobj2read, level, trace?1:0, kw);
    if(ret == 0 || trace == 0) {
        save_user_trace(gobj, gobj_name_, level, trace?1:0, TRUE);
    }

    json_t *jn_data = gobj_get_gobj_trace_level(gobj2read);
    json_t *kw_response = build_command_response(
        gobj,
        ret,
        json_sprintf("%s", (ret<0)? gobj_log_last_message():""),
        0,
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Set no gobj trace level
 ***************************************************************************/
PRIVATE json_t *cmd_set_no_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gobj_name_ = kw_get_str(
        gobj,
        kw,
        "gobj_name",
        kw_get_str(gobj, kw, "gobj", "", 0),
        0
    );
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

    const char *level = kw_get_str(gobj, kw, "level", 0, 0);
    if(empty_string(level)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: what level?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    const char *trace_value = kw_get_str(gobj, kw, "set", 0, 0);
    if(empty_string(trace_value)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: bitmask set or re-set?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    BOOL trace;
    if(strcasecmp(trace_value, "true")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "false")==0 || strcasecmp(trace_value, "reset")==0) {
        trace = 0;
    } else {
        trace = atoi(trace_value);
    }

    int ret = gobj_set_gobj_no_trace(gobj2read, level, trace);
    if(ret == 0 || trace == 0) {
        save_user_no_trace(gobj, gobj_name_, level, trace?1:0, TRUE);
    }

    json_t *jn_data = gobj_get_gobj_trace_no_level(gobj2read);
    json_t *kw_response = build_command_response(
        gobj,
        ret,
        json_sprintf("%s", (ret<0)? gobj_log_last_message():""),
        0,
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_trace_filter(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *attr = kw_get_str(gobj, kw, "attr", 0, 0);
    const char *value = kw_get_str(gobj, kw, "value", 0, 0);
    BOOL all = kw_get_bool(gobj, kw, "all", 0, KW_WILD_NUMBER);

    const char *trace_value = kw_get_str(gobj, kw, "set", 0, 0);
    if(empty_string(trace_value)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: set or re-set?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    BOOL set;
    if(strcasecmp(trace_value, "true")==0 || strcasecmp(trace_value, "set")==0) {
        set = 1;
    } else if(strcasecmp(trace_value, "false")==0 || strcasecmp(trace_value, "reset")==0) {
        set = 0;
    } else {
        set = atoi(trace_value);
    }

    const char *gclass_name_ = kw_get_str(
        gobj,
        kw,
        "gclass_name",
        kw_get_str(gobj, kw, "gclass", "", 0),
        0
    );
    hgclass gclass = gclass_find_by_name(gclass_name_);
    if(!gclass) {
        gclass = get_gclass_from_gobj(gclass_name_);
        if(!gclass) {
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("what gclass is '%s'?", gclass_name_),
                0,      // jn_schema
                0       // jn_data
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    if(empty_string(attr) && (set || !all)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,
            json_sprintf("what attr?"),
            0,
            0
        );
        JSON_DECREF(kw)
        return kw_response;
    }
    if(empty_string(value) && (set || !all)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,
            json_sprintf("what value?"),
            0,
            0
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    int ret;
    if(set) {
        ret = gobj_add_trace_filter(gclass, attr, value);
    } else {
        // If attr is empty then remove all filters, if value is empty then remove all values of attr
        ret = gobj_remove_trace_filter(gclass, attr, value);
    }
    if(ret == 0) {
        save_trace_filter(gobj, gclass);
    }

    json_t *jn_filters = gobj_get_trace_filter(gclass); // Return is not YOURS

    json_t *kw_response = build_command_response(
        gobj,
        ret,
        json_sprintf("%s", (ret<0)? gobj_log_last_message():""),
        0,      // jn_schema
        json_incref(jn_filters)
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_get_trace_filter(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gclass_name_ = kw_get_str(
        gobj,
        kw,
        "gclass_name",
        kw_get_str(gobj, kw, "gclass", "", 0),
        0
    );
    hgclass gclass = gclass_find_by_name(gclass_name_);
    if(!gclass) {
        gclass = get_gclass_from_gobj(gclass_name_);
        if(!gclass) {
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("what gclass is '%s'?", gclass_name_),
                0,      // jn_schema
                0       // jn_data
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    json_t *jn_filters = gobj_get_trace_filter(gclass);

    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,      // jn_schema
        json_incref(jn_filters)
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  Reset all gclass or gobj trace levels
 ***************************************************************************/
PRIVATE json_t *cmd_reset_all_traces(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *gclass_name_ = kw_get_str(gobj, kw, "gclass", "", 0);
    const char *gobj_name_ = kw_get_str(gobj, kw, "gobj", "", 0);

    hgclass gclass = NULL;
    if(!empty_string(gclass_name_)) {
        gclass = gclass_find_by_name(gclass_name_);
        if(!gclass) {
            gclass = get_gclass_from_gobj(gclass_name_);
            if(!gclass) {
                json_t *kw_response = build_command_response(
                    gobj,
                    -1,     // result
                    json_sprintf("what gclass is '%s'?", gclass_name_),
                    0,      // jn_schema
                    0       // jn_data
                );
                JSON_DECREF(kw)
                return kw_response;
            }
        }

        json_t *levels = gobj_get_gclass_trace_level(gclass);
        size_t idx; json_t *jn_level;
        json_array_foreach(levels, idx, jn_level) {
            const char *level = json_string_value(jn_level);
            gobj_set_gclass_trace(gclass, level, 0);
            save_user_trace(gobj, gclass_name_, level, 0, FALSE);
        }
        json_decref(levels);

        gobj_save_persistent_attrs(gobj, json_string("trace_levels"));

        json_t *jn_data = gobj_get_gclass_trace_level(gclass);
        json_t *kw_response = build_command_response(
            gobj,
            0,
            0,
            0,
            jn_data
        );
        JSON_DECREF(kw);
        return kw_response;
    }

    if(!empty_string(gobj_name_)) {
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
        json_t *levels = gobj_get_gobj_trace_level(gobj2read);
        size_t idx; json_t *jn_level;
        json_array_foreach(levels, idx, jn_level) {
            const char *level = json_string_value(jn_level);
            gobj_set_gobj_trace(gobj2read, level, 0, 0);
            gobj_set_gclass_trace(gobj_gclass(gobj2read), level, 0);
            save_user_trace(gobj, gobj_name_, level, 0, FALSE);
        }
        json_decref(levels);

        gobj_save_persistent_attrs(gobj, json_string("trace_levels"));

        json_t *jn_data = gobj_get_gobj_trace_level(gobj2read);
        json_t *kw_response = build_command_response(
            gobj,
            0,
            0,
            0,
            jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    json_t *kw_response = build_command_response(
        gobj,
        -1,
        json_sprintf("What gclass or gobj?"),
        0,
        0
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_set_deep_trace(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *trace_value = kw_get_str(gobj, kw, "set", 0, 0);
    if(empty_string(trace_value)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf(
                "%s: bitmask set or re-set?", gobj_short_name(gobj)
            ),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    BOOL trace;
    if(strcasecmp(trace_value, "true")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "false")==0 || strcasecmp(trace_value, "reset")==0) {
        trace = 0;
    } else {
        trace = atoi(trace_value);
    }

    gobj_write_integer_attr(gobj, "deep_trace", trace);
    gobj_set_deep_tracing(trace);
    gobj_save_persistent_attrs(gobj, json_string("deep_trace"));

    json_t *kw_response = build_command_response(
        gobj,
        0,
        json_sprintf(
            "%s: daemon debug set to %d", gobj_short_name(gobj), trace
        ),
        0,
        0
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_reset_log_counters(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    gobj_log_clear_counters();
    json_t *kw_response = build_command_response(
        gobj,
        0,
        json_sprintf("%s: Log counters reset.", gobj_short_name(gobj)),
        0,
        0
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_view_log_counters(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_data = gobj_get_log_data();
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_add_log_handler(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *handler_name = kw_get_str(gobj, kw, "name", "", KW_REQUIRED);
    const char *handler_type = kw_get_str(gobj, kw, "type", "", KW_REQUIRED);
    const char *handler_options_ = kw_get_str(gobj, kw, "options", "", 0);
    log_handler_opt_t handler_options = atoi(handler_options_);
    if(!handler_options) {
        handler_options = LOG_OPT_ALL;
    }

    int added = 0;

    if(empty_string(handler_name)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,
            json_sprintf("What name?"),
            0,
            0
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    /*-------------------------------------*
     *      Check if already exists
     *-------------------------------------*/
    if(gobj_log_exist_handler(handler_name)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,
            json_sprintf("Handler already exists: %s", handler_name),
            0,
            0
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    if(strcmp(handler_type, "file")==0) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,
            json_sprintf("Handler 'file' type not allowed"),
            0,
            0
        );
        JSON_DECREF(kw)
        return kw_response;

    } else if(strcmp(handler_type, "udp")==0) {
        const char *url = kw_get_str(gobj, kw, "url", "", KW_REQUIRED);
        if(empty_string(url)) {
            json_t *kw_response = build_command_response(
                gobj,
                -1,
                json_sprintf("What url?"),
                0,
                0
            );
            JSON_DECREF(kw)
            return kw_response;
        }
        size_t bf_size = 0;                     // 0 = default 64K
        size_t udp_frame_size = 0;              // 0 = default 1500
        output_format_t output_format = 0;       // 0 = default OUTPUT_FORMAT_YUNETA
        const char *bindip = 0;

        KW_GET(bindip, bindip, kw_get_str)
        KW_GET(bf_size, bf_size, kw_get_int)
        KW_GET(udp_frame_size, udp_frame_size, kw_get_int)
        KW_GET(output_format, output_format, kw_get_int)

        udpc_t udpc = udpc_open(
            url,
            bindip,
            bf_size,
            udp_frame_size,
            output_format,
            TRUE    // exit on failure
        );
        if(udpc) {
            if(gobj_log_add_handler(handler_name, handler_type, handler_options, udpc)==0) {
                added++;
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "log_add_handler() FAILED",
                    "handler_type", "%s", handler_type,
                    "url",          "%s", url,
                    NULL
                );
            }
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "udpc_open() FAILED",
                "handler_type", "%s", handler_type,
                "url",          "%s", url,
                NULL
            );
        }
    } else {
        json_t *kw_response = build_command_response(
            gobj,
            -1,
            json_sprintf("Unknown '%s' handler type.", handler_type),
            0,
            0
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        added>0?0:-1,
        json_sprintf("%s: %d handlers added.", gobj_short_name(gobj), added),
        0,
        0
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_del_log_handler(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *handler_name = kw_get_str(gobj, kw, "name", "", KW_REQUIRED);
    if(empty_string(handler_name)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,
            json_sprintf("%s: what name?", gobj_short_name(gobj)),
            0,
            0
        );
        JSON_DECREF(kw)
        return kw_response;
    }
    int deletions = gobj_log_del_handler(handler_name);

    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        deletions>0?0:-1,
        json_sprintf("%s: %d handlers deleted.", gobj_short_name(gobj), deletions),
        0,
        0
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_list_log_handlers(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        gobj_log_list_handlers() // owned
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
        esp_restart();
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

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_user_gclass_traces(hgobj gobj)
{
    json_t *jn_trace_levels = gobj_read_json_attr(gobj, "trace_levels");
    if(!jn_trace_levels) {
        jn_trace_levels = json_object();
        gobj_write_json_attr(gobj, "trace_levels", jn_trace_levels);
        json_decref(jn_trace_levels);
    }

    json_t *jn_global = json_object_get(jn_trace_levels, "__global_trace__");
    if(jn_global) {
        size_t idx;
        json_t *jn_level;
        json_array_foreach(jn_global, idx, jn_level) {
            const char *level = json_string_value(jn_level);
            gobj_set_global_trace(level, TRUE);
        }
    }

    const char *key;
    json_t *jn_name;
    json_object_foreach(jn_trace_levels, key, jn_name) {
        const char *name = key;
        hgclass gclass = gclass_find_by_name(name);
        if(!gclass) {
            continue;
        }
        if(!json_is_array(jn_name)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "value MUST be a json list",
                "name",         "%s", name,
                NULL
            );
            continue;
        }

        size_t idx;
        json_t *jn_level;
        json_array_foreach(jn_name, idx, jn_level) {
            const char *level = json_string_value(jn_level);
            gobj_set_gclass_trace(gclass, level, TRUE);
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_user_gclass_no_traces(hgobj gobj)
{
    json_t *jn_trace_no_levels = gobj_read_json_attr(gobj, "no_trace_levels");
    if(!jn_trace_no_levels) {
        jn_trace_no_levels = json_object();
        gobj_write_json_attr(gobj, "no_trace_levels", jn_trace_no_levels);
        json_decref(jn_trace_no_levels);
    }
    const char *key;
    json_t *jn_name;
    json_object_foreach(jn_trace_no_levels, key, jn_name) {
        const char *name = key;
        hgclass gclass = gclass_find_by_name(name);
        if(!gclass) {
            continue;
        }
        if(!json_is_array(jn_name)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "value MUST be a json list",
                "name",         "%s", name,
                NULL
            );
            continue;
        }

        size_t idx;
        json_t *jn_level;
        json_array_foreach(jn_name, idx, jn_level) {
            const char *level = json_string_value(jn_level);
            gobj_set_gclass_no_trace(gclass, level, TRUE);
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_user_trace_filter(hgobj gobj)
{
    json_t *jn_trace_levels = gobj_read_json_attr(gobj, "trace_levels");
    if(!jn_trace_levels) {
        jn_trace_levels = json_object();
        gobj_write_json_attr(gobj, "trace_levels", jn_trace_levels);
        json_decref(jn_trace_levels);
    }

    json_t *jn_trace_filters = kw_get_dict(
        gobj,
        jn_trace_levels, "__trace_filter__", 0, 0
    );

    const char *key;
    json_t *jn_trace_filter;
    json_object_foreach(jn_trace_filters, key, jn_trace_filter) {
        const char *name = key;
        hgclass gclass = gclass_find_by_name(name);
        if(!gclass) {
            continue;
        }
        if(!json_is_object(jn_trace_filter)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "filter MUST be a json object",
                "name",         "%s", name,
                NULL
            );
            continue;
        }

        gobj_load_trace_filter(gclass, json_incref(jn_trace_filter));
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_user_gobj_traces(hgobj gobj)
{
    json_t *jn_trace_levels = gobj_read_json_attr(gobj, "trace_levels");
    if(!jn_trace_levels) {
        jn_trace_levels = json_object();
        gobj_write_json_attr(gobj, "trace_levels", jn_trace_levels);
        json_decref(jn_trace_levels);
    }

    json_t *jn_global = json_object_get(jn_trace_levels, "__global_trace__");
    if(jn_global) {
        size_t idx;
        json_t *jn_level;
        json_array_foreach(jn_global, idx, jn_level) {
            const char *level = json_string_value(jn_level);
            gobj_set_global_trace(level, TRUE);
        }
    }

    BOOL save = FALSE;
    const char *key;
    json_t *jn_name;
    void *n;
    json_object_foreach_safe(jn_trace_levels, n, key, jn_name) {
        const char *name = key;
        if(empty_string(name)) {
            continue;
        }
        if(strcmp(name, "__yuno__")==0) {
            continue;
        }
        if(strcmp(name, "__root__")==0) {
            continue;
        }
        if(strcmp(name, "__global_trace__")==0) {
            continue;
        }
        if(strcmp(name, "__trace_filter__")==0) {
            continue;
        }

        hgclass gclass = 0;
        hgobj namedgobj = 0;
        gclass = gclass_find_by_name(name);
        if(!gclass) { // Check gclass to check if no gclass and no gobj
            namedgobj = gobj_find_service(name, FALSE);
            if(!namedgobj) {
                char temp[80];
                snprintf(temp, sizeof(temp), "%s NOT FOUND: %s",
                    gclass?"named-gobj":"GClass",
                    name
                );
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", temp,
                    "name",         "%s", name,
                    NULL
                );
                json_object_del(jn_trace_levels, name);
                save = TRUE;
                continue;
            }
        }
        if(!namedgobj) {
            // Must be gclass, already set
            continue;
        }

        if(!json_is_array(jn_name)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "value MUST be a json list",
                "name",         "%s", name,
                NULL
            );
            continue;
        }

        size_t idx;
        json_t *jn_level;
        json_array_foreach(jn_name, idx, jn_level) {
            const char *level = json_string_value(jn_level);
            if(namedgobj) {
                gobj_set_gobj_trace(namedgobj, level, TRUE, 0); // Se pierden los "channel_name"!!!
            }
        }
    }

    if(save) {
        gobj_save_persistent_attrs(gobj, json_string("trace_levels"));
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_user_gobj_no_traces(hgobj gobj)
{
    json_t *jn_trace_no_levels = gobj_read_json_attr(gobj, "no_trace_levels");
    if(!jn_trace_no_levels) {
        jn_trace_no_levels = json_object();
        gobj_write_json_attr(gobj, "no_trace_levels", jn_trace_no_levels);
        json_decref(jn_trace_no_levels);
    }

    BOOL save = FALSE;
    const char *key;
    json_t *jn_name;
    void *n;
    json_object_foreach_safe(jn_trace_no_levels, n, key, jn_name) {
        const char *name = key;
        if(empty_string(name)) {
            continue;
        }
        if(strcmp(name, "__yuno__")==0) {
            continue;
        }
        if(strcmp(name, "__root__")==0) {
            continue;
        }
        if(strcmp(name, "__global_trace__")==0) {
            continue;
        }
        if(strcmp(name, "__trace_filter__")==0) {
            continue;
        }

        hgclass gclass = 0;
        hgobj namedgobj = 0;
        gclass = gclass_find_by_name(name);
        if(!gclass) { // Check gclass to check if no gclass and no gobj
            namedgobj = gobj_find_service(name, FALSE);
            if(!namedgobj) {
                char temp[80];
                snprintf(temp, sizeof(temp), "%s NOT FOUND: %s",
                    gclass?"named-gobj":"GClass",
                    name
                );
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", temp,
                    "name",         "%s", name,
                    NULL
                );
                json_object_del(jn_trace_no_levels, name);
                save = TRUE;
                continue;
            }
        }
        if(!namedgobj) {
            // Must be gclass, already set
            continue;
        }
        if(!json_is_array(jn_name)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "value MUST be a json list",
                "name",         "%s", name,
                NULL
            );
            continue;
        }

        size_t idx;
        json_t *jn_level;
        json_array_foreach(jn_name, idx, jn_level) {
            const char *level = json_string_value(jn_level);
            if(namedgobj) {
                gobj_set_gobj_no_trace(namedgobj, level, TRUE);
            }
        }
    }

    if(save) {
        gobj_save_persistent_attrs(gobj, json_string("no_trace_levels"));
    }

    return 0;
}




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
            abort();    // relaunch program
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
    if(__gclass__) {
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
    __gclass__ = gclass_create(
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
    if(!__gclass__) {
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
