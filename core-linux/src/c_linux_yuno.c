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
#include <libintl.h>
#include <pwd.h>
#include <grp.h>
#include <locale.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <gobj_environment.h>
#include <command_parser.h>
#include "yunetas_ev_loop.h"
#include "yunetas_environment.h"
#include "c_timer.h"
#include "c_linux_yuno.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void remove_pid_file(void);
PRIVATE int save_pid_in_file(hgobj gobj);

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_gclass_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_service_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_write_attr(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_attrs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_attrs_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_persistent_attrs(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t *cmd_remove_persistent_attrs(hgobj gobj, const char* cmd, json_t* kw, hgobj src);

PRIVATE json_t *cmd_info_global_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_global_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_info_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_gclass_no_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_info_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_gobj_no_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_set_trace_filter(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_trace_filter(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_set_global_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_no_gclass_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_reset_all_traces(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_no_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_deep_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE char pidfile[PATH_MAX] = {0};

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

SDATACM (DTP_SCHEMA,    "view-gclass-register",     0,      0,          cmd_view_gclass_register,"View gclass's register"),
SDATACM (DTP_SCHEMA,    "view-service-register",    a_services,pm_gclass_name,cmd_view_service_register,"View service's register"),

SDATACM (DTP_SCHEMA,    "write-attr",               0,      pm_wr_attr, cmd_write_attr, "Write a writable attribute)"),
SDATACM (DTP_SCHEMA,    "view-attrs",               a_read_attrs,pm_gobj_def_name, cmd_view_attrs,      "View gobj's attrs"),
SDATACM (DTP_SCHEMA,    "view-attrs-schema",        a_read_attrs2,pm_gobj_def_name, cmd_attrs_schema,    "View gobj's attrs schema"),

SDATACM (DTP_SCHEMA,    "list-persistent-attrs",    a_pers_attrs,pm_list_persistent_attrs,cmd_list_persistent_attrs,  "List persistent attributes of yuno"),
SDATACM (DTP_SCHEMA,    "remove-persistent-attrs",  0,      pm_remove_persistent_attrs,cmd_remove_persistent_attrs,  "List persistent attributes of yuno"),

SDATACM (DTP_SCHEMA,    "info-global-trace",        0,      0,              cmd_info_global_trace,      "Info of global trace levels"),
SDATACM (DTP_SCHEMA,    "get-global-trace",         0,      0,              cmd_get_global_trace,       "Get global trace levels"),
SDATACM (DTP_SCHEMA,    "set-global-trace",         0,      pm_set_global_tr,cmd_set_global_trace,      "Set global trace level"),

SDATACM (DTP_SCHEMA,    "info-gclass-trace",        0,      pm_gclass_name, cmd_info_gclass_trace,      "Info of class's trace levels"),
SDATACM (DTP_SCHEMA,    "get-gclass-trace",         0,      pm_gclass_name, cmd_get_gclass_trace,       "Get gclass' trace"),
SDATACM (DTP_SCHEMA,    "set-gclass-trace",         0,      pm_set_gclass_tr,cmd_set_gclass_trace,      "Set trace of a gclass)"),
SDATACM (DTP_SCHEMA,    "get-gclass-no-trace",      0,      pm_gclass_name, cmd_get_gclass_no_trace,    "Get no gclass' trace"),
SDATACM (DTP_SCHEMA,    "set-gclass-no-trace",      0,      pm_set_gclass_tr,cmd_set_no_gclass_trace,   "Set no-trace of a gclass)"),

SDATACM (DTP_SCHEMA,    "info-gobj-trace",          0,      pm_gobj_root_name, cmd_info_gobj_trace,      "Info gobj's trace"),
SDATACM (DTP_SCHEMA,    "get-gobj-trace",           0,      pm_gobj_root_name, cmd_get_gobj_trace,       "Get gobj's trace and his childs"),
SDATACM (DTP_SCHEMA,    "get-gobj-no-trace",        0,      pm_gobj_root_name, cmd_get_gobj_no_trace,    "Get no gobj's trace  and his childs"),
SDATACM (DTP_SCHEMA,    "set-gobj-trace",           0,      pm_set_gobj_tr, cmd_set_gobj_trace,         "Set trace of a named-gobj"),
SDATACM (DTP_SCHEMA,    "set-gobj-no-trace",        0,      pm_set_gobj_tr, cmd_set_no_gobj_trace,      "Set no-trace of a named-gobj"),

SDATACM (DTP_SCHEMA,    "set-trace-filter",         0,      pm_set_trace_filter, cmd_set_trace_filter,  "Set a gclass trace filter"),
SDATACM (DTP_SCHEMA,    "get-trace-filter",         0,      0, cmd_get_trace_filter, "Get trace filters"),

SDATACM (DTP_SCHEMA,    "reset-all-traces",         0,      pm_reset_all_tr, cmd_reset_all_traces,         "Reset all traces of a named-gobj of gclass"),
SDATACM (DTP_SCHEMA,    "set-deep-trace",           0,      pm_set_deep_trace,cmd_set_deep_trace,   "Set deep trace, all traces active"),

//SDATACM (DTP_SCHEMA,    "trunk-rotatory-file",      0,      0,              cmd_trunk_rotatory_file,    "Trunk rotatory files"),
//SDATACM (DTP_SCHEMA,    "reset-log-counters",       0,      0,              cmd_reset_log_counters,     "Reset log counters"),
//SDATACM (DTP_SCHEMA,    "add-log-handler",          0,      pm_add_log_handler,cmd_add_log_handler,     "Add log handler"),
//SDATACM (DTP_SCHEMA,    "delete-log-handler",       0,      pm_del_log_handler,cmd_del_log_handler,     "Delete log handler"),
//SDATACM (DTP_SCHEMA,    "list-log-handler",         0,      0,              cmd_list_log_handler,       "List log handlers"),

//SDATACM (DTP_SCHEMA,    "view-gclass",              0,      pm_gclass_name, cmd_view_gclass,            "View gclass description"),
//SDATACM (DTP_SCHEMA,    "view-gobj",                0,      pm_gobj_def_name, cmd_view_gobj,            "View gobj"),
//
//SDATACM (DTP_SCHEMA,    "public-attrs",             0,      pm_gclass_name, cmd_public_attrs,           "List public attrs of gclasses"),
//
//SDATACM (DTP_SCHEMA,    "webix-gobj-tree",          0,      pm_gobj_root_name,cmd_webix_gobj_tree,      "View webix style gobj tree"),
//SDATACM (DTP_SCHEMA,    "view-gobj-tree",           0,      pm_gobj_root_name,cmd_view_gobj_tree,       "View gobj tree"),
//SDATACM (DTP_SCHEMA,    "view-gobj-treedb",         0,      pm_gobj_root_name,cmd_view_gobj_treedb,     "View gobj treedb"),
//
//SDATACM (DTP_SCHEMA,    "list-childs",              0,      pm_list_childs, cmd_list_childs,            "List childs of the specified gclass"),
//SDATACM (DTP_SCHEMA,    "list-channels",            0,      pm_list_channels,cmd_list_channels,         "List channels (GUI usage)."),

//SDATACM (DTP_SCHEMA,    "authzs",                   0,      pm_authzs,  cmd_authzs,     "Authorization's help"),
//SDATACM (DTP_SCHEMA,    "view-config",              0,      0,          cmd_view_config,"View final json configuration"),


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

SDATA (DTP_STRING,  "work_dir",         SDF_RD,         "",             "Work dir"),
SDATA (DTP_STRING,  "domain_dir",       SDF_RD,         "",             "Domain dir"),
SDATA (DTP_STRING,  "bind_ip",          SDF_RD,         "",             "Bind ip of yuno's realm. Set by agent"),
SDATA (DTP_BOOLEAN, "yuno_multiple",    SDF_RD,         "0",            "True when yuno can open shared ports. Set by agent"),
SDATA (DTP_INTEGER, "launch_id",        SDF_RD,         "0",            "Launch Id. Set by agent"),
SDATA (DTP_STRING,  "start_date",       SDF_RD|SDF_STATS, "",           "Yuno starting date"),
SDATA (DTP_INTEGER, "uptime",           SDF_RD|SDF_STATS, "0",          "Yuno living time"),
SDATA (DTP_INTEGER, "start_time",       SDF_RD|SDF_STATS,"0",           "Yuno starting time"),

SDATA (DTP_JSON,    "tags",             SDF_RD,         "{}",           "tags"),
SDATA (DTP_JSON,    "required_services",SDF_RD,         "{}",           "Required services"),
SDATA (DTP_JSON,    "public_services",  SDF_RD,         "{}",           "Public services"),
SDATA (DTP_JSON,    "service_descriptor",SDF_RD,        "{}",           "Public service descriptor"),
SDATA (DTP_STRING,  "i18n_dirname",     SDF_RD,         "",             "dir_name parameter of bindtextdomain()"),
SDATA (DTP_STRING,  "i18n_domain",      SDF_RD,         "",             "domain_name parameter of bindtextdomain() and textdomain()"),
SDATA (DTP_STRING,  "i18n_codeset",     SDF_RD,         "UTF-8",        "codeset of textdomain"),
SDATA (DTP_INTEGER, "watcher_pid",      SDF_RD,         "0",            "Watcher pid"),
SDATA (DTP_JSON,    "allowed_ips",      SDF_PERSIST,    "{}",           "Allowed peer ip's if true, false not allowed"),
SDATA (DTP_JSON,    "denied_ips",       SDF_PERSIST,    "{}",           "Denied peer ip's if true, false not denied"),

SDATA (DTP_INTEGER, "deep_trace",       SDF_WR|SDF_STATS|SDF_PERSIST,"0", "Deep trace set or not set"),
SDATA (DTP_DICT,    "trace_levels",     SDF_PERSIST,    "{}",           "Trace levels"),
SDATA (DTP_DICT,    "no_trace_levels",  SDF_PERSIST,    "{}",           "No trace levels"),
SDATA (DTP_INTEGER, "periodic",         SDF_RD,         "1000",         "Timeout periodic, in miliseconds"),
SDATA (DTP_INTEGER, "timeout_stats",    SDF_RD,         "1",            "timeout (seconds) for publishing stats"),
SDATA (DTP_INTEGER, "timeout_flush",    SDF_RD,         "2",            "timeout (seconds) for rotatory flush"),
SDATA (DTP_INTEGER, "timeout_restart",  SDF_PERSIST,    "0",            "timeout (seconds) to restart"),
SDATA (DTP_INTEGER, "autokill",         SDF_RD,         "0",            "Timeout (>0) to autokill in seconds"),

SDATA (DTP_INTEGER, "io_uring_entries", SDF_RD,         "0",            "Entries for the SQ ring"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
enum {
    TRACE_XXXX          = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    //{"xxxx",             "Trace xxxx"},
    {0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
//SDATAAUTHZ (DTP_SCHEMA, "open-console",     0,      0,      0,      "Permission to open console"),
//SDATAAUTHZ (DTP_SCHEMA, "close-console",    0,      0,      0,      "Permission to close console"),
//SDATAAUTHZ (DTP_SCHEMA, "list-consoles",    0,      0,      0,      "Permission to list consoles"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_timer;
    yev_loop_t *yev_loop;

    size_t t_flush;
    size_t t_stats;
    size_t t_restart;
    int timeout_flush;
    int timeout_stats;
    int timeout_restart;
    int periodic;
    int autokill;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Create the event loop
     */
    yev_loop_create(
        gobj,
        (unsigned)gobj_read_integer_attr(gobj, "io_uring_entries"),
        &priv->yev_loop
    );

    if (!atexit_registered) {
        atexit(remove_pid_file);
        atexit_registered = 1;
    }

    time_t now;
    time(&now);
    gobj_write_integer_attr(gobj, "start_time", now);

    char bfdate[90];
    current_timestamp(bfdate, sizeof(bfdate));
    gobj_write_str_attr(gobj, "start_date", bfdate);

    gobj_log_warning(0, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "yuno start time",
        "t",            "%lld", (long long int)now,
        "start_date",   "%s", bfdate,
        NULL
    );

    json_t *attrs = gobj_hsdata(gobj);
    gobj_trace_json(gobj, attrs, "yuno's attrs");

    const char *i18n_domain = gobj_read_str_attr(gobj, "i18n_domain");
    if(!empty_string(i18n_domain)) {
        const char *i18n_dirname = gobj_read_str_attr(gobj, "i18n_dirname");
        if(!empty_string(i18n_dirname)) {
            bindtextdomain(i18n_domain, i18n_dirname);
        }
        textdomain(i18n_domain);
        const char *i18n_codeset = gobj_read_str_attr(gobj, "i18n_codeset");
        if(!empty_string(i18n_codeset)) {
            bind_textdomain_codeset(i18n_domain, i18n_codeset);
        }
    }

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
    // TODO   set_user_gclass_traces(gobj);
//    set_user_trace_filter(gobj);
//    set_user_gclass_no_traces(gobj);

    /*------------------------*
     *  Create childs
     *------------------------*/
    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    if(gobj_read_integer_attr(gobj, "launch_id")) {
        save_pid_in_file(gobj);
    }

    SET_PRIV(periodic,              (int)gobj_read_integer_attr)
    SET_PRIV(autokill,              (int)gobj_read_integer_attr)
    SET_PRIV(timeout_stats,         (int)gobj_read_integer_attr)
    SET_PRIV(timeout_flush,         (int)gobj_read_integer_attr)
    SET_PRIV(timeout_restart,       (int)gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(periodic,            (int)gobj_read_integer_attr)
        if(gobj_is_running(gobj)) {
            set_timeout_periodic(priv->gobj_timer, priv->periodic);
        }
    ELIF_EQ_SET_PRIV(autokill,          (int)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_stats,     (int)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_flush,     (int)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_restart,   (int)gobj_read_integer_attr)
        if(priv->timeout_restart > 0) {
            priv->t_restart = start_sectimer(priv->timeout_restart);
        } else {
            priv->t_restart = 0;
        }
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
    // TODO set_user_gobj_traces(gobj);
    //set_user_gobj_no_traces(gobj);

    gobj_start(priv->gobj_timer);

    set_timeout_periodic(priv->gobj_timer, priv->periodic);

    if(priv->timeout_flush > 0) {
        priv->t_flush = start_sectimer(priv->timeout_flush);
    }
    if(priv->timeout_stats > 0) {
        priv->t_stats = start_sectimer(priv->timeout_stats);
    }
    if(priv->timeout_restart > 0) {
        priv->t_restart = start_sectimer(priv->timeout_restart);
    }

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
    yev_loop_stop(priv->yev_loop);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

     yev_loop_destroy(priv->yev_loop);
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  This play order can come from yuneta_agent or autoplay config option or programmatic sentence
     */
    gobj_play(gobj_default_service());

    /*
     *  Run event loop
     */
    gobj_log_debug(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_STARTUP,
        "msg",              "%s", "Running main",
        "yuno_id",          "%s", gobj_read_str_attr(gobj, "yuno_id"),
        "yuno_name",        "%s", gobj_read_str_attr(gobj, "yuno_name"),
        "yuno_role",        "%s", gobj_read_str_attr(gobj, "yuno_role"),
        NULL
    );

    yev_loop_run(priv->yev_loop);   // Infinite loop while some handler is active

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    /*
     *  This play order can come from yuneta_agent or programmatic sentence
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
//    json_t *jn_trace_levels = gobj_read_json_attr(gobj, "trace_levels");
//    json_t *jn_trace_filters = gobj_get_trace_filter(gclass_);

// TODO   if(jn_trace_filters) {
//        kw_set_subdict_value(
//            jn_trace_levels, "__trace_filter__", gclass->gclass_name, json_incref(jn_trace_filters)
//        );
//    } else {
//        kw_delete_subkey(jn_trace_levels, "__trace_filter__", gclass->gclass_name);
//    }

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
    json_t *jn_no_trace_levels = gobj_read_json_attr(gobj, "no_trace_levels");

    if(!kw_has_key(jn_no_trace_levels, name)) {
        /*
         *  Create new row, only if new_bitmask is != 0
         */
        if(set) {
            /*
             *  Create row
             */
            json_object_set_new(jn_no_trace_levels, name, json_array());
            json_t *jn_levels = json_object_get(jn_no_trace_levels, name);
            json_array_append_new(jn_levels, json_string(level));
        }
    } else {
        json_t *jn_no_levels = json_object_get(jn_no_trace_levels, name);
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
    json_t *jn_data = gobj_get_gclass_no_trace_level_list(gclass);
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

    json_t *jn_data = gobj_get_gclass_no_trace_level(gclass);
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
 *  Info gobj trace
 ***************************************************************************/
PRIVATE json_t *cmd_info_gobj_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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
    json_t *kw_response = build_command_response(
        gobj,
        0,      // result
        0,
        0,      // jn_schema
        gobj_repr_gclass_trace_levels(gobj_gclass_name(gobj2read))
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
    json_t *jn_data = gobj_get_gobj_no_trace_level_tree(gobj2read);
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

    json_t *jn_data = gobj_get_gobj_no_trace_level(gobj2read);
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




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void remove_pid_file(void)
{
    if(!empty_string(pidfile)) {
        unlink(pidfile);
        pidfile[0] = 0;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_pid_in_file(hgobj gobj)
{
    /*
     *  Let it create the bin_path. Can exist some zombi yuno.
     */
    unsigned int pid = getpid();
    yuneta_bin_file(pidfile, sizeof(pidfile), "yuno.pid", TRUE);
    FILE *file = fopen(pidfile, "w");
    fprintf(file, "%d\n", pid);
    fclose(file);
    return 0;
}





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
            gobj_trace_msg(gobj, "❌❌❌❌ SHUTDOWN ❌❌❌❌");
            gobj_shutdown();
            JSON_DECREF(kw)
            return -1;
        }
    }

    if(priv->timeout_flush > 0 && test_sectimer(priv->t_flush)) {
        priv->t_flush = start_sectimer(priv->timeout_flush);
        // TODO rotatory_flush(0);
    }
    if(gobj_get_yuno_must_die()) {
        JSON_DECREF(kw);
        gobj_shutdown(); // provoca gobj_pause y gobj_stop del gobj yuno
        return 0;
    }
    if(priv->timeout_restart > 0 && test_sectimer(priv->t_restart)) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_STARTUP,
            "msg",          "%s", "Exit to restart",
            NULL
        );
        gobj_set_exit_code(-1);
        JSON_DECREF(kw);
        gobj_shutdown(); // provoca gobj_pause y gobj_stop del gobj yuno
        return 0;
    }

    if(priv->timeout_stats > 0 && test_sectimer(priv->t_stats)) {
        priv->t_stats = start_sectimer(priv->timeout_stats);
        // TODO load_stats(gobj);
    }


    // Let others uses the periodic timer, save resources
    gobj_publish_event(gobj, EV_TIMEOUT_PERIODIC, 0);

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
    ev_action_t st_idle[] = {
        {EV_TIMEOUT_PERIODIC,       ac_periodic_timeout,    0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
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
PUBLIC int register_c_linux_yuno(void)
{
    return create_gclass(C_YUNO);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void *yuno_event_loop(void)
{
    PRIVATE_DATA *priv;
    hgobj yuno = gobj_yuno();
    if(!yuno) {
        return 0;
    }
    priv = gobj_priv_data(yuno);

    return priv->yev_loop;
}
