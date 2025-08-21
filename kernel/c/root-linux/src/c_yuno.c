/****************************************************************************
 *          c_yuno.c
 *
 *          GClass __yuno__
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>
#include <libintl.h>
#include <pwd.h>
#include <errno.h>
#include <grp.h>
#include <locale.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <sched.h>
#include <sys/mman.h>
#include <sys/prctl.h>     // for PR_CAPBSET_READ
#include <linux/capability.h>  // for CAP_SYS_NICE, CAP_IPC_LOCK#include <sys/prctl.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <command_parser.h>
#include <log_udp_handler.h>
#include <yuneta_version.h>
#include <yev_loop.h>
#include <rotatory.h>
#include <tr_treedb.h>
#include "yunetas_environment.h"
#include "c_timer0.h"
#include "msg_ievent.h"
#include "c_yuno.h"


/***************************************************************
 *              Constants
 ***************************************************************/
#define KW_GET(__name__, __default__, __func__) \
    __name__ = __func__(gobj, kw, #__name__, __default__, 0);

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE json_t *get_cpus(void);
PRIVATE void boost_process_performance(int priority, int cpu_core);
PRIVATE unsigned int get_HZ(void);
PRIVATE void read_uptime(unsigned long long *uptime);
PRIVATE json_t *get_process_memory_info(void);
PRIVATE json_t *get_machine_memory_info(void);

PRIVATE int capture_signals(hgobj gobj);
PRIVATE hgclass get_gclass_from_gobj(const char *gobj_name);
PRIVATE void remove_pid_file(void);
PRIVATE int save_pid_in_file(hgobj gobj);

PRIVATE int set_user_gclass_traces(hgobj gobj);
PRIVATE int set_user_gclass_no_traces(hgobj gobj);
PRIVATE int set_user_trace_filter(hgobj gobj);
PRIVATE int set_user_gobj_traces(hgobj gobj);
PRIVATE int set_user_gobj_no_traces(hgobj gobj);
PRIVATE int set_limit_open_files(hgobj gobj, json_int_t limit_open_files);

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_gclass_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_service_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_write_attr(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_attrs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_attrs_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_info_mem(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
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
PRIVATE json_t *cmd_set_trace_machine_format(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_trunk_rotatory_file(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_reset_log_counters(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_log_counters(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t* cmd_add_log_handler(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_del_log_handler(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_list_log_handlers(hgobj gobj, const char* cmd, json_t* kw, hgobj src);

PRIVATE json_t *cmd_info_cpus(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_info_ifs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_info_os(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t* cmd_list_allowed_ips(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_add_allowed_ip(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_remove_allowed_ip(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_list_denied_ips(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_add_denied_ip(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_remove_denied_ip(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_system_topic_schema(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_global_variables(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_list_subscriptions(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_list_subscribings(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_list_gclass_commands(hgobj gobj, const char* cmd, json_t* kw, hgobj src);
PRIVATE json_t* cmd_list_gobj_commands(hgobj gobj, const char* cmd, json_t* kw, hgobj src);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE char pidfile[PATH_MAX] = {0};

PRIVATE const sdata_desc_t pm_gclass_name[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gclass_name",  0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "gclass",       0,              0,          "gclass-name"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_list_gclass_commands[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gclass_name",  0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "gclass",       0,              0,          "gclass-name"),
SDATAPM (DTP_INTEGER,   "details",      0,              0,          "1 show details"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_list_gobj_commands[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj"),
SDATAPM (DTP_STRING,    "gobj",         0,              "__default_service__", "named-gobj or full gobj"),
SDATAPM (DTP_INTEGER,   "details",      0,              0,          "0 show only names, 1 show details"),
SDATAPM (DTP_BOOLEAN,   "bottoms",      0,              0,          "TRUE show bottoms too"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_wr_attr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "__default_service__", "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "attribute",    0,              0,          "attribute name"),
SDATAPM (DTP_STRING,    "value",        0,              0,          "value"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_gobj_def_name[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "__default_service__", "named-gobj or full gobj name"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_list_persistent_attrs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj"),
SDATAPM (DTP_STRING,    "gobj",         0,              0,          "named-gobj or full gobj"),
SDATAPM (DTP_STRING,    "attribute",    0,              0,          "Attribute to list/remove"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_remove_persistent_attrs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj"),
SDATAPM (DTP_STRING,    "gobj",         0,              0,          "named-gobj or full gobj"),
SDATAPM (DTP_STRING,    "attribute",    0,              0,          "Attribute to list/remove"),
SDATAPM (DTP_BOOLEAN,   "__all__",      0,              0,          "Remove all persistent attrs"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_set_global_tr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "level",        0,              0,          "attribute name"),
SDATAPM (DTP_STRING,    "set",          0,              0,          "value"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_set_gclass_tr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gclass_name",  0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "gclass",       0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "level",        0,              0,          "attribute name"),
SDATAPM (DTP_STRING,    "set",          0,              0,          "value"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_reset_all_tr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj",         0,              "",         "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gclass",       0,              0,          "gclass-name"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_gobj_root_name[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "__yuno__", "named-gobj or full gobj name"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_gobj_tree[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "__yuno__", "named-gobj or full gobj name"),
SDATAPM (DTP_JSON,      "options",      0,              "[\"name\",\"state\",\"running\",\"playing\",\"service\",\"disabled\",\"volatil\",\"gobj_trace_level\",\"bottom_gobj\",\"commands\"]",       "json LIST with strings, empty all"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_set_gobj_tr[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "",         "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "level",        0,              0,          "attribute name"),
SDATAPM (DTP_STRING,    "set",          0,              0,          "value"),
SDATA_END()
};

PRIVATE const sdata_desc_t pm_set_trace_filter[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gclass_name",  0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "gclass",       0,              0,          "gclass-name"),
SDATAPM (DTP_STRING,    "attr",         0,              "",         "Attribute of gobj to filter"),
SDATAPM (DTP_STRING,    "value",        0,              "",         "Value of attribute to filer"),
SDATAPM (DTP_STRING,    "set",          0,              0,          "value"),
SDATAPM (DTP_BOOLEAN,   "all",          0,              0,          "Remove all filters"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_set_deep_trace[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "set",          0,              0,          "value"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_add_log_handler[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              0,          "Handler log name"),
SDATAPM (DTP_STRING,    "type",         0,              0,          "Handler log type"),
SDATAPM (DTP_STRING,    "options",      0,              0,          "Handler log options"),
SDATAPM (DTP_STRING,    "url",          0,              0,          "Url for log 'udp' type"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_del_log_handler[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              0,          "Handler name"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_add_allowed_ip[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "ip",           0,              "",         "ip"),
SDATAPM (DTP_BOOLEAN,   "allowed",      0,              0,          "TRUE allowed, FALSE denied"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_remove_allowed_ip[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "ip",           0,              "",         "ip"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_add_denied_ip[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "ip",           0,              "",         "ip"),
SDATAPM (DTP_BOOLEAN,   "denied",       0,              0,          "TRUE denied, FALSE denied"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_remove_denied_ip[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "ip",           0,              "",         "ip"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_list_subscriptions[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "gobj_name",    0,              0,          "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "gobj",         0,              "__default_service__", "named-gobj or full gobj name"),
SDATAPM (DTP_STRING,    "event",        0,              0,          "Event"),
SDATAPM (DTP_BOOLEAN,   "tree",         0,              0,          "TRUE: search subs in all below tree"),
SDATA_END()
};

PRIVATE const sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_services[] = {"services", "list-services", 0};
PRIVATE const char *a_read_attrs[] = {"read-attrs", 0};
PRIVATE const char *a_read_attrs2[] = {"read-attrs2", 0};
PRIVATE const char *a_pers_attrs[] = {"persistent-attrs", 0};

PRIVATE const sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------------------alias---items-------json_fn---------------------description*/
SDATACM (DTP_SCHEMA,    "help",                     a_help, pm_help,    cmd_help,                   "Command's help"),
SDATACM (DTP_SCHEMA,    "authzs",                   0,      pm_authzs,  cmd_authzs,                 "Authorization's help"),

/*-CMD2---type----------name------------------------flag---------alias---items-------json_fn-------------description--*/
SDATACM2(DTP_SCHEMA,    "info-cpus",                SDF_AUTHZ_X, 0,      0,          cmd_info_cpus,              "Info of cpus"),
SDATACM2(DTP_SCHEMA,    "info-ifs",                 SDF_AUTHZ_X, 0,      0,          cmd_info_ifs,               "Info of ifs"),
SDATACM2(DTP_SCHEMA,    "info-os",                  SDF_AUTHZ_X, 0,      0,          cmd_info_os,                "Info os"),
SDATACM2(DTP_SCHEMA,    "info-mem",                 SDF_AUTHZ_X, 0,      0,          cmd_info_mem,               "Info yuno memory and system"),
SDATACM2(DTP_SCHEMA,    "list-allowed-ips",         SDF_AUTHZ_X, 0,      0,          cmd_list_allowed_ips,       "List allowed ips"),
SDATACM2(DTP_SCHEMA,    "add-allowed-ip",           SDF_AUTHZ_X, 0,      pm_add_allowed_ip,  cmd_add_allowed_ip, "Add a ip to allowed list"),
SDATACM2(DTP_SCHEMA,    "remove-allowed-ip",        SDF_AUTHZ_X, 0,      pm_remove_allowed_ip, cmd_remove_allowed_ip, "Add a ip to allowed list"),
SDATACM2(DTP_SCHEMA,    "list-denied-ips",          SDF_AUTHZ_X, 0,      0,          cmd_list_denied_ips,        "List denied ips"),
SDATACM2(DTP_SCHEMA,    "add-denied-ip",            SDF_AUTHZ_X, 0,      pm_add_denied_ip, cmd_add_denied_ip,    "Add a ip to denied list"),
SDATACM2(DTP_SCHEMA,    "remove-denied-ip",         SDF_AUTHZ_X, 0,      pm_remove_denied_ip, cmd_remove_denied_ip, "Add a ip to denied list"),
SDATACM2(DTP_SCHEMA,    "system-schema",            SDF_AUTHZ_X, 0,      0, cmd_system_topic_schema,             "Get system topic schema"),
SDATACM2(DTP_SCHEMA,    "global-variables",         SDF_AUTHZ_X, 0,      0, cmd_global_variables,                "Get global variables"),

SDATACM2(DTP_SCHEMA,    "trunk-rotatory-file",      SDF_AUTHZ_X, 0,      0,          cmd_trunk_rotatory_file,    "Trunk rotatory files"),
SDATACM2(DTP_SCHEMA,    "reset-log-counters",       SDF_AUTHZ_X, 0,      0,          cmd_reset_log_counters,     "Reset log counters"),
SDATACM2(DTP_SCHEMA,    "view-log-counters",        SDF_AUTHZ_X, 0,      0,          cmd_view_log_counters,      "View log counters"),
SDATACM2(DTP_SCHEMA,    "add-log-handler",          SDF_AUTHZ_X, 0,      pm_add_log_handler,cmd_add_log_handler, "Add log handler"),
SDATACM2(DTP_SCHEMA,    "delete-log-handler",       SDF_AUTHZ_X, 0,      pm_del_log_handler,cmd_del_log_handler, "Delete log handler"),
SDATACM2(DTP_SCHEMA,    "list-log-handlers",        SDF_AUTHZ_X, 0,      0,          cmd_list_log_handlers,      "List log handlers"),

SDATACM2(DTP_SCHEMA,    "view-gclass-register",     SDF_AUTHZ_X, 0,      0,          cmd_view_gclass_register,   "View gclass's register"),
SDATACM2(DTP_SCHEMA,    "view-service-register",    SDF_AUTHZ_X, a_services,0,cmd_view_service_register,         "View service's register"),

SDATACM2(DTP_SCHEMA,    "write-attr",               SDF_AUTHZ_X, 0,      pm_wr_attr, cmd_write_attr,             "Write a writable attribute)"),
SDATACM2(DTP_SCHEMA,    "view-attrs",               SDF_AUTHZ_X, a_read_attrs,pm_gobj_def_name, cmd_view_attrs,  "View gobj's attrs"),
SDATACM2(DTP_SCHEMA,    "view-attrs-schema",        SDF_AUTHZ_X, a_read_attrs2,pm_gobj_def_name, cmd_attrs_schema,"View gobj's attrs schema"),

SDATACM2(DTP_SCHEMA,    "view-gclass",              SDF_AUTHZ_X, 0,      pm_gclass_name, cmd_view_gclass,        "View gclass description"),
SDATACM2(DTP_SCHEMA,    "view-gobj",                SDF_AUTHZ_X, 0,      pm_gobj_def_name, cmd_view_gobj,        "View gobj"),

SDATACM2(DTP_SCHEMA,    "view-gobj-tree",           SDF_AUTHZ_X, 0,      pm_gobj_tree,cmd_view_gobj_tree,        "View gobj tree"),

SDATACM2(DTP_SCHEMA,    "enable-gobj",              SDF_AUTHZ_X, 0,      pm_gobj_def_name,cmd_enable_gobj,       "Enable named-gobj, exec own mt_enable() or gobj_start_tree()"),
SDATACM2(DTP_SCHEMA,    "disable-gobj",             SDF_AUTHZ_X, 0,      pm_gobj_def_name,cmd_disable_gobj,      "Disable named-gobj, exec own mt_disable() or gobj_stop_tree()"),

SDATACM2(DTP_SCHEMA,    "list-persistent-attrs",    SDF_AUTHZ_X, a_pers_attrs,pm_list_persistent_attrs,cmd_list_persistent_attrs,  "List persistent attributes of yuno"),
SDATACM2(DTP_SCHEMA,    "remove-persistent-attrs",  SDF_AUTHZ_X, 0,      pm_remove_persistent_attrs,cmd_remove_persistent_attrs,  "List persistent attributes of yuno"),

SDATACM2(DTP_SCHEMA,    "list-subscriptions",       SDF_AUTHZ_X, 0,      pm_list_subscriptions,cmd_list_subscriptions,          "List subscriptions [of __default_service__]"),

SDATACM2(DTP_SCHEMA,    "list-subscribings",        SDF_AUTHZ_X, 0,      pm_list_subscriptions,cmd_list_subscribings,          "List subscribings [of __default_service__]"),

SDATACM2(DTP_SCHEMA,    "list-gclass-commands",     SDF_AUTHZ_X, 0,      pm_list_gclass_commands, cmd_list_gclass_commands,          "List commands of gclass's"),

SDATACM2(DTP_SCHEMA,    "list-gobj-commands",       SDF_AUTHZ_X, 0,      pm_list_gobj_commands, cmd_list_gobj_commands,          "List commands of gobj and bottoms"),

SDATACM2(DTP_SCHEMA,    "info-global-trace",        SDF_AUTHZ_X, 0,      0, cmd_info_global_trace,  "Info of global trace levels"),
SDATACM2(DTP_SCHEMA,    "info-gclass-trace",        SDF_AUTHZ_X, 0,      pm_gclass_name, cmd_info_gclass_trace,  "Info of class's trace levels"),

SDATACM2(DTP_SCHEMA,    "get-global-trace",         SDF_AUTHZ_X, 0,      0, cmd_get_global_trace,   "Get global trace levels"),
SDATACM2(DTP_SCHEMA,    "set-global-trace",         SDF_AUTHZ_X, 0,      pm_set_global_tr,cmd_set_global_trace,  "Set global trace level"),

SDATACM2(DTP_SCHEMA,    "get-gclass-trace",         SDF_AUTHZ_X, 0,      pm_gclass_name, cmd_get_gclass_trace,   "Get gclass' trace"),
SDATACM2(DTP_SCHEMA,    "set-gclass-trace",         SDF_AUTHZ_X, 0,      pm_set_gclass_tr,cmd_set_gclass_trace,  "Set trace of a gclass)"),
SDATACM2(DTP_SCHEMA,    "get-gclass-no-trace",      SDF_AUTHZ_X, 0,      pm_gclass_name, cmd_get_gclass_no_trace,"Get no gclass' trace"),
SDATACM2(DTP_SCHEMA,    "set-gclass-no-trace",      SDF_AUTHZ_X, 0,      pm_set_gclass_tr,cmd_set_no_gclass_trace,"Set no-trace of a gclass)"),

SDATACM2(DTP_SCHEMA,    "get-gobj-trace",           SDF_AUTHZ_X, 0,      pm_gobj_root_name, cmd_get_gobj_trace,   "Get gobj's trace and his children"),
SDATACM2(DTP_SCHEMA,    "set-gobj-trace",           SDF_AUTHZ_X, 0,      pm_set_gobj_tr, cmd_set_gobj_trace,      "Set trace of a named-gobj"),
SDATACM2(DTP_SCHEMA,    "get-gobj-no-trace",        SDF_AUTHZ_X, 0,      pm_gobj_root_name, cmd_get_gobj_no_trace,"Get no gobj's trace  and his children"),
SDATACM2(DTP_SCHEMA,    "set-gobj-no-trace",        SDF_AUTHZ_X, 0,      pm_set_gobj_tr, cmd_set_no_gobj_trace,   "Set no-trace of a named-gobj"),

SDATACM2(DTP_SCHEMA,    "set-trace-filter",         SDF_AUTHZ_X, 0,      pm_set_trace_filter, cmd_set_trace_filter,"Set a gclass trace filter"),
SDATACM2(DTP_SCHEMA,    "get-trace-filter",         SDF_AUTHZ_X, 0,      0, cmd_get_trace_filter, "Get trace filters"),

SDATACM2(DTP_SCHEMA,    "reset-all-traces",         SDF_AUTHZ_X, 0,      pm_reset_all_tr, cmd_reset_all_traces,    "Reset all traces of a named-gobj of gclass"),
SDATACM2(DTP_SCHEMA,    "set-deep-trace",           SDF_AUTHZ_X, 0,      pm_set_deep_trace,cmd_set_deep_trace,   "Set deep trace, all traces active"),
SDATACM2(DTP_SCHEMA,    "set-machine-format",       SDF_AUTHZ_X, 0,      pm_set_deep_trace,cmd_set_trace_machine_format,   "Set trace machine format, 0 legacy default, 1 simpler"),

SDATA_END()
};

/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t attrs_table[] = {
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
SDATA (DTP_STRING,  "yuno_role_plus_name",SDF_RD,       "",             "Yuno Role plus name"),

SDATA (DTP_STRING,  "yuno_version",     SDF_RD,         "",             "Yuno version (APP_VERSION)"),
SDATA (DTP_STRING,  "yuneta_version",   SDF_RD,         YUNETA_VERSION, "Yuneta version"),

SDATA (DTP_STRING,  "appName",          SDF_RD,         "",             "App name, must match the role"),
SDATA (DTP_STRING,  "appDesc",          SDF_RD,         "",             "App Description"),
SDATA (DTP_STRING,  "appDate",          SDF_RD,         "",             "App date/time"),

SDATA (DTP_STRING,  "work_dir",         SDF_RD,         "",             "Work dir"),
SDATA (DTP_STRING,  "domain_dir",       SDF_RD,         "",             "Domain dir"),
SDATA (DTP_STRING,  "bind_ip",          SDF_RD,         "",             "Bind ip of yuno's realm. Set by agent"),
SDATA (DTP_BOOLEAN, "yuno_multiple",    SDF_RD,         "0",            "True when yuno can open shared ports. Set by agent"),
SDATA (DTP_INTEGER, "keep_alive",       SDF_RD,         "60",           "Set keep-alive"),
SDATA (DTP_INTEGER, "launch_id",        SDF_RD,         "0",            "Launch Id. Set by agent"),
SDATA (DTP_STRING,  "start_date",       SDF_RD|SDF_STATS,"",            "Yuno starting date"),
SDATA (DTP_INTEGER, "start_time",       SDF_RD,         "0",            "Yuno starting time"),
SDATA (DTP_INTEGER, "uptime",           SDF_RD|SDF_STATS,"0",           "Yuno living time"),
SDATA (DTP_INTEGER, "cpu",              SDF_RD|SDF_STATS,"0",           "Cpu percent usage"),
SDATA (DTP_INTEGER, "disk_size_in_gigas",SDF_RD|SDF_STATS,"0",          "Disk size of /yuneta"),
SDATA (DTP_INTEGER, "disk_free_percent",SDF_RD|SDF_STATS, "0",          "Disk free of /yuneta"),

SDATA (DTP_LIST,    "tags",             SDF_RD,         "[]",           "tags"),
SDATA (DTP_LIST,    "required_services",SDF_RD,         "[]",           "Required services. Format: 'public_service_name[.yuno_name]'. TODO add alternative parameter: dict (jn_filter)"),
SDATA (DTP_LIST,    "public_services",  SDF_RD,         "[]",           "Public services"),
SDATA (DTP_DICT,    "service_descriptor",SDF_RD,        "{}",           "Public service descriptor"),
SDATA (DTP_STRING,  "i18n_dirname",     SDF_RD,         "",             "dir_name parameter of bindtextdomain()"),
SDATA (DTP_STRING,  "i18n_domain",      SDF_RD,         "",             "domain_name parameter of bindtextdomain() and textdomain()"),
SDATA (DTP_STRING,  "i18n_codeset",     SDF_RD,         "UTF-8",        "codeset of textdomain"),
SDATA (DTP_INTEGER, "watcher_pid",      SDF_RD,         "0",            "Watcher pid"),
SDATA (DTP_JSON,    "allowed_ips",      SDF_PERSIST,    "{}",           "Allowed peer ip's if TRUE, FALSE not allowed"),
SDATA (DTP_JSON,    "denied_ips",       SDF_PERSIST,    "{}",           "Denied peer ip's if TRUE, FALSE not denied"),


SDATA (DTP_INTEGER, "trace_machine_format", SDF_WR|SDF_PERSIST,"0",     "trace machine format, 0 legacy default, 1 simpler"),
SDATA (DTP_INTEGER, "deep_trace",       SDF_WR|SDF_PERSIST,"0", "Deep trace set or not set"),
SDATA (DTP_DICT,    "trace_levels",     SDF_PERSIST,    "{}",           "Trace levels"),
SDATA (DTP_DICT,    "no_trace_levels",  SDF_PERSIST,    "{}",           "No trace levels"),
SDATA (DTP_INTEGER, "timeout_periodic", SDF_RD,         "1000",         "Timeout periodic, in milliseconds. This periodic timeout feeds C_TIMER, the precision is important, but affect to performance, many C_TIMER gobjs will be consumed a lot of CPU"),
SDATA (DTP_INTEGER, "timeout_stats",    SDF_RD,         "1000",         "timeout (milliseconds) for publishing stats."),
SDATA (DTP_INTEGER, "timeout_flush",    SDF_RD,         "2000",         "timeout (milliseconds) for rotatory flush"),
SDATA (DTP_INTEGER, "timeout_restart",  SDF_PERSIST,    "0",            "timeout (seconds) to restart"),
SDATA (DTP_BOOLEAN, "autoplay",         SDF_RD,         "0",            "Auto play the yuno, don't use in yunos citizen, only in standalone or tests"),

SDATA (DTP_INTEGER, "io_uring_entries", SDF_RD,         "0",            "Entries for the SQ ring, multiply by 3 the maximum number of wanted connections. Default if 0 = 2400"),
SDATA (DTP_INTEGER, "limit_open_files", SDF_PERSIST,    "20000",        "Limit open files"),
SDATA (DTP_INTEGER, "limit_open_files_done", SDF_RD,    "",             "Limit open files done"),

SDATA (DTP_INTEGER, "cpu_core",         SDF_WR|SDF_PERSIST, "0",        "Cpu core, used if > 0"),
SDATA (DTP_INTEGER, "priority",         SDF_WR|SDF_PERSIST, "20",       "Process priority, BE CAREFUL"),

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
PRIVATE const sdata_desc_t authz_table[] = {
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
    yev_loop_h yev_loop;

    uint64_t last_cpu_ticks;
    uint64_t last_ms;

    uint64_t t_flush;
    uint64_t t_stats;
    time_t t_restart;
    json_int_t timeout_flush;
    json_int_t timeout_stats;
    json_int_t timeout_restart;
    json_int_t timeout_periodic;
    json_int_t limit_open_files;

    yev_event_h yev_signal;
} PRIVATE_DATA;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *  yev_loop callback
 ***************************************************************************/
PRIVATE int yev_loop_callback(yev_event_h yev_event)
{
    if (!yev_event) {
        /*
         *  It's the timeout
         */
        return 0;  // don't break the loop, c_yuno is not using yev_loop_run with timeout, by now
    }

    uint32_t trace_level = gobj_global_trace_level();
    if(trace_level & (TRACE_URING)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_debug(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "👁💥 yev callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "state",        "%s", yev_get_state_name(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "flag",         "%j", jn_flags,
            "fd",           "%d", yev_get_fd(yev_event),
            "gbuffer",      "%p", yev_get_gbuf(yev_event),
            "p",            "%p", yev_event,
            NULL
        );
        json_decref(jn_flags);
    }

    hgobj gobj = yev_get_gobj(yev_event);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_READ_TYPE:
            if(yev_event == priv->yev_signal) {
                if(yev_state == YEV_ST_IDLE) {
                    gbuffer_t *gbuf = yev_get_gbuf(yev_event);
                    size_t len = gbuffer_leftbytes(gbuf);
                    if(len == sizeof(struct signalfd_siginfo)) {
                        struct signalfd_siginfo *fdsi = gbuffer_cur_rd_pointer(gbuf);
                        if(trace_level & (TRACE_URING)) {
                            gobj_log_info(0, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_YEV_LOOP,
                                "msg",          "%s", "yev signal",
                                "msg2",         "%s", "👁💥 yev signal",
                                "signal",       "%d", fdsi->ssi_signo,
                                NULL
                            );
                        }

                        switch(fdsi->ssi_signo) {
                            case SIGALRM:
                            case SIGQUIT:
                            case SIGINT:
                                //quit_sighandler(fdsi.ssi_signo);
                            {
                                static int tries = 0;
                                tries++;
                                set_yuno_must_die();
                                if(tries > 1) {
                                    // exit with 0 to avoid the watcher to relaunch the daemon
                                    _exit(0);
                                }
                            }
                            break;

                            case SIGUSR1:
                                gobj_set_deep_tracing(-1);
                            break;

                            case SIGPIPE:
                            case SIGTERM:
                                // ignored
                            default:
                                // unexpected signal
                                break;
                        }

                    } else {
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                            "msg",          "%s", "len != sizeof(struct signalfd_siginfo)",
                            "len",          "%d", len,
                            NULL
                        );
                    }

                    /*
                     *  Clear buffer
                     *  Re-arm read
                     */
                    if(gbuf) {
                        gbuffer_clear(gbuf);
                        yev_start_event(yev_event);
                    }
                }

            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "what event?",
                    NULL
                );
            }
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char role_plus_name[NAME_MAX];
    if(empty_string(gobj_yuno_name())) {
        snprintf(role_plus_name, sizeof(role_plus_name),
            "%s",
            gobj_yuno_role()
        );
    } else {
        snprintf(role_plus_name, sizeof(role_plus_name),
            "%s^%s",
            gobj_yuno_role(),
            gobj_yuno_name()
        );
        gobj_write_str_attr(gobj, "yuno_role_plus_name", role_plus_name);
    }

    /*--------------------------*
     *  Create the event loop
     *--------------------------*/
    yev_loop_create(
        gobj,
        (unsigned)gobj_read_integer_attr(gobj, "io_uring_entries"),
        (int) gobj_read_integer_attr(gobj, "keep_alive"),
        yev_loop_callback,
        &priv->yev_loop
    );

    if (!atexit_registered) {
        atexit(remove_pid_file);
        atexit_registered = 1;
    }

    /*-----------------------------*
     *  Set priority and cpu core
     *-----------------------------*/
    int priority = (int)gobj_read_integer_attr(gobj, "priority");
    int cpu_core = (int)gobj_read_integer_attr(gobj, "cpu_core");
    if(cpu_core > 0) {
        json_t *jn_cpus = get_cpus();
        if(json_array_size(jn_cpus)>= cpu_core-1) {
            boost_process_performance(priority, cpu_core);
        }
        JSON_DECREF(jn_cpus)
    }

    /*--------------------------*
     *  Set limit open files
     *--------------------------*/
    json_int_t limit_open_files = gobj_read_integer_attr(gobj, "limit_open_files");
    set_limit_open_files(gobj, limit_open_files);

    /*--------------------------*
     *      Set start time
     *--------------------------*/
    time_t now;
    time(&now);
    gobj_write_integer_attr(gobj, "start_time", now);

    char bfdate[90];
    current_timestamp(bfdate, sizeof(bfdate));
    gobj_write_str_attr(gobj, "start_date", bfdate);

    /*--------------------------*
     *  Show attrs in start
     *--------------------------*/
    json_t *attrs = gobj_hsdata(gobj);
    gobj_trace_json(gobj, attrs, "yuno's attrs"); // Show attributes of yuno

    /*--------------------------*
     *      Set i18n
     *--------------------------*/
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
    set_user_gclass_traces(gobj);
    set_user_gclass_no_traces(gobj);
    set_user_trace_filter(gobj);
    if(gobj_read_integer_attr(gobj, "deep_trace")) {
        gobj_set_deep_tracing((int)gobj_read_integer_attr(gobj, "deep_trace"));
    }
    if(gobj_read_integer_attr(gobj, "trace_machine_format")) {
        gobj_set_trace_machine_format((int)gobj_read_integer_attr(gobj, "trace_machine_format"));
    }

    /*------------------------*
     *  Create children
     *------------------------*/
    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);

    if(gobj_read_integer_attr(gobj, "launch_id")) {
        save_pid_in_file(gobj);
    }

    /*-------------------------------*
     *  Capture the signals to quit
     *-------------------------------*/
    capture_signals(gobj);

    SET_PRIV(timeout_periodic,      gobj_read_integer_attr)
    SET_PRIV(timeout_stats,         gobj_read_integer_attr)
    SET_PRIV(timeout_flush,         gobj_read_integer_attr)
    SET_PRIV(timeout_restart,       gobj_read_integer_attr)
    SET_PRIV(limit_open_files,      gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_periodic,            gobj_read_integer_attr)
        if(gobj_is_running(gobj)) {
            set_timeout_periodic0(priv->gobj_timer, priv->timeout_periodic);
        }
    ELIF_EQ_SET_PRIV(timeout_stats,     gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_flush,     gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_restart,   gobj_read_integer_attr)
        if(priv->timeout_restart > 0) {
            priv->t_restart = start_sectimer(priv->timeout_restart);
        } else {
            priv->t_restart = 0;
        }
    ELIF_EQ_SET_PRIV(limit_open_files,  gobj_read_integer_attr)
        set_limit_open_files(gobj, priv->limit_open_files);
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

    gobj_start(priv->gobj_timer);

    set_timeout_periodic0(priv->gobj_timer, priv->timeout_periodic);

    if(priv->timeout_flush > 0) {
        priv->t_flush = start_msectimer(priv->timeout_flush);
    }
    if(priv->timeout_stats > 0) {
        priv->t_stats = start_msectimer(priv->timeout_stats);
    }
    if(priv->timeout_restart > 0) {
        priv->t_restart = start_sectimer(priv->timeout_restart);
    }

    yev_start_event(priv->yev_signal);

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
    yev_stop_event(priv->yev_signal);

    gobj_stop(priv->gobj_timer);
    gobj_stop_children(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    yev_destroy_event(priv->yev_signal);
    priv->yev_signal = 0;
    yev_loop_destroy(priv->yev_loop);
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    /*
     *  This play order can come from yuneta_agent or autoplay config option or programmatic sentence
     */
    if(!gobj_is_playing(gobj_default_service())) {
        gobj_play(gobj_default_service());
    }

    gobj_log_info(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_STARTUP,
        "msg",              "%s", "Playing yuno",
        "yuno_id",          "%s", gobj_read_str_attr(gobj, "yuno_id"),
        "yuno_name",        "%s", gobj_read_str_attr(gobj, "yuno_name"),
        "yuno_role",        "%s", gobj_read_str_attr(gobj, "yuno_role"),
        NULL
    );

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
    gobj_log_info(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_STARTUP,
        "msg",              "%s", "Pausing yuno",
        "yuno_id",          "%s", gobj_read_str_attr(gobj, "yuno_id"),
        "yuno_name",        "%s", gobj_read_str_attr(gobj, "yuno_name"),
        "yuno_role",        "%s", gobj_read_str_attr(gobj, "yuno_role"),
        NULL
    );
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
 *  Show register
 ***************************************************************************/
PRIVATE json_t *cmd_view_gclass_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_response = build_command_response(
        gobj,
        0,
        0,
        0,
        gclass_gclass_register()
    );
    JSON_DECREF(kw)
    return jn_response;
}

/***************************************************************************
 *  Show register
 ***************************************************************************/
static const json_desc_t services_desc[] = { // HACK must match with gobj_service_register()
// Name             Type        Defaults    Fillspace
{"service",         "string",   "",         "40"},  // First item is the pkey
{"gclass",          "string",   "",         "40"},
{"cmds",            "boolean",  "",         "10"},
{0}
};
PRIVATE json_t *cmd_view_service_register(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_schema = json_desc_to_schema(services_desc);
    json_t *jn_response = build_command_response(
        gobj,
        0,
        0,
        jn_schema,
        gobj_service_register()
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
                if(strcasecmp(svalue, "TRUE")==0) {
                    value = 1;
                } else if(strcasecmp(svalue, "FALSE")==0) {
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

        case DTP_REAL:
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
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *kw_response = build_command_response(
        gobj,
        -1,     // result
        json_sprintf("Not yet implemented"),   // jn_comment
        0,      // jn_schema
        0       // jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_info_mem(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_data = json_object();
#ifdef ESP_PLATFORM
    #include <esp_system.h>
    size_t size = esp_get_free_heap_size();
    json_add_integer(hgen, "HEAP free", size);
    json_object_set_new(jn_data, "HEAP free", json_integer(size));
#endif

    json_object_set_new(jn_data, "yuno max_system_memory", json_integer((json_int_t)get_max_system_memory()));
    json_object_set_new(jn_data, "yuno cur_system_memory", json_integer((json_int_t)get_cur_system_memory()));

    json_object_set_new(jn_data, "process_memory", get_process_memory_info());
    json_object_set_new(jn_data, "machine_memory", get_machine_memory_info());

    json_object_set_new(jn_data, "machine free mem in kb", json_integer((json_int_t)free_ram_in_kb()));

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
    if(empty_string(gclass_name_)) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("what gclass?"),
            0,      // jn_schema
            0       // jn_data
        );
        JSON_DECREF(kw)
        return kw_response;
    }

    hgclass gclass = 0;
    if(!empty_string(gclass_name_)) {
        gclass = gclass_find_by_name(gclass_name_);
        if(!gclass) {
            gclass = get_gclass_from_gobj(gclass_name_);
        }
    }

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
    json_t *jn_data = gobj_view_tree(gobj2read, json_incref(jn_options));

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
            if(json_size(jn_levels)==0) {
                json_object_del(jn_trace_levels, "__global_trace__");
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
    if(strcasecmp(trace_value, "TRUE")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "FALSE")==0 || strcasecmp(trace_value, "reset")==0) {
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
    if(strcasecmp(trace_value, "TRUE")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "FALSE")==0 || strcasecmp(trace_value, "reset")==0) {
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
    if(strcasecmp(trace_value, "TRUE")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "FALSE")==0 || strcasecmp(trace_value, "reset")==0) {
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
    if(strcasecmp(trace_value, "TRUE")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "FALSE")==0 || strcasecmp(trace_value, "reset")==0) {
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
    if(strcasecmp(trace_value, "TRUE")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "FALSE")==0 || strcasecmp(trace_value, "reset")==0) {
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
    if(strcasecmp(trace_value, "TRUE")==0 || strcasecmp(trace_value, "set")==0) {
        set = 1;
    } else if(strcasecmp(trace_value, "FALSE")==0 || strcasecmp(trace_value, "reset")==0) {
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
        JSON_DECREF(kw)
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
    if(strcasecmp(trace_value, "TRUE")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "FALSE")==0 || strcasecmp(trace_value, "reset")==0) {
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
PRIVATE json_t* cmd_set_trace_machine_format(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
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
    if(strcasecmp(trace_value, "TRUE")==0 || strcasecmp(trace_value, "set")==0) {
        trace = 1;
    } else if(strcasecmp(trace_value, "FALSE")==0 || strcasecmp(trace_value, "reset")==0) {
        trace = 0;
    } else {
        trace = atoi(trace_value);
    }

    gobj_write_integer_attr(gobj, "trace_machine_format", trace);
    gobj_set_trace_machine_format(trace);
    gobj_save_persistent_attrs(gobj, json_string("trace_machine_format"));

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
PRIVATE json_t *cmd_trunk_rotatory_file(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    rotatory_trunk(0); // WARNING trunk all files
    json_t *kw_response = build_command_response(
        gobj,
        0,
        json_sprintf("%s: Trunk all rotatory files done.", gobj_short_name(gobj)),
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
PRIVATE json_t *cmd_add_log_handler(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
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
            NULL,   // if_name,
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
PRIVATE json_t *cmd_del_log_handler(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
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
PRIVATE json_t *cmd_list_log_handlers(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
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

/***************************************************************************
 *  get_cpu_info()
 *
 *  Returns a json array with info about CPU cores.
 *
 *  Each object contains all fields from /proc/cpuinfo for the core.
 *
 *  Example:
 *  [
 *    {
 *      "processor": 0,
 *      "vendor_id": "GenuineIntel",
 *      "cpu family": "6",
 *      "model": "158",
 *      "model name": "Intel(R) Core(TM) i7-9700 CPU @ 3.00GHz",
 *      "stepping": "13",
 *      "microcode": "0xde",
 *      "cpu MHz": "3700.000",
 *      "cache size": "12288 KB",
 *      ...
 *    },
 *    ...
 *  ]
 ***************************************************************************/
PRIVATE json_t *cmd_info_cpus(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_list = get_cpus();
    json_t *kw_response = build_command_response(
        gobj,
        0,
        json_sprintf("%d cores", (int)json_array_size(jn_list)),
        0,
        jn_list
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  get_network_interfaces()
 *
 *  Returns a json array with info about network interfaces.
 *
 *  Example return:
 *  [
 *    {
 *      "name": "lo",
 *      "flags": 73,
 *      "addresses": [
 *        {
 *          "family": "AF_INET",
 *          "address": "127.0.0.1",
 *          "netmask": "255.0.0.0"
 *        },
 *        {
 *          "family": "AF_INET6",
 *          "address": "::1",
 *          "netmask": "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"
 *        }
 *      ]
 *    },
 *    ...
 *  ]
 ***************************************************************************/
PRIVATE json_t *cmd_info_ifs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    struct ifaddrs *ifaddr, *ifa;
    json_t *jn_list = json_array();

    if(getifaddrs(&ifaddr) == -1) {
        return jn_list;
    }

    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if(!ifa->ifa_name) {
            continue;
        }

        // Search for existing interface object
        json_t *jn_iface = NULL;
        size_t index;
        json_t *value;
        json_array_foreach(jn_list, index, value) {
            const char *name = json_string_value(json_object_get(value, "name"));
            if(name && strcmp(name, ifa->ifa_name) == 0) {
                jn_iface = value;
                break;
            }
        }

        // Create new interface object if not found
        if(!jn_iface) {
            jn_iface = json_object();
            json_object_set_new(jn_iface, "name", json_string(ifa->ifa_name));
            json_object_set_new(jn_iface, "flags", json_integer(ifa->ifa_flags));
            json_object_set_new(jn_iface, "addresses", json_array());
            json_array_append_new(jn_list, jn_iface);
        }

        // Skip if no address
        if(!ifa->ifa_addr) {
            continue;
        }

        char addr_buf[INET6_ADDRSTRLEN] = {0};
        char netmask_buf[INET6_ADDRSTRLEN] = {0};
        char broadcast_buf[INET6_ADDRSTRLEN] = {0};
        const char *family = NULL;

        if(ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), addr_buf, sizeof(addr_buf));
            family = "AF_INET";

            if(ifa->ifa_netmask) {
                struct sockaddr_in *nm = (struct sockaddr_in *)ifa->ifa_netmask;
                inet_ntop(AF_INET, &(nm->sin_addr), netmask_buf, sizeof(netmask_buf));
            }
            if(ifa->ifa_broadaddr && (ifa->ifa_flags & IFF_BROADCAST)) {
                struct sockaddr_in *ba = (struct sockaddr_in *)ifa->ifa_broadaddr;
                inet_ntop(AF_INET, &(ba->sin_addr), broadcast_buf, sizeof(broadcast_buf));
            }
        } else if(ifa->ifa_addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            inet_ntop(AF_INET6, &(sa6->sin6_addr), addr_buf, sizeof(addr_buf));
            family = "AF_INET6";

            if(ifa->ifa_netmask) {
                struct sockaddr_in6 *nm6 = (struct sockaddr_in6 *)ifa->ifa_netmask;
                inet_ntop(AF_INET6, &(nm6->sin6_addr), netmask_buf, sizeof(netmask_buf));
            }
        } else {
            continue; // skip other families
        }

        // Add address info
        json_t *jn_addr = json_object();
        json_object_set_new(jn_addr, "family", json_string(family));
        json_object_set_new(jn_addr, "address", json_string(addr_buf));
        if(netmask_buf[0]) {
            json_object_set_new(jn_addr, "netmask", json_string(netmask_buf));
        }
        if(broadcast_buf[0]) {
            json_object_set_new(jn_addr, "broadcast", json_string(broadcast_buf));
        }

        json_array_append_new(json_object_get(jn_iface, "addresses"), jn_addr);
    }

    freeifaddrs(ifaddr);

    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        jn_list
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  OS's info
 ***************************************************************************/
PRIVATE json_t *cmd_info_os(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_data = json_object();

    struct utsname buf;
    if(uname(&buf) == 0) {
        json_object_set_new(jn_data, "sysname", json_string(buf.sysname));
        json_object_set_new(jn_data, "nodename", json_string(buf.nodename));
        json_object_set_new(jn_data, "release", json_string(buf.release));
        json_object_set_new(jn_data, "version", json_string(buf.version));
        json_object_set_new(jn_data, "machine", json_string(buf.machine));
    }

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
PRIVATE json_t* cmd_list_allowed_ips(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        json_incref(gobj_read_json_attr(gobj_yuno(), "allowed_ips"))
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_add_allowed_ip(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *ip = kw_get_str(gobj, kw, "ip", "", 0);
    BOOL allowed = kw_get_bool(gobj, kw, "allowed", 0, KW_WILD_NUMBER);

    if(empty_string(ip)) {
        JSON_DECREF(kw)
        return build_command_response(
            gobj,
            -1,     // result
            json_sprintf("What ip?"),
            0,      // jn_schema
            0       // jn_data
        );
    }
    if(!kw_has_key(kw, "allowed")) {
        JSON_DECREF(kw)
        return build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Allowed, TRUE or FALSE?"),
            0,      // jn_schema
            0       // jn_data
        );
    }

    add_allowed_ip(ip, allowed);

    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        json_incref(gobj_read_json_attr(gobj_yuno(), "allowed_ips"))
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_remove_allowed_ip(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *ip = kw_get_str(gobj, kw, "ip", "", 0);
    if(empty_string(ip)) {
        JSON_DECREF(kw)
        return build_command_response(
            gobj,
            -1,     // result
            json_sprintf("What ip?"),
            0,      // jn_schema
            0       // jn_data
        );
    }

    remove_allowed_ip(ip);

    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        json_incref(gobj_read_json_attr(gobj_yuno(), "allowed_ips"))
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_list_denied_ips(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        json_incref(gobj_read_json_attr(gobj_yuno(), "denied_ips"))
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_add_denied_ip(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *ip = kw_get_str(gobj, kw, "ip", "", 0);
    BOOL denied = kw_get_bool(gobj, kw, "denied", 0, KW_WILD_NUMBER);

    if(empty_string(ip)) {
        JSON_DECREF(kw)
        return build_command_response(
            gobj,
            -1,     // result
            json_sprintf("What ip?"),
            0,      // jn_schema
            0       // jn_data
        );
    }
    if(!kw_has_key(kw, "denied")) {
        JSON_DECREF(kw)
        return build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Denied, TRUE or FALSE?"),
            0,      // jn_schema
            0       // jn_data
        );
    }

    add_denied_ip(ip, denied);

    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        json_incref(gobj_read_json_attr(gobj_yuno(), "denied_ips"))
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_remove_denied_ip(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *ip = kw_get_str(gobj, kw, "ip", "", 0);
    if(empty_string(ip)) {
        JSON_DECREF(kw)
        return build_command_response(
            gobj,
            -1,     // result
            json_sprintf("What ip?"),
            0,      // jn_schema
            0       // jn_data
        );
    }

    remove_denied_ip(ip);

    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        json_incref(gobj_read_json_attr(gobj_yuno(), "denied_ips"))
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_system_topic_schema(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        _treedb_create_topic_cols_desc()
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_global_variables(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        0,
        gobj_global_variables()
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  subs schema
 ***************************************************************************/
static const json_desc_t subs_desc[] = {
// Name             Type        Defaults    Fillspace
{"event",           "string",   "",         "28"},  // First item is the pkey
{"publisher",       "string",   "",         "30"},
{"subscriber",      "string",   "",         "30"},
{"__service__",     "string",   "",         "20"},
{"__filter__",      "json",     "",         "10"},
{"__global__",      "json",     "",         "10"},
{"flag",            "int",      "",         "6"},
{"__local__",       "json",     "",         "10"},
{"__config__",      "json",     "",         "10"},
{"renamed_ev",      "string",   "",         "10"},
{0}
};

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int cb_list_subscriptions(hgobj child, void *user_data, void *user_data2, void *user_data3)
{
    json_t *jn_list = user_data;
    gobj_event_t event = user_data2;

    json_t *jn_data = gobj_list_subscriptions(child, event, NULL, NULL);
    if(json_array_size(jn_data)) {
        json_array_extend(jn_list, jn_data);
    }
    JSON_DECREF(jn_data)
    return 0;
}

PRIVATE json_t* cmd_list_subscriptions(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *gobj_name_ = kw_get_str( // __default_service__
        gobj,
        kw,
        "gobj_name",
        kw_get_str(gobj, kw, "gobj", "", 0),
        0
    );
    gobj_event_t event = NULL;
    json_t *kw_subs = NULL;
    hgobj subscriber = NULL;

    hgobj gobj2view = gobj_find_service(gobj_name_, FALSE);
    if(!gobj2view) {
        gobj2view = gobj_find_gobj(gobj_name_);
        if(!gobj2view) {
            json_t *kw_response = build_command_response(
                gobj,
                -1,
                json_sprintf(
                    "%s: gobj '%s' not found.", gobj_short_name(gobj), gobj_name_
                ),
                0,
                0
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    const char *event_name = kw_get_str(gobj, kw, "event", 0, 0); // from parameter, not a gobj_event_t
    if(!empty_string(event_name)) {
        event_type_t *event_type = gobj_find_event_type(event_name, 0, FALSE);
        if(event_type) {
            event = event_type->event_name;
        } else {
            json_t *kw_response = build_command_response(
                gobj,
                -1,
                json_sprintf(
                    "%s: event '%s' not found.", gobj_short_name(gobj), event_name
                ),
                0,
                NULL
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    json_t *jn_data = gobj_list_subscriptions(gobj2view, event, kw_subs, subscriber);

    BOOL tree = kw_get_bool(gobj, kw, "tree", 0, KW_WILD_NUMBER);
    if(tree) {
        gobj_walk_gobj_children_tree(
            gobj, WALK_TOP2BOTTOM, cb_list_subscriptions, jn_data, (void *)event, 0
        );
    }

    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        json_desc_to_schema(subs_desc),
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  list subscribings
 ***************************************************************************/
PRIVATE int cb_list_subscribings(hgobj child, void *user_data, void *user_data2, void *user_data3)
{
    json_t *jn_list = user_data;
    gobj_event_t event = user_data2;

    json_t *jn_data = gobj_list_subscribings(child, event, NULL, NULL);
    if(json_array_size(jn_data)) {
        json_array_extend(jn_list, jn_data);
    }
    JSON_DECREF(jn_data)
    return 0;
}

PRIVATE json_t* cmd_list_subscribings(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *gobj_name_ = kw_get_str( // __default_service__
        gobj,
        kw,
        "gobj_name",
        kw_get_str(gobj, kw, "gobj", "", 0),
        0
    );
    gobj_event_t event = NULL;
    json_t *kw_subs = NULL;
    hgobj subscriber = NULL;

    hgobj gobj2view = gobj_find_service(gobj_name_, FALSE);
    if(!gobj2view) {
        gobj2view = gobj_find_gobj(gobj_name_);
        if(!gobj2view) {
            json_t *kw_response = build_command_response(
                gobj,
                -1,
                json_sprintf(
                    "%s: gobj '%s' not found.", gobj_short_name(gobj), gobj_name_
                ),
                0,
                NULL
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    const char *event_name = kw_get_str(gobj, kw, "event", 0, 0); // from parameter, not a gobj_event_t
    if(!empty_string(event_name)) {
        event_type_t *event_type = gobj_find_event_type(event_name, 0, FALSE);
        if(event_type) {
            event = event_type->event_name;
        } else {
            json_t *kw_response = build_command_response(
                gobj,
                -1,
                json_sprintf(
                    "%s: event '%s' not found.", gobj_short_name(gobj), event_name
                ),
                0,
                0
            );
            JSON_DECREF(kw)
            return kw_response;
        }
    }

    json_t *jn_data = gobj_list_subscribings(gobj2view, event, kw_subs, subscriber);

    BOOL tree = kw_get_bool(gobj, kw, "tree", 0, KW_WILD_NUMBER);
    if(tree) {
        gobj_walk_gobj_children_tree(
            gobj, WALK_TOP2BOTTOM, cb_list_subscribings, jn_data, (void *)event, 0
        );
    }

    /*
     *  Inform
     */
    json_t *kw_response = build_command_response(
        gobj,
        0,
        0,
        json_desc_to_schema(subs_desc),
        jn_data
    );
    JSON_DECREF(kw)
    return kw_response;
}

/***************************************************************************
 *  list commands
 ***************************************************************************/
PRIVATE json_t *get_command_info(hgobj gobj, hgclass gclass, BOOL details)
{
    json_t *gclass_desc = gclass2json(gclass);

    json_t *jn_commands = kw_get_dict_value(gobj, gclass_desc, "commands", 0, KW_REQUIRED);
    if(details) {
        json_incref(jn_commands);
        JSON_DECREF(gclass_desc)
        return jn_commands;

    } else {
        json_t *jn_data = json_array();

        int idx2; json_t *jn_command;
        json_array_foreach(jn_commands, idx2, jn_command) {
            const char *command = kw_get_str(gobj, jn_command, "command", "", KW_REQUIRED);
            if(empty_string(command)) {
                continue;
            }
            json_array_append_new(
                jn_data,
                json_string(command)
            );
        }
        JSON_DECREF(gclass_desc)
        return jn_data;
    }
}

/***************************************************************************
 *  list commands
 ***************************************************************************/
PRIVATE json_t *cmd_list_gclass_commands(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    const char *gclass_name = kw_get_str(
        gobj,
        kw,
        "gclass_name",
        kw_get_str(gobj, kw, "gclass", "", 0),
        0
    );
    int details = (int)kw_get_int(gobj, kw, "details", 0, 0);

    json_t *jn_data = NULL;

    if(!empty_string(gclass_name)) {
        /*
         *  Explicit gclass
         */
        hgclass gclass = gclass_find_by_name(gclass_name);
        if(!gclass) {
            gclass = get_gclass_from_gobj(gclass_name);
            if(!gclass) {
                json_t *kw_response = build_command_response(
                    gobj,
                    -1,     // result
                    json_sprintf("what gclass is '%s'?", gclass_name),
                    0,      // jn_schema
                    0       // jn_data
                );
                JSON_DECREF(kw)
                return kw_response;
            }
        }
        json_t *jn_commands = get_command_info(gobj, gclass, details);
        jn_data = json_pack("{s:s, s:o}",
            "gclass", gclass_name,
            "commands", jn_commands
        );

    } else {
        /*
         *  All gclass
         */
        json_t *jn_gclasses = gclass_gclass_register();
        jn_data = json_array();

        int idx; json_t *jn_gclass;
        json_array_foreach(jn_gclasses, idx, jn_gclass) {
            gclass_name = kw_get_str(gobj,jn_gclass, "gclass_name", "", KW_REQUIRED);
            hgclass gclass = gclass_find_by_name(gclass_name);
            if(gclass) {
                json_t *jn_commands = get_command_info(gobj, gclass, details);
                if(json_array_size(jn_commands)) {
                    json_array_append_new(
                        jn_data,
                        json_pack("{s:s, s:o}",
                            "gclass", gclass_name,
                            "commands", jn_commands
                        )
                    );

                } else {
                    JSON_DECREF(jn_commands)
                }
            }
        }
        JSON_DECREF(jn_gclasses)
    }

    /*
     *  Inform
     */
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
 *  list commands
 ***************************************************************************/
PRIVATE json_t *cmd_list_gobj_commands(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    int details = (int)kw_get_int(gobj, kw, "details", 0, 0);
    BOOL bottoms = kw_get_bool(gobj, kw, "bottoms", 0, 0);

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

    hgobj gobj2cmd = gobj_find_service(gobj_name_, FALSE);
    if(!gobj2cmd) {
        gobj2cmd = gobj_find_gobj(gobj_name_);
        if (!gobj2cmd) {
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

    hgclass gclass = gobj_gclass(gobj2cmd);

    json_t *jn_data = json_array();

    json_t *jn_commands = get_command_info(gobj, gclass, details);
    json_array_append_new(
        jn_data,
        json_pack("{s:s, s:o}",
            "gclass", gclass_gclass_name(gclass),
            "commands", jn_commands
        )
    );

    if(bottoms) {
        hgobj gobj_bottom = gobj_bottom_gobj(gobj2cmd);
        while(gobj_bottom) {
            gclass = gobj_gclass(gobj_bottom);
            json_t *jn_commands2 = get_command_info(gobj, gclass, details);
            json_array_append_new(
                jn_data,
                json_pack("{s:s, s:o}",
                    "gclass", gclass_gclass_name(gclass),
                    "commands", jn_commands2
                )
            );

            gobj_bottom = gobj_bottom_gobj(gobj_bottom);
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
        jn_data
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
PRIVATE json_t *get_cpus(void)
{
    json_t *jn_list = json_array();

    FILE *fp = fopen("/proc/cpuinfo", "r");
    if(!fp) {
        return jn_list;
    }

    json_t *jn_cpu = NULL;

    char line[1024];
    while(fgets(line, sizeof(line), fp)) {
        // Blank line signals end of one processor block
        if(line[0] == '\n' || line[0] == '\0') {
            if(jn_cpu) {
                json_array_append_new(jn_list, jn_cpu);
                jn_cpu = NULL;
            }
            continue;
        }

        char *key = strtok(line, ":");
        char *value = strtok(NULL, "\n");

        if(!key || !value) {
            continue;
        }

        left_justify(key);
        left_justify(value);

        // If "processor", start a new object
        if(strcmp(key, "processor") == 0) {
            if(jn_cpu) {
                json_array_append_new(jn_list, jn_cpu);
            }
            jn_cpu = json_object();
            json_object_set_new(jn_cpu, "processor", json_integer((json_int_t)atoi(value)));
            continue;
        }

        // Add other fields as strings
        if(jn_cpu) {
            json_object_set_new(jn_cpu, key, json_string(value));
        }
    }

    // In case last processor did not end with blank line
    if(jn_cpu) {
        json_array_append_new(jn_list, jn_cpu);
    }

    fclose(fp);

    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void boost_process_performance(int priority, int cpu_core)
{
    /*-----------------------------*
     * 1. Check CAP_SYS_NICE
     *-----------------------------*/
    if(prctl(PR_CAPBSET_READ, CAP_SYS_NICE) == 1) {
        struct sched_param param = { .sched_priority = priority };
        if(sched_setscheduler(0, SCHED_RR, &param) < 0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "sched_setscheduler() FAILED",
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
    } else {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Missing CAP_SYS_NICE (skipping sched_setscheduler)",
            NULL
        );
    }

    /*-----------------------------*
     * 2. Set CPU affinity
     *-----------------------------*/
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);

    if(sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) < 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "sched_setaffinity() FAILED",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }

    /*-----------------------------*
     * 3. Check CAP_IPC_LOCK
     *-----------------------------*/
    if(prctl(PR_CAPBSET_READ, CAP_IPC_LOCK) == 1) {
        if(mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "mlockall() FAILED",
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
    } else {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Missing CAP_IPC_LOCK (skipping mlockall)",
            NULL
        );
    }
}

/***************************************************************************
 * Read machine uptime, independently of the number of processors.
 *
 * OUT:
 * @uptime  Uptime value in jiffies.
 ***************************************************************************/
PRIVATE void read_uptime(unsigned long long *uptime)
{
    FILE *fp;
    char line[128];
    unsigned long up_sec, up_cent;

    if ((fp = fopen("/proc/uptime", "r")) == NULL)
        return;

    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return;
    }

    sscanf(line, "%lu.%lu", &up_sec, &up_cent);
    unsigned int HZ = get_HZ();
    *uptime = (unsigned long long) up_sec * HZ +
              (unsigned long long) up_cent * HZ / 100;

    fclose(fp);
}

/***************************************************************************
 * Get number of clock ticks per second.
 ***************************************************************************/
PRIVATE unsigned int get_HZ(void)
{
    long ticks = 0;
    unsigned int hz;

#if defined(__linux__)
    if ((ticks = sysconf(_SC_CLK_TCK)) == -1) {
        ticks = 0;
    }
#endif
    hz = (unsigned int) ticks;
    return hz;
}

/***************************************************************************
 *  get_process_memory_info()
 *
 *  Returns json object:
 *  {
 *    "VmSize": N,   // virtual size in KB
 *    "VmRSS":  N    // resident size in KB
 *  }
 ***************************************************************************/
PRIVATE json_t *get_process_memory_info(void)
{
    FILE *fp = fopen("/proc/self/status", "r");
    if(!fp) {
        return json_object();
    }

    json_t *jn_info = json_object();
    char line[512];

    while(fgets(line, sizeof(line), fp)) {
        if(strncmp(line, "VmSize:", 7) == 0) {
            left_justify(line+7);
            json_object_set_new(jn_info, "VmSize", json_string(line + 7));
        } else if(strncmp(line, "VmRSS:", 6) == 0) {
            left_justify(line+6);
            json_object_set_new(jn_info, "VmRSS", json_string(line + 6));
        }
    }

    fclose(fp);
    return jn_info;
}

/***************************************************************************
 *  get_machine_memory_info()
 *
 *  Returns json object:
 *  {
 *    "MemTotal":      N,   // total memory in KB
 *    "MemFree":       N,
 *    "MemAvailable":  N,
 *    "Buffers":       N,
 *    "Cached":        N,
 *    "SwapTotal":     N,
 *    "SwapFree":      N
 *  }
 ***************************************************************************/
PRIVATE json_t *get_machine_memory_info(void)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if(!fp) {
        return json_object();
    }

    json_t *jn_info = json_object();
    char line[512];

    while(fgets(line, sizeof(line), fp)) {
        char *p = strchr(line, ':');
        if(!p) {
            continue;
        }
        *p = 0;
        char *key = line;
        char *value = p+1;
        left_justify(key);
        left_justify(value);

        if(strcmp(key, "MemTotal") == 0) {
            json_object_set_new(jn_info, "MemTotal", json_string(value));
        } else if(strcmp(key, "MemFree") == 0) {
            json_object_set_new(jn_info, "MemFree", json_string(value));
        } else if(strcmp(key, "MemAvailable") == 0) {
            json_object_set_new(jn_info, "MemAvailable", json_string(value));
        } else if(strcmp(key, "Buffers") == 0) {
            json_object_set_new(jn_info, "Buffers", json_string(value));
        } else if(strcmp(key, "Cached") == 0) {
            json_object_set_new(jn_info, "Cached", json_string(value));
        } else if(strcmp(key, "SwapTotal") == 0) {
            json_object_set_new(jn_info, "SwapTotal", json_string(value));
        } else if(strcmp(key, "SwapFree") == 0) {
            json_object_set_new(jn_info, "SwapFree", json_string(value));
        }
    }

    fclose(fp);
    return jn_info;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int catch_signals(hgobj gobj)
{
    sigset_t mask;
    int sfd;

    // Block signals so they aren’t handled via default signal handlers
    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGUSR1);

    if(sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "sigprocmask() FAILED",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    // Create the signalfd
    sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if(sfd == -1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "signalfd() FAILED",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    // Now the caller should monitor `sfd` (e.g., with poll()/epoll()/io_uring)
    return sfd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int capture_signals(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int sfd = catch_signals(gobj);

    /*
     *  Alloc buffer to read
     */
    size_t len = sizeof(struct signalfd_siginfo); // signalfd_siginfo is 128 bytes
    gbuffer_t *gbuf = gbuffer_create(len, len);

    priv->yev_signal = yev_create_read_event(
        priv->yev_loop,
        yev_loop_callback,
        gobj,
        sfd,
        gbuf
    );

    return 0;
}

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

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_limit_open_files(hgobj gobj, json_int_t limit_open_files)
{
    struct rlimit rl;
    rl.rlim_cur = (rlim_t)limit_open_files;  // Set soft limit
    rl.rlim_max = (rlim_t)limit_open_files;  // Set hard limit
    if(setrlimit(RLIMIT_NOFILE, &rl)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "setrlimit() FAILED",
            "limit",        "%lu", (unsigned long)limit_open_files,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    }

    // Verify the new limit
    if(getrlimit(RLIMIT_NOFILE, &rl) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "getrlimit() FAILED",
            "limit",        "%lu", (unsigned long)limit_open_files,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    }

    gobj_write_integer_attr(gobj, "limit_open_files_done", (json_int_t)rl.rlim_cur);

    if(rl.rlim_cur != limit_open_files || rl.rlim_max != limit_open_files) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "setrlimit() limit not match ",
            "rlim_cur",     "%lu", (unsigned long)rl.rlim_cur,
            "rlim_max",     "%lu", (unsigned long)rl.rlim_max,
            "limit",        "%lu", (unsigned long)limit_open_files,
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void load_stats(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------*
     *      cpu
     *---------------------------------------*/
    {
        double cpu_percent = cpu_usage_percent(&priv->last_cpu_ticks, &priv->last_ms);
        gobj_write_integer_attr(gobj, "cpu", (uint32_t)cpu_percent);
    }

    /*---------------------------------------*
     *      uptime
     *---------------------------------------*/
    {
        unsigned long long uptime=0;
        read_uptime(&uptime);
        gobj_write_integer_attr(
            gobj,
            "uptime",
            (json_int_t)uptime
        );
    }

    /*---------------------------------------*
     *      Disk
     *---------------------------------------*/
    {
        struct statvfs st;

        if(statvfs("/yuneta", &st)==0) {
            int free_percent = (int)((st.f_bavail * 100)/st.f_blocks);
            uint64_t total_size = (uint64_t)st.f_frsize * st.f_blocks;
            total_size = total_size/(1024LL*1024LL*1024LL);
            gobj_write_integer_attr(
                gobj,
                "disk_size_in_gigas",
                (json_int_t)total_size
            );
            gobj_write_integer_attr(
                gobj,
                "disk_free_percent",
                free_percent
            );
        }
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_periodic(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->timeout_flush > 0 && test_msectimer(priv->t_flush)) {
        rotatory_flush(0);
        priv->t_flush = start_msectimer(priv->timeout_flush);
    }

    if(priv->timeout_restart > 0 && test_sectimer(priv->t_restart)) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_STARTUP,
            "msg",          "%s", "Exit to restart",
            NULL
        );
        rotatory_flush(0);
        gobj_set_exit_code(-1);
        JSON_DECREF(kw)
        yev_loop_reset_running(priv->yev_loop);
        return 0;
    }

    if(priv->timeout_stats > 0 && test_msectimer(priv->t_stats)) {
        load_stats(gobj);
        priv->t_stats = start_msectimer(priv->timeout_stats);
    }

    // Let others uses the periodic timer, WARNING: avoid a lot of C_TIMER's, bad performance!
    gobj_publish_event(gobj, EV_TIMEOUT_PERIODIC, json_incref(kw));

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  This event comes from timer0
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)

    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }

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
    static hgclass __gclass__ = 0;
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
        {EV_TIMEOUT_PERIODIC,       ac_timeout_periodic,    0},
        {EV_STOPPED,                ac_stopped,             0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT_PERIODIC,       EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_STOPPED,                0},
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
        attrs_table,
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
PUBLIC int register_c_yuno(void)
{
    return create_gclass(C_YUNO);
}

/***************************************************************************
 *  Return void * to hide #include <yev_loop.h> dependency
 ***************************************************************************/
PUBLIC void *yuno_event_loop(void)
{
    hgobj gobj = gobj_yuno();
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    return priv->yev_loop;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void set_yuno_must_die(void)
{
    hgobj gobj = gobj_yuno();
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", "Exit to die",
        "msg2",         "%s", "❌❌❌❌ Exit to die ❌❌❌❌",
        NULL
    );
    gobj_set_exit_code(0);
    rotatory_flush(0);
    yev_loop_reset_running(priv->yev_loop);
}

/***************************************************************************
 *  Is ip or peername allowed?
 *  IP's must be numeric
 ***************************************************************************/
PUBLIC BOOL is_ip_allowed(const char *peername)
{
    char ip[NAME_MAX];
    snprintf(ip, sizeof(ip), "%s", peername);
    char *p = strchr(ip, ':');
    if(p) {
        *p = 0;
    }
    json_t *b = json_object_get(gobj_read_json_attr(gobj_yuno(), "allowed_ips"), ip);
    return json_is_true(b)?TRUE:FALSE;
}

/***************************************************************************
 * allowed: TRUE to allow, FALSE to deny
 ***************************************************************************/
PUBLIC int add_allowed_ip(const char *ip, BOOL allowed)
{
    if(json_object_set_new(
        gobj_read_json_attr(gobj_yuno(), "allowed_ips"),
        ip,
        allowed?json_true(): json_false()
    )==0) {
        return gobj_save_persistent_attrs(gobj_yuno(), json_string("allowed_ips"));
    } else {
        return -1;
    }
}

/***************************************************************************
 *  Remove from interna list (dict)
 ***************************************************************************/
PUBLIC int remove_allowed_ip(const char *ip)
{
    if(json_object_del(gobj_read_json_attr(gobj_yuno(), "allowed_ips"), ip)==0) {
        return gobj_save_persistent_attrs(gobj_yuno(), json_string("allowed_ips"));
    } else {
        return -1;
    }
}

/***************************************************************************
 *  Is ip or peername denied?
 ***************************************************************************/
PUBLIC BOOL is_ip_denied(const char *peername)
{
    char ip[NAME_MAX];
    snprintf(ip, sizeof(ip), "%s", peername);
    char *p = strchr(ip, ':');
    if(p) {
        *p = 0;
    }
    json_t *b = json_object_get(gobj_read_json_attr(gobj_yuno(), "denied_ips"), ip);
    return json_is_true(b)?TRUE:FALSE;
}

/***************************************************************************
 * denied: TRUE to deny, FALSE to deny
 ***************************************************************************/
PUBLIC int add_denied_ip(const char *ip, BOOL denied)
{
    if(json_object_set_new(
        gobj_read_json_attr(gobj_yuno(), "denied_ips"),
        ip,
        denied?json_true(): json_false()
    )==0) {
        return gobj_save_persistent_attrs(gobj_yuno(), json_string("denied_ips"));
    } else {
        return -1;
    }
}

/***************************************************************************
 *  Remove from interna list (dict)
 ***************************************************************************/
PUBLIC int remove_denied_ip(const char *ip)
{
    if(json_object_del(gobj_read_json_attr(gobj_yuno(), "denied_ips"), ip)==0) {
        return gobj_save_persistent_attrs(gobj_yuno(), json_string("denied_ips"));
    } else {
        return -1;
    }
}
