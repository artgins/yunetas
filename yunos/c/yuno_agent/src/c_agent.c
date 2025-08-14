/***********************************************************************
 *          C_AGENT.C
 *          Agent GClass.
 *
 *          Yuneta Agent, the first authority of realms and yunos in a host
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <grp.h>
#include <errno.h>
#include <regex.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>

#include <c_pty.h>
#include "c_agent.h"
#include "treedb_schema_yuneta_agent.c"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SDATA_GET_ID(hs)  kw_get_str(gobj, (hs), "id", "", KW_REQUIRED)
#define SDATA_GET_STR(hs, field)  kw_get_str(gobj, (hs), (field), "", KW_REQUIRED)
#define SDATA_GET_INT(hs, field)  kw_get_int(gobj, (hs), (field), 0, KW_REQUIRED)
#define SDATA_GET_BOOL(hs, field)  kw_get_bool(gobj, (hs), (field), 0, KW_REQUIRED)
#define SDATA_GET_ITER(hs, field)  kw_get_list(gobj, (hs), (field), 0, KW_REQUIRED)
#define SDATA_GET_JSON(hs, field)  kw_get_dict_value(gobj, (hs), (field), 0, KW_REQUIRED)

#define SDATA_SET_STR(hs, key, value) json_object_set_new((hs), (key), json_string(value))
#define SDATA_SET_INT(hs, key, value) json_object_set_new((hs), (key), json_integer(value))
#define SDATA_SET_BOOL(hs, key, value) json_object_set_new((hs), (key), value?json_true():json_false())
#define SDATA_SET_JSON(hs, key, value) json_object_set((hs), (key), value)
#define SDATA_SET_JSON_NEW(hs, key, value) json_object_set_new((hs), (key), value)


/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int is_yuneta_agent(pid_t pid);
PRIVATE void remove_pid_file(void);
PRIVATE uint64_t long_reference(void);

PRIVATE char *yuneta_repos_yuno_dir(
    char *bf,
    int bfsize,
    json_t *jn_tags,  // not owned
    const char *yuno_role,
    const char *yuno_version,
    BOOL create
);
PRIVATE char *yuneta_repos_yuno_file(
    char* bf_,
    int bfsize,
    json_t* jn_tags, // not owned
    const char* yuno_role,
    const char* yuno_version,
    const char *filename,
    BOOL create
);
PRIVATE json_t *get_yuno_realm(hgobj gobj, json_t *yuno);
PRIVATE char * build_yuno_private_domain(hgobj gobj, json_t *yuno, char *bf, int bfsize);
PRIVATE int build_role_plus_name(char *bf, int bf_len, json_t *yuno);
PRIVATE int build_role_plus_id(char *bf, int bf_len, json_t *yuno);
PRIVATE char * build_yuno_bin_path(hgobj gobj, json_t *yuno, char *bf, int bfsize, BOOL create_dir);
PRIVATE char * build_yuno_log_path(hgobj gobj, json_t *yuno, char *bf, int bfsize, BOOL create_dir);
PRIVATE int run_yuno(
    hgobj gobj,
    json_t *yuno,
    hgobj src
);
PRIVATE int kill_yuno(
    hgobj gobj,
    json_t *yuno
);
PRIVATE int play_yuno(
    hgobj gobj,
    json_t *yuno,
    json_t *kw,
    hgobj src
);
PRIVATE int pause_yuno(
    hgobj gobj,
    json_t *yuno,
    json_t *kw,
    hgobj src
);
PRIVATE int trace_on_yuno(
    hgobj gobj,
    json_t *yuno,
    json_t *kw,
    hgobj src
);
PRIVATE int trace_off_yuno(
    hgobj gobj,
    json_t *yuno,
    json_t *kw,
    hgobj src
);
PRIVATE int command_to_yuno(
    hgobj gobj,
    json_t *yuno,
    const char* command,
    json_t* kw,
    hgobj src
);
PRIVATE int stats_to_yuno(
    hgobj gobj, json_t *yuno, const char* stats, json_t* kw, hgobj src
);
PRIVATE int authzs_to_yuno(
    json_t *yuno, json_t* kw, hgobj src
);
PRIVATE int audit_command_cb(const char *command, json_t *kw, void *user_data);

PRIVATE json_t *find_binary_version(
    hgobj gobj,
    const char *role,
    const char *version
);
PRIVATE json_t *find_configuration_version(
    hgobj gobj,
    const char *role,
    const char *name,
    const char *version
);
PRIVATE int build_release_name(char *bf, int bfsize, json_t *hs_binary, json_t *hs_config);

PRIVATE int register_public_services(
    hgobj gobj,
    json_t *yuno, // not owned
    json_t *hs_binary, // not owned
    json_t *hs_realm // not owned
);
PRIVATE int restart_nodes(hgobj gobj);

PRIVATE int add_console_in_input_gate(hgobj gobj, const char *name, hgobj src);
PRIVATE int add_console_route(
    hgobj gobj,
    const char *name,
    json_t *jn_console,
    hgobj src,
    json_t *kw
);
PRIVATE int remove_console_route(
    hgobj gobj,
    const char *name,
    const char *route_service,
    const char *route_child
);
PRIVATE int get_last_public_port(hgobj gobj);

/***************************************************************************
 *              Resources
 ***************************************************************************/


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE const char *pidfile = "/yuneta/realms/agent/yuneta_agent.pid";

// Deja que siga con insecure connection
PRIVATE char agent_filter_chain_config[]= "\
{                                           \n\
    'services': [                           \n\
        {                                   \n\
            'name': 'agent_client',         \n\
            'gclass': 'C_IEVENT_CLI',       \n\
            'as_service': true,             \n\
            'autostart': true,              \n\
            'kw': {                         \n\
                'remote_yuno_name': '',                 \n\
                'remote_yuno_role': 'yuneta_agent',     \n\
                'remote_yuno_service': 'agent',         \n\
                'extra_info': {                             \n\
                    'realm_id': '%s',                       \n\
                    'yuno_id': '%s'                         \n\
                }                                           \n\
            },                                          \n\
            'children': [                               \n\
                {                                       \n\
                    'name': 'agent_client',             \n\
                    'gclass': 'C_IOGATE',               \n\
                    'kw': {                             \n\
                    },                                  \n\
                    'children': [                           \n\
                        {                                   \n\
                            'name': 'agent_client',         \n\
                            'gclass': 'C_CHANNEL',          \n\
                            'kw': {                         \n\
                            },                              \n\
                            'children': [                   \n\
                                {                           \n\
                                    'name': 'agent_client',     \n\
                                    'gclass': 'C_WEBSOCKET',    \n\
                                    'kw': {                     \n\
                                        'url':'(^^__url__^^)'   \n\
                                    },                          \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'agent_client', \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]    \n\
                                }    \n\
                            ]    \n\
                        }    \n\
                    ]    \n\
                }    \n\
            ]    \n\
        }    \n\
    ]    \n\
}    \n\
";

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_consoles(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_open_console(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_close_console(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_run_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_kill_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_play_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_pause_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_enable_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_disable_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_trace_on_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_trace_off_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_dir_yuneta(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_dir_realms(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_dir_logs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_dir_local_data(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_dir_repos(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_dir_store(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_replicate_node(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_replicate_binaries(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_list_public_services(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_update_public_service(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_public_service(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_list_realms(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_check_realm(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_create_realm(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_update_realm(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_realm(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_list_binaries(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_install_binary(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_update_binary(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_binary(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_list_configs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_create_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_update_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_top_yunos(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_yunos(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_find_new_yunos(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_create_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_tag(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_multiple(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_command_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_stats_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_command_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_stats_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_okill(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_qkill(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_check_json(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_snap_content(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_snaps(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_shoot_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_activate_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_deactivate_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_realms_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_yunos_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_binaries_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_configs_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_public_services_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_ping(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_uuid(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help"),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_list_binaries[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "Version"),
SDATAPM (DTP_STRING,    "tags",         0,              0,          "tags"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_configs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "name",         0,              0,          "Name"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "Version"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_realms[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "realm_owner",  0,              0,          "Realm Owner"),
SDATAPM (DTP_STRING,    "realm_role",   0,              0,          "Realm Role"),
SDATAPM (DTP_STRING,    "realm_name",   0,              0,          "Realm Name"),
SDATAPM (DTP_STRING,    "realm_env",    0,              0,          "Environment"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_check_realm[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_create_realm[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "realm_owner",  0,              0,          "Realm Owner"),
SDATAPM (DTP_STRING,    "realm_role",   0,              0,          "Realm Role"),
SDATAPM (DTP_STRING,    "realm_name",   0,              0,          "Realm Name"),
SDATAPM (DTP_STRING,    "realm_env",    0,              0,          "Environment"),
SDATAPM (DTP_STRING,    "bind_ip",      0,              0,          "Ip to be bind by the Realm services"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_update_realm[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "bind_ip",      0,              0,          "Ip to be bind by the Realm"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_del_realm[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "realm version"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force delete"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_public_services[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_command_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "command",      0,              0,          "Command to be executed in agent"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service of agent where execute the command"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_stats_agent[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "stats",        0,              0,          "Statistic to be executed in agent"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service of agent where execute the statistic"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_running_keys[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_run_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_kill_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATAPM (DTP_BOOLEAN,   "app",          0,              0,          "Kill app yunos (id>=1000)"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "kill with SIGKILL instead of SIGQUIT"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_yunos[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATAPM (DTP_BOOLEAN,   "yuno_running", 0,              0,          "True if yuno is running"),
SDATAPM (DTP_BOOLEAN,   "yuno_playing", 0,              0,          "True if yuno is playing"),
SDATAPM (DTP_BOOLEAN,   "yuno_disabled",0,              0,          "True if yuno is disabled"),
SDATAPM (DTP_BOOLEAN,   "must_play",    0,              0,          "True if yuno must play"),
SDATAPM (DTP_STRING,    "role_version", 0,              0,          "Role version"),
SDATAPM (DTP_STRING,    "name_version", 0,              0,          "Name version"),
SDATAPM (DTP_BOOLEAN,   "traced",       0,              0,          "True if yuno is tracing"),
SDATAPM (DTP_BOOLEAN,   "yuno_multiple",0,              0,          "True if yuno can have multiple instances with same name"),
SDATAPM (DTP_BOOLEAN,   "global",       0,              0,          "Yuno with public service (False: bind to 127.0.0.1, True: bind to realm ip)"),
SDATAPM (DTP_BOOLEAN,   "webix",        0,              0,          "List in webix format [{id,value}]"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_play_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_pause_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_enable_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_disable_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_trace_on_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_trace_off_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_command_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "command",      0,              0,          "Command to be executed in matched yunos"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service of yuno where execute the command"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_stats_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "stats",        0,              0,          "Statistic to be executed in matched yunos"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service of yuno where execute the statistic"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service of yuno where search the permission"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_tag[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_STRING,    "tag",          0,              0,          "New Yuno Tag"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATAPM (DTP_BOOLEAN,   "yuno_running", 0,              0,          "Yuno running"),
SDATAPM (DTP_BOOLEAN,   "yuno_disabled",0,              0,          "Yuno disabled"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_multiple[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATAPM (DTP_BOOLEAN,   "yuno_multiple",0,              0,          "New multiple set"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATAPM (DTP_BOOLEAN,   "yuno_running", 0,              0,          "Yuno running"),
SDATAPM (DTP_BOOLEAN,   "yuno_disabled",0,              0,          "Yuno disabled"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_write_tty[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              0,          "Name of console"),
SDATAPM (DTP_STRING,    "content64",    0,              0,          "Content64 data to write to tty"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_read_json[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "filename",      0,              0,         "Filename to read"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_read_file[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "filename",      0,              0,         "Filename to read"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_read_binary_file[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "filename",      0,              0,         "Filename to read"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_dir[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "subdirectory", 0,              0,          "Subdirectory wanted"),
SDATAPM (DTP_STRING,    "match",        0,              0,          "Pattern to match"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_logs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_local_data[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id of yuno"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_replicate_node[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "url",          0,              0,          "Url of node where replicate/upgrade"),
SDATAPM (DTP_STRING,    "filename",     0,              0,          "Filename where save replicate/upgrade"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_replicate_binaries[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Binary role you want replicate"),
SDATAPM (DTP_STRING,    "url",          0,              0,          "Url of node where replicate binaries"),
SDATAPM (DTP_STRING,    "filename",     0,              0,          "Filename where save the replicate"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_update_service[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_JSON,      "__filter__",   0,              0,          "Filter to match records"),
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "binary version"),
SDATAPM (DTP_STRING,    "ip",           0,              0,          "Ip assigned"),
SDATAPM (DTP_INTEGER,   "port",         0,              0,          "Port assigned"),
SDATAPM (DTP_STRING,    "url",          0,              0,          "Url assigned"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_del_service[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "service version"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force delete"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_install_binary[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "content64",    0,              0,          "yuno binary content in base64. Use content64=$$(role) or content64=full-path"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_update_binary[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "content64",    0,              0,          "yuno binary content in base64. Use content64=$$(role) or content64=full-path"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_delete_binary[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "binary version"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force delete"),
SDATA_END()
};


PRIVATE sdata_desc_t pm_create_config[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "content64",    0,              0,          "Content in base64"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_edit_config[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "configuration version"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_view_config[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "configuration version"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_update_config[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "configuration version"),
SDATAPM (DTP_STRING,    "content64",    0,              0,          "content in base64"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_delete_config[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "configuration version"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force delete"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_find_new_yunos[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_BOOLEAN,   "create",       0,              0,          "Create new found yunos"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_create_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),

SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno role"),
SDATAPM (DTP_STRING,    "role_version", 0,              0,          "Role version"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno name"),
SDATAPM (DTP_STRING,    "name_version", 0,              0,          "Name version"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),

SDATAPM (DTP_BOOLEAN,   "yuno_disabled",0,              0,          "True if yuno is disabled"),
SDATAPM (DTP_BOOLEAN,   "must_play",    0,              0,          "True if yuno must play"),
SDATAPM (DTP_BOOLEAN,   "yuno_multiple",0,              0,          "True if yuno can have multiple instances with same name"),
SDATAPM (DTP_BOOLEAN,   "global",       0,              0,          "Yuno with public service (False: bind to 127.0.0.1, True: bind to realm ip)"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_delete_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno role"),
SDATAPM (DTP_STRING,    "role_version", 0,              0,          "Role version"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno name"),
SDATAPM (DTP_STRING,    "name_version", 0,              0,          "Name version"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno release"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force delete"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_check_json[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER, "max_refcount",   0,              0,          "Maximum refcount"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_snap_content[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "snap_id",      0,              0,          "Snap id"),
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_shoot_snap[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              0,          "Snap name"),
SDATAPM (DTP_STRING,    "description",  0,              0,          "Snap description"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_activate_snap[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              0,          "Snap name"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_open_console[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              "",         "Name of console"),
SDATAPM (DTP_STRING,    "process",      0,              "bash",     "Process to execute"),
SDATAPM (DTP_STRING,    "cwd",          0,              0,          "Current work directory"),
SDATAPM (DTP_BOOLEAN,   "hold_open",    0,              0,          "True to not close pty on client disconnection"),
SDATAPM (DTP_INTEGER,   "cx",           0,              "80",       "Columns"),
SDATAPM (DTP_INTEGER,   "cy",           0,              "24",       "Rows"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_close_console[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              "",         "Name of console"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force to close although hold_open TRUE"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_edit_config[] = {"EV_EDIT_CONFIG", 0};
PRIVATE const char *a_view_config[] = {"EV_VIEW_CONFIG", 0};
PRIVATE const char *a_edit_yuno_config[] = {"EV_EDIT_YUNO_CONFIG", 0};
PRIVATE const char *a_view_yuno_config[] = {"EV_VIEW_YUNO_CONFIG", 0};
PRIVATE const char *a_read_json[] = {"EV_READ_JSON", 0};
PRIVATE const char *a_read_file[] = {"EV_READ_FILE", 0};
PRIVATE const char *a_read_binary_file[] = {"EV_READ_BINARY_FILE", 0};
PRIVATE const char *a_read_running_keys[] = {"EV_READ_RUNNING_KEYS", 0};
PRIVATE const char *a_read_running_bin[] = {"EV_READ_RUNNING_BIN", 0};
PRIVATE const char *a_write_tty[] = {"EV_WRITE_TTY", 0};

PRIVATE const char *a_top_yunos[] = {"t", 0};

PRIVATE const char *a_list_yunos[] = {"1", 0};
PRIVATE const char *a_list_binaries[] = {"2", 0};
PRIVATE const char *a_list_configs[] = {"3", 0};
PRIVATE const char *a_list_realms[] = {"4", 0};
PRIVATE const char *a_list_public_services[] = {"5", 0};

PRIVATE const char *a_yunos_instances[] = {"11", 0};
PRIVATE const char *a_binaries_instances[] = {"22", 0};
PRIVATE const char *a_configs_instances[] = {"33", 0};
PRIVATE const char *a_realms_instances[] = {"44", 0};
PRIVATE const char *a_public_services_instances[] = {"55", 0};
PRIVATE const char *a_uuid[] = {"uuid", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD2--type-----------name----------------flag----------------alias---------------items-----------json_fn---------description---------- */
SDATACM2 (DTP_SCHEMA,   "help",             0,                  a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "authzs",           SDF_WILD_CMD,       0,                  pm_authzs,      cmd_authzs,     "Authorization's help"),

SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              "\nAgent\n-----------"),

// HACK DANGER backdoor, use Yuneta only in private networks, or public but encrypted and assured connections.
SDATACM2 (DTP_SCHEMA,    "list-consoles",   0,                  0,                  0,              cmd_list_consoles, "List consoles"),
SDATACM2 (DTP_SCHEMA,    "open-console",    0,                  0,                  pm_open_console,cmd_open_console, "Open console"),
SDATACM2 (DTP_SCHEMA,    "close-console",   0,                  0,                  pm_close_console,cmd_close_console,"Close console"),

SDATACM2 (DTP_SCHEMA,   "command-agent",    SDF_WILD_CMD,       0,                  pm_command_agent,cmd_command_agent,"Command to agent. WARNING: parameter's keys are not checked"),
SDATACM2 (DTP_SCHEMA,   "stats-agent",      SDF_WILD_CMD,       0,                  pm_stats_agent, cmd_stats_agent, "Get statistics of agent"),
SDATACM2 (DTP_SCHEMA,   "set-ordered-kill", 0,                  0,                  0,              cmd_set_okill,  "Kill yunos with SIGQUIT, ordered kill"),
SDATACM2 (DTP_SCHEMA,   "set-quick-kill",   0,                  0,                  0,              cmd_set_qkill,  "Kill yunos with SIGKILL, quick kill"),
SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              "\nYuneta tree\n-----------"),
SDATACM2 (DTP_SCHEMA,   "dir-logs",         0,                  0,                  pm_logs,        cmd_dir_logs,   "List log filenames of yuno"),
SDATACM2 (DTP_SCHEMA,   "dir-local-data",   0,                  0,                  pm_local_data,  cmd_dir_local_data, "List local data filenames of yuno"),
SDATACM2 (DTP_SCHEMA,   "dir-yuneta",       0,                  0,                  pm_dir,         cmd_dir_yuneta, "List /yuneta directory"),
SDATACM2 (DTP_SCHEMA,   "dir-realms",       0,                  0,                  pm_dir,         cmd_dir_realms, "List /yuneta/realms directory"),
SDATACM2 (DTP_SCHEMA,   "dir-repos",        0,                  0,                  pm_dir,         cmd_dir_repos,  "List /yuneta/repos directory"),
SDATACM2 (DTP_SCHEMA,   "dir-store",        0,                  0,                  pm_dir,         cmd_dir_store,  "List /yuneta/store directory"),
SDATACM2 (DTP_SCHEMA,   "write-tty",        0,                  a_write_tty,        pm_write_tty,   0,              "Write data to tty"),
SDATACM2 (DTP_SCHEMA,   "read-json",        0,                  a_read_json,        pm_read_json,   0,              "Read json file"),
SDATACM2 (DTP_SCHEMA,   "read-file",        0,                  a_read_file,        pm_read_file,   0,              "Read a text file"),
SDATACM2 (DTP_SCHEMA,   "read-binary-file", 0,                  a_read_binary_file, pm_read_binary_file, 0,         "Read a binary file (encoded in base64)"),
SDATACM2 (DTP_SCHEMA,   "running-keys",    0,                  a_read_running_keys,pm_running_keys,0,             "Read yuno running parameters"),
SDATACM2 (DTP_SCHEMA,   "running-bin",      0,                  a_read_running_bin, pm_running_keys,0,              "Read yuno running bin path"),
SDATACM2 (DTP_SCHEMA,   "check-json",       0,                  0,                  pm_check_json,  cmd_check_json, "Check json refcounts"),
SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              "\nDeploy\n------"),
SDATACM2 (DTP_SCHEMA,   "replicate-node",   0,                  0,                  pm_replicate_node, cmd_replicate_node, "Replicate realms' yunos in other node or in file"),
SDATACM2 (DTP_SCHEMA,   "upgrade-node",     0,                  0,                  pm_replicate_node, cmd_replicate_node, "Upgrade realms' yunos in other node or in file"),
SDATACM2 (DTP_SCHEMA,   "replicate-binaries", 0,                0,                  pm_replicate_binaries, cmd_replicate_binaries, "Replicate binaries in other node or in file"),
SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              ""),
SDATACM2 (DTP_SCHEMA,   "update-public-service", 0,             0,                  pm_update_service, cmd_update_public_service, "Update a public service"),
SDATACM2 (DTP_SCHEMA,   "delete-public-service", 0,             0,                  pm_del_service, cmd_delete_public_service, "Remove a public service"),
SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              ""),
SDATACM2 (DTP_SCHEMA,   "check-realm",      0,                  0,                  pm_check_realm,  cmd_check_realm,"Check if a realm exists"),
SDATACM2 (DTP_SCHEMA,   "create-realm",     0,                  0,                  pm_create_realm,  cmd_create_realm,"Create a new realm"),
SDATACM2 (DTP_SCHEMA,   "update-realm",     0,                  0,                  pm_update_realm,cmd_update_realm,"Update a realm"),
SDATACM2 (DTP_SCHEMA,   "delete-realm",     0,                  0,                  pm_del_realm,   cmd_delete_realm,"Remove a realm"),
SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              ""),
SDATACM2 (DTP_SCHEMA,   "install-binary",   0,                  0,                  pm_install_binary,cmd_install_binary, "Install yuno binary. Use 'content64=$$(role)' for YunetaS (has preference) and Yuneta"),
SDATACM2 (DTP_SCHEMA,   "update-binary",    0,                  0,                  pm_update_binary,cmd_update_binary, "Update yuno binary. WARNING: Don't use in production!"),
SDATACM2 (DTP_SCHEMA,   "delete-binary",    0,                  0,                  pm_delete_binary,cmd_delete_binary, "Delete binary"),
SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              ""),
SDATACM2 (DTP_SCHEMA,   "create-config",    0,                  0,                  pm_create_config,cmd_create_config, "Create configuration"),
SDATACM2 (DTP_SCHEMA,   "edit-config",      0,                  a_edit_config,      pm_edit_config, 0,              "Edit configuration"),
SDATACM2 (DTP_SCHEMA,   "view-config",      0,                  a_view_config,      pm_view_config, 0,              "View configuration"),
SDATACM2 (DTP_SCHEMA,   "update-config",    0,                  0,                  pm_update_config,cmd_update_config, "Update configuration"),
SDATACM2 (DTP_SCHEMA,   "delete-config",    0,                  0,                  pm_delete_config,cmd_delete_config, "Delete configuration"),
SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              ""),
SDATACM2 (DTP_SCHEMA,   "find-new-yunos",   0,                  0,                  pm_find_new_yunos,cmd_find_new_yunos, "Find new yunos"),
SDATACM2 (DTP_SCHEMA,   "create-yuno",      0,                  0,                  pm_create_yuno, cmd_create_yuno, "Create yuno"),
SDATACM2 (DTP_SCHEMA,   "delete-yuno",      0,                  0,                  pm_delete_yuno, cmd_delete_yuno, "Delete yuno"),
SDATACM2 (DTP_SCHEMA,   "set-tag",          0,                  0,                  pm_set_tag,   cmd_set_tag,  "Set yuno tag"),
SDATACM2 (DTP_SCHEMA,   "set-multiple",     0,                  0,                  pm_set_multiple,cmd_set_multiple,"Set yuno multiple"),
SDATACM2 (DTP_SCHEMA,   "edit-yuno-config", 0,                  a_edit_yuno_config, pm_list_yunos,  0,              "Edit yuno configuration"),
SDATACM2 (DTP_SCHEMA,   "view-yuno-config", 0,                  a_view_yuno_config, pm_list_yunos,  0,              "View yuno configuration"),

/*-CMD2--type-----------name----------------flag----------------alias---------------items-----------json_fn---------description---------- */
SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              "\nOperation\n---------"),
SDATACM2 (DTP_SCHEMA,   "top",              0,                  a_top_yunos,        pm_list_yunos,       cmd_top_yunos,  "List only enabled yunos"),

SDATACM2 (DTP_SCHEMA,   "list-yunos",       0,                  a_list_yunos,       pm_list_yunos,       cmd_list_yunos, "List all yunos"),
SDATACM2 (DTP_SCHEMA,   "list-binaries",    0,                  a_list_binaries,    pm_list_binaries,    cmd_list_binaries,"List binaries"),
SDATACM2 (DTP_SCHEMA,   "list-configs",     0,                  a_list_configs,     pm_list_configs,     cmd_list_configs,"List configurations"),
SDATACM2 (DTP_SCHEMA,   "list-realms",      0,                  a_list_realms,      pm_list_realms,      cmd_list_realms,"List realms"),
SDATACM2 (DTP_SCHEMA,   "list-public-services", 0,              a_list_public_services,pm_public_services, cmd_list_public_services,"List public services"),

SDATACM2 (DTP_SCHEMA,   "list-yunos-instances",0,               a_yunos_instances,  pm_list_yunos,       cmd_yunos_instances, "List yunos instances"),
SDATACM2 (DTP_SCHEMA,   "list-binaries-instances",0,            a_binaries_instances,pm_list_binaries,   cmd_binaries_instances,"List binaries instances"),
SDATACM2 (DTP_SCHEMA,   "list-configs-instances",0,             a_configs_instances,pm_list_configs,     cmd_configs_instances,"List configurations instances"),
SDATACM2 (DTP_SCHEMA,   "list-realms-instances",0,              a_realms_instances, pm_list_realms,      cmd_realms_instances,"List realms instances"),
SDATACM2 (DTP_SCHEMA,   "list-public-services-instances",0,     a_public_services_instances,pm_public_services, cmd_public_services_instances,"List public services instances"),

SDATACM2 (DTP_SCHEMA,   "snaps",            0,                  0,                  0,              cmd_list_snaps, "List snaps"),
SDATACM2 (DTP_SCHEMA,   "snap-content",     0,                  0,                  pm_snap_content,cmd_snap_content, "Show snap content"),
SDATACM2 (DTP_SCHEMA,   "shoot-snap",       0,                  0,                  pm_shoot_snap,  cmd_shoot_snap, "Shoot snap"),
SDATACM2 (DTP_SCHEMA,   "activate-snap",    0,                  0,                  pm_activate_snap,cmd_activate_snap,"Activate snap"),
SDATACM2 (DTP_SCHEMA,   "deactivate-snap",  0,                  0,                  0,              cmd_deactivate_snap,"De-Activate snap"),
SDATACM2 (DTP_SCHEMA,   "run-yuno",         0,                  0,                  pm_run_yuno,    cmd_run_yuno,   "Run yuno"),
SDATACM2 (DTP_SCHEMA,   "kill-yuno",        0,                  0,                  pm_kill_yuno,   cmd_kill_yuno,  "Kill yuno"),
SDATACM2 (DTP_SCHEMA,   "play-yuno",        0,                  0,                  pm_play_yuno,   cmd_play_yuno,  "Play yuno"),
SDATACM2 (DTP_SCHEMA,   "pause-yuno",       0,                  0,                  pm_pause_yuno,  cmd_pause_yuno, "Pause yuno"),
SDATACM2 (DTP_SCHEMA,   "enable-yuno",      0,                  0,                  pm_enable_yuno, cmd_enable_yuno,"Enable yuno"),
SDATACM2 (DTP_SCHEMA,   "disable-yuno",     0,                  0,                  pm_disable_yuno,cmd_disable_yuno,"Disable yuno"),
SDATACM2 (DTP_SCHEMA,   "trace-on-yuno",    SDF_WILD_CMD,       0,                  pm_trace_on_yuno,cmd_trace_on_yuno,"Trace on yuno"),
SDATACM2 (DTP_SCHEMA,   "trace-off-yuno",   SDF_WILD_CMD,       0,                  pm_trace_off_yuno,cmd_trace_off_yuno,"Trace off yuno"),
SDATACM2 (DTP_SCHEMA,   "command-yuno",     SDF_WILD_CMD,       0,                  pm_command_yuno,cmd_command_yuno,"Command to yuno. WARNING: parameter's keys are not checked"),
SDATACM2 (DTP_SCHEMA,   "stats-yuno",       SDF_WILD_CMD,       0,                  pm_stats_yuno,  cmd_stats_yuno, "Get statistics of yuno"),
SDATACM2 (DTP_SCHEMA,   "authzs-yuno",      SDF_WILD_CMD,       0,                  pm_authzs_yuno,  cmd_authzs_yuno, "Get permissions of yuno"),
SDATACM2 (DTP_SCHEMA,   "ping",             0,                  0,                  0,   cmd_ping,       "Ping command"),
SDATACM2 (DTP_SCHEMA,   "node-uuid",        0,                  a_uuid,             0,   cmd_uuid,       "Get uuid of node"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_STRING,      "__username__",     SDF_RD,             "",             "Username"),
SDATA (DTP_STRING,      "tranger_path",     SDF_RD,             "/yuneta/store/agent/yuneta_agent.trdb", "tranger path"),
SDATA (DTP_STRING,      "startup_command",  SDF_RD,             0,              "Command to execute at startup"),
SDATA (DTP_JSON,        "agent_environment",SDF_RD,             0,              "Agent environment. Override the yuno environment"),
SDATA (DTP_JSON,        "node_variables",   SDF_RD,             0,              "Global to Node json config variables"),
SDATA (DTP_INTEGER,     "timerStBoot",      SDF_RD,             "6000",         "Timer to run yunos on boot"),
SDATA (DTP_INTEGER,     "signal2kill",      SDF_RD,             "3",        "Signal to kill yunos:SIGQUIT"),

SDATA (DTP_JSON,        "range_ports",      SDF_RD,             "[[11100,11199]]", "Range Ports. List of ports to be assigned to public services of yunos."),
SDATA (DTP_INTEGER,     "last_port",        SDF_WR,             0,              "Last port assigned"),
SDATA (DTP_INTEGER,     "max_consoles",     SDF_WR,             "10",           "Maximum consoles opened"),
SDATA (DTP_INTEGER,     "timeout_expiration",SDF_WR,            "60000",        "Expiration timeout for commands"),

SDATA (DTP_BOOLEAN,     "use_audit_command_file",SDF_WR,        "1",            "Use audit file commands"),
SDATA (DTP_INTEGER,     "max_megas_audit_file",SDF_WR,          "500",          "max megas rotatory file size"),
SDATA (DTP_INTEGER,     "min_free_disk_percentage",SDF_WR,      "20",           "min free disk percentage using audit file"),

SDATA (DTP_POINTER,     "user_data",        0,                  0,              "User data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,              "More user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,              "Subscriber of output-events. Not a child gobj"),
SDATA_END()
};

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
SDATAAUTHZ (DTP_SCHEMA, "open-console",     0,      0,      0,      "Permission to open console"),
SDATAAUTHZ (DTP_SCHEMA, "close-console",    0,      0,      0,      "Permission to close console"),
SDATAAUTHZ (DTP_SCHEMA, "list-consoles",    0,      0,      0,      "Permission to list consoles"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t timerStBoot;
    BOOL enabled_yunos_running;

    hgobj gobj_tranger;
    json_t *tranger;

    json_t *list_consoles; // Dictionary of console names

    hgobj resource;
    hgobj timer;
    hrotatory_h audit_file;

    json_int_t timeout_expiration;
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

    if (!atexit_registered) {
        atexit(remove_pid_file);
        atexit_registered = 1;
    }

    /*----------------------------------------*
     *  Get node uuid
     *----------------------------------------*/
    const char *uuid = node_uuid();
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "yuneta_agent starting: node uuid",
        "uuid",         "%s", uuid,
        NULL
    );

    /*----------------------------------------*
     *  Check node_owner
     *----------------------------------------*/
    const char *node_owner = gobj_yuno_node_owner();
    if(empty_string(node_owner)) {
        node_owner = "none";
        gobj_write_str_attr(gobj_yuno(), "node_owner", node_owner);

        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "node_owner EMPTY, setting none",
            NULL
        );
    }

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    BOOL is_yuneta = FALSE;
    struct passwd *pw = getpwuid(getuid());
    if(strcmp(pw->pw_name, "yuneta")==0) {
        gobj_write_str_attr(gobj, "__username__", "yuneta");
        is_yuneta = TRUE;
    } else {
        static gid_t groups[30]; // HACK to use outside
        int ngroups = sizeof(groups)/sizeof(groups[0]);

        getgrouplist(pw->pw_name, 0, groups, &ngroups);
        for(int i=0; i<ngroups; i++) {
            struct group *gr = getgrgid(groups[i]);
            if(strcmp(gr->gr_name, "yuneta")==0) {
                gobj_write_str_attr(gobj, "__username__", "yuneta");
                is_yuneta = TRUE;
                break;
            }
        }
    }
    if(!is_yuneta) {
        gobj_trace_msg(gobj, "User or group 'yuneta' is needed to run %s", gobj_yuno_role());
        printf("User or group 'yuneta' is needed to run %s\n", gobj_yuno_role());
        exit(0);
    }

    /*
     *  Chequea schema, exit si falla.
     */
    helper_quote2doublequote(treedb_schema_yuneta_agent);
    json_t *jn_treedb_schema_yuneta_agent;
    jn_treedb_schema_yuneta_agent = legalstring2json(treedb_schema_yuneta_agent, TRUE);
    if(parse_schema(jn_treedb_schema_yuneta_agent)<0) {
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

    priv->timer = gobj_create("", C_TIMER, 0, gobj);

    /*---------------------------------------*
     *      Check if already running
     *---------------------------------------*/
    {
        int pid = 0;

        FILE *file = fopen(pidfile, "r");
        if(file) {
            (void)fscanf(file, "%d", &pid);
            fclose(file);

            int ret = is_yuneta_agent(pid);
            if(ret == 0) {
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "yuneta_agent already running, exiting",
                    "pid",          "%d", pid,
                    NULL
                );
                exit(0);
            } else if(errno == ESRCH) {
                unlink(pidfile);
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "cannot check pid",
                    "pid",          "%d", pid,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
                unlink(pidfile);
            }

        }
        file = fopen(pidfile, "w");
        if(file) {
            fprintf(file, "%d\n", getpid());
            fclose(file);
        }
    }

    /*---------------------------*
     *      Timeranger
     *---------------------------*/
    const char *path = gobj_read_str_attr(gobj, "tranger_path");
    json_t *kw_tranger = json_pack("{s:s, s:s, s:b, s:I, s:i}",
        "path", path,
        "filename_mask", "%Y",
        "master", 1,
        "subscriber", (json_int_t)(uintptr_t)gobj,
        "on_critical_error", (int)(LOG_OPT_EXIT_ZERO)
    );
    priv->gobj_tranger = gobj_create_service(
        "tranger",
        C_TRANGER,
        kw_tranger,
        gobj
    );
    priv->tranger = gobj_read_pointer_attr(priv->gobj_tranger, "tranger");

    /*-----------------------------*
     *      Open Agent Treedb
     *-----------------------------*/
    const char *treedb_name = kw_get_str(gobj,
        jn_treedb_schema_yuneta_agent,
        "id",
        "treedb_yuneta_agent",
        KW_REQUIRED
    );
    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", treedb_name,
        "treedb_schema", jn_treedb_schema_yuneta_agent,
        "exit_on_error", LOG_OPT_EXIT_ZERO
    );

    priv->resource = gobj_create_service(
        treedb_name,
        C_NODE,
        kw_resource,
        gobj
    );

    if(gobj_read_bool_attr(gobj, "use_audit_command_file")) {
        /*-----------------------------*
         *      Audit
         *-----------------------------*/
        char audit_path[NAME_MAX];
        yuneta_realm_file(audit_path, sizeof(audit_path), "audit", "ZZZ-DD_MM_CCYY.log", TRUE);
        priv->audit_file = rotatory_open(
            audit_path,
            0,                      // 0 = default 64K
            gobj_read_integer_attr(gobj, "max_megas_audit_file"), // 0 = default 8
            gobj_read_integer_attr(gobj, "min_free_disk_percentage"), // 0 = default 10
            yuneta_xpermission(),   // permission for directories and executable files. 0 = default 02775
            0660,                   // permission for regular files. 0 = default 0664
            TRUE
        );
        if(priv->audit_file) {
            gobj_audit_commands(audit_command_cb, gobj);
        }
    }

    priv->list_consoles = json_object();

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timerStBoot,             gobj_read_integer_attr)
    SET_PRIV(timeout_expiration,      gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
     PRIVATE_DATA *priv = gobj_priv_data(gobj);

     IF_EQ_SET_PRIV(timeout_expiration,         gobj_read_integer_attr)
     END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->list_consoles);

    if(priv->audit_file) {
        rotatory_close(priv->audit_file);
        priv->audit_file = 0;
    }

    remove_pid_file();
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    set_timeout(priv->timer, priv->timerStBoot);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_trace_on(hgobj gobj, const char *level, json_t *kw)
{
    treedb_set_trace(TRUE);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_trace_off(hgobj gobj, const char *level, json_t *kw)
{
    treedb_set_trace(FALSE);

    KW_DECREF(kw);
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
PRIVATE json_t *cmd_ping(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("pong"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_uuid(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        json_sprintf("%s", node_uuid()),
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_authzs_doc(gobj, cmd, kw);
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
PRIVATE json_t *cmd_dir_yuneta(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *match = kw_get_str(gobj, kw, "match", 0, 0);
    if(!match) {
        match = ".*";
    }
    const char *subdirectory = kw_get_str(gobj, kw, "subdirectory", "", 0);
    char directory[PATH_MAX];
    build_path(directory, sizeof(directory), "/yuneta", subdirectory, NULL);

    dir_array_t da;
    get_ordered_filename_array(gobj,
        directory,
        match,
        WD_RECURSIVE|WD_MATCH_DIRECTORY|WD_MATCH_REGULAR_FILE|WD_MATCH_SYMBOLIC_LINK|WD_HIDDENFILES,
        &da
    );

    json_t *jn_array = json_array();
    for(int i=0; i<da.count; i++) {
        char *fullpath = da.items[i];
        json_array_append_new(jn_array, json_string(fullpath));
    }

    dir_array_free(&da);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_array,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_dir_realms(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *match = kw_get_str(gobj, kw, "match", 0, 0);
    if(!match) {
        match = ".*";
    }
    const char *subdirectory = kw_get_str(gobj, kw, "subdirectory", "", 0);
    char directory[PATH_MAX];
    build_path(directory, sizeof(directory), "/yuneta/realms", subdirectory, NULL);

    dir_array_t da;
    get_ordered_filename_array(gobj,
        directory,
        match,
        WD_RECURSIVE|WD_MATCH_DIRECTORY|WD_MATCH_REGULAR_FILE|WD_MATCH_SYMBOLIC_LINK|WD_HIDDENFILES,
        &da
    );

    json_t *jn_array = json_array();
    for(int i=0; i<da.count; i++) {
        char *fullpath = da.items[i];
        json_array_append_new(jn_array, json_string(fullpath));
    }

    dir_array_free(&da);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_array,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_dir_repos(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *match = kw_get_str(gobj, kw, "match", 0, 0);
    if(!match) {
        match = ".*";
    }
    const char *subdirectory = kw_get_str(gobj, kw, "subdirectory", "", 0);
    char directory[PATH_MAX];
    build_path(directory, sizeof(directory), "/yuneta/repos", subdirectory, NULL);

    dir_array_t da;
    get_ordered_filename_array(gobj,
        directory,
        match,
        WD_RECURSIVE|WD_MATCH_DIRECTORY|WD_MATCH_REGULAR_FILE|WD_MATCH_SYMBOLIC_LINK|WD_HIDDENFILES,
        &da
    );

    json_t *jn_array = json_array();
    for(int i=0; i<da.count; i++) {
        char *fullpath = da.items[i];
        json_array_append_new(jn_array, json_string(fullpath));
    }

    dir_array_free(&da);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_array,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_dir_store(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *match = kw_get_str(gobj, kw, "match", 0, 0);
    if(!match) {
        match = ".*";
    }

    const char *subdirectory = kw_get_str(gobj, kw, "subdirectory", "", 0);
    char directory[PATH_MAX];
    build_path(directory, sizeof(directory), "/yuneta/store", subdirectory, NULL);

    dir_array_t da;
    get_ordered_filename_array(gobj,
        directory,
        match,
        WD_RECURSIVE|WD_MATCH_DIRECTORY|WD_MATCH_REGULAR_FILE|WD_MATCH_SYMBOLIC_LINK|WD_HIDDENFILES,
        &da
    );

    json_t *jn_array = json_array();
    for(int i=0; i<da.count; i++) {
        char *fullpath = da.items[i];
        json_array_append_new(jn_array, json_string(fullpath));
    }

    dir_array_free(&da);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_array,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_dir_logs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *id = kw_get_str(gobj, kw, "id", 0, 0);
    if(!id) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("'id' required"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *node = gobj_get_node(
        priv->resource,
        "yunos",
        json_incref(kw),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(!node) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("Yuno not found: %s", id),
            0,
            0,
            kw  // owned
        );
    }

    char yuno_log_path[NAME_MAX];
    build_yuno_log_path(gobj, node, yuno_log_path, sizeof(yuno_log_path), FALSE);

    dir_array_t da;
    get_ordered_filename_array(gobj,
        yuno_log_path,
        ".*",
        WD_RECURSIVE|WD_MATCH_DIRECTORY|WD_MATCH_REGULAR_FILE|WD_MATCH_SYMBOLIC_LINK|WD_HIDDENFILES,
        &da
    );

    json_t *jn_array = json_array();
    for(int i=0; i<da.count; i++) {
        char *fullpath = da.items[i];
        json_array_append_new(jn_array, json_string(fullpath));
    }

    dir_array_free(&da);

    json_decref(node);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_array,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_dir_local_data(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *id = kw_get_str(gobj, kw, "id", 0, 0);
    if(!id) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("'id' required"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *node = gobj_get_node(
        priv->resource,
        "yunos",
        json_incref(kw),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(!node) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("Yuno not found: %s", id),
            0,
            0,
            kw  // owned
        );
    }

    char private_domain[PATH_MAX];
    build_yuno_private_domain(gobj, node, private_domain, sizeof(private_domain));

    char yuno_data_path[NAME_MAX];
    const char *work_dir = yuneta_root_dir();
    build_path(yuno_data_path, sizeof(yuno_data_path), work_dir, private_domain, NULL);

    dir_array_t da;
    get_ordered_filename_array(gobj,
        yuno_data_path,
        ".*",
        WD_RECURSIVE|WD_MATCH_DIRECTORY|WD_MATCH_REGULAR_FILE|WD_MATCH_SYMBOLIC_LINK|WD_HIDDENFILES,
        &da
    );

    json_t *jn_array = json_array();
    for(int i=0; i<da.count; i++) {
        char *fullpath = da.items[i];
        json_array_append_new(jn_array, json_string(fullpath));
    }

    dir_array_free(&da);

    json_decref(node);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_array,
        kw  // owned
    );
}

// /***************************************************************************
//  *
//  ***************************************************************************/
// PRIVATE BOOL read_json_cb(
//     void *user_data,
//     wd_found_type type,     // type found
//     char *fullpath,         // directory+filename found
//     const char *directory,  // directory of found filename
//     char *name,             // dname[255]
//     int level,              // level of tree where file found
//     int index               // index of file inside of directory, relative to 0
// )
// {
//     json_t *jn_dict = user_data;
//     size_t flags = 0;
//     json_error_t error;
//     json_t *jn_attr = json_load_file(fullpath, flags, &error);
//     if(jn_attr) {
//         json_object_set_new(jn_dict, fullpath, jn_attr);
//     } else {
//         jn_attr = json_sprintf("Invalid json in '%s' file, error '%s'", fullpath, error.text);
//         json_object_set_new(jn_dict, fullpath, jn_attr);
//     }
//     return TRUE; // to continue
// }

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_replicate_node(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
//     BOOL upgrade = strstr(cmd, "upgrade")?1:0;
//     int realm_replicates = 0;
//     json_t *kw_ids = 0;
//
//     const char *realm_id = kw_get_str(gobj, kw, "realm_id", 0, 0);
//     if(!empty_string(realm_id)) {
//         json_int_t realm_id = find_last_id_by_name(gobj, "realms", "name", realm_id);
//         if(!realm_id) {
//             return msg_iev_build_response(
//                 gobj,
//                 -1,
//                 json_sprintf("Realm %s not found", realm_id),
//                 0,
//                 0,
//                 kw  // owned
//             );
//         }
//         kw_ids = kwids_id2kwids(realm_id);
//     } else {
//         kw_ids = kwids_extract_and_expand_ids(kw);
//     }
//
//     KW_INCREF(kw_ids); // use later with yunos
//     dl_list_t *iter_realms = gobj_list_nodes(priv->resource, "realms", kw_ids);
//     realm_replicates = dl_size(iter_realms);
//     if(realm_replicates==0) {
//         KW_DECREF(kw_ids);
//         return msg_iev_build_response(
//             gobj,
//             -1,
//             json_sprintf("No realms found"),
//             0,
//             0,
//             kw  // owned
//         );
//     }
//
//     const char *filename = kw_get_str(gobj, kw, "filename", 0, 0);
//     const char *url = kw_get_str(gobj, kw, "url", 0, 0);
//
//     /*----------------------------------*
//      *      Build destination file
//      *----------------------------------*/
//     char fecha[32];
//     char source_[NAME_MAX];
//     if(empty_string(filename)) {
//         /*
//         *  Mask "DD/MM/CCYY-hh:mm:ss-w-ddd"
//         */
//         time_t t;
//         time(&t);
//         strftime(fecha, sizeof(fecha), "%Y-%m-%d", localtime(&t));
//
//         if(!empty_string(realm_id)) {
//             snprintf(source_, sizeof(source_), "%s-%s-realm-%s.json",
//                 upgrade?"upgrade":"replicate",
//                 fecha,
//                 realm_id
//             );
//         } else {
//             gbuffer_t *gbuf_ids = gbuffer_create((size_t)4*1024, (size_t)32*1024);
//
//             hsdata hs_realm; rc_instance_t *i_hs;
//             i_hs = rc_first_instance(iter_realms, (rc_resource_t **)&hs_realm);
//             while(i_hs) {
//                 json_int_t realm_id = SDATA_GET_ID(hs_realm);
//                 gbuffer_printf(gbuf_ids, "-%d", (int)realm_id);
//
//                 /*
//                 *  Next realm
//                 */
//                 i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs_realm);
//             }
//
//             char *realms_ids = gbuffer_cur_rd_pointer(gbuf_ids);
//             snprintf(source_, sizeof(source_), "%s-%s-realms%s.json",
//                 upgrade?"upgrade":"replicate",
//                 fecha,
//                 realms_ids
//             );
//             gbuffer_decref(gbuf_ids);
//         }
//         filename = source_;
//     }
//     char path[NAME_MAX];
//     yuneta_store_file(path, sizeof(path), "replicates", "", filename, TRUE);
//
//     /*----------------------------------*
//      *      Create json script file
//      *----------------------------------*/
//     FILE *file = fopen(path, "w");
//     if(!file) {
//         KW_DECREF(kw_ids);
//         gobj_free_iter(iter_realms, TRUE, 0);
//         return msg_iev_build_response(
//             gobj,
//             -1,
//             json_sprintf("Cannot create '%s' file", path),
//             0,
//             0,
//             kw  // owned
//         );
//     }
//
//     /*----------------------------------*
//      *      Fill realms
//      *----------------------------------*/
//     hsdata hs_realm; rc_instance_t *i_hs;
//     i_hs = rc_first_instance(iter_realms, (rc_resource_t **)&hs_realm);
//     while(i_hs) {
//         json_t *jn_range_ports = SDATA_GET_JSON(hs_realm xxxx , "range_ports");
//         char *range_ports = json2uglystr(jn_range_ports);
//         fprintf(file, "{\"command\": \"%screate-realm domain='%s' range_ports=%s role='%s' name='%s' bind_ip='%s'\"}\n",
//             upgrade?"-":"",
//             SDATA_GET_STR(hs_realm, "domain"),
//             range_ports,
//             SDATA_GET_STR(hs_realm, "role"),
//             SDATA_GET_STR(hs_realm, "name"),
//             SDATA_GET_STR(hs_realm, "bind_ip")
//         );
//         gbmem_free(range_ports);
//
//         /*
//          *  Next realm
//          */
//         i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs_realm);
//     }
//     fprintf(file, "\n");
//     gobj_free_iter(iter_realms, TRUE, 0);
//
//     /*---------------------------------------------------------------*
//      *      Fill top yunos with his binaries and configurations
//      *---------------------------------------------------------------*/
//     /*
//      *  Control repeated binaries/configurations
//      */
//     json_t *jn_added_binaries = json_array();
//     json_t *jn_added_configs = json_array();
//
//     dl_list_t *iter_yunos = gobj_list_nodes(priv->resource, "yunos", 0); // Select all yunos
//     json_t *yuno;
//     i_hs = rc_first_instance(iter_yunos, (rc_resource_t **)&hs_yuno);
//     while(i_hs) {
//         BOOL valid_yuno = TRUE;
//         /*
//          *  The rule: only enabled yunos and taged yunos are replicated.
//          */
//         BOOL yuno_disabled = kw_get_bool(gobj, yuno, "yuno_disabled");
//         const char *tag = kw_get_str(gobj, yuno, "yuno_tag");
//         if(empty_string(tag)) {
//             if(yuno_disabled) {
//                 // Sin tag y disabled, ignora en cualquier caso.
//                 valid_yuno = FALSE;
//             }
//         } else {
//             // NEW Version 2.2.5
//             if(yuno_disabled && upgrade) {
//                 // Con tag y disabled, ignora en upgrade, no en replicate.
//                 valid_yuno = FALSE;
//             }
//         }
//         if(!valid_yuno) {
//             /*
//              *  Next
//              */
//             i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs_yuno);
//             continue;
//         }
//         /*
//          *  Check if yuno belongs to some realm to replicate.
//          */
//         if(json_array_size(json_object_get(kw_ids, "ids"))>0) {
//             json_int_t realm_id = kw_get_str(gobj, yuno, "realm_id", "", KW_REQUIRED);
//             if(!int_in_dict_list(realm_id, kw_ids, "ids")) {
//                 valid_yuno = FALSE;
//             }
//         }
//         if(!valid_yuno) {
//             /*
//              *  Next
//              */
//             i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs_yuno);
//             continue;
//         }
//
//         /*
//          *  Valid yuno to replicate.
//          */
//         const char *realm_id = kw_get_str(gobj, yuno, "realm_id");
//         if(!realm_id) {
//             realm_id = "";
//         }
//         const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role");
//         const char *yuno_name = kw_get_str(gobj, yuno, "yuno_name");
//         if(!yuno_name) {
//             yuno_name = "";
//         }
//         const char *yuno_tag = kw_get_str(gobj, yuno, "yuno_tag");
//         if(!yuno_tag) {
//             yuno_tag = "";
//         }
//
//         /*
//          *  Order: kill-yuno.
//          */
//         fprintf(file,
//             "{\"command\": \"-kill-yuno realm_id='%s' yuno_role='%s' yuno_name='%s'\"}\n",
//             realm_id,
//             yuno_role,
//             yuno_name
//         );
//
//         /*
//          *  Save yuno's configurations.
//          */
//         dl_list_t *iter_config_ids = xsdata_read_iter(hs_yuno, "config_ids");
//         if(rc_iter_size(iter_config_ids)>0) {
//             hsdata hs_config; rc_instance_t *i_hs;
//             i_hs = rc_first_instance(iter_config_ids, (rc_resource_t **)&hs_config);
//             while(i_hs) {
//                 json_int_t config_id = SDATA_GET_ID(hs_config);
//                 if(int_in_jn_list(config_id, jn_added_configs)) {
//                     /*
//                      *  Next config
//                      */
//                     i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs_config);
//                     continue;
//                 }
//
//                 const char *name = SDATA_GET_STR(hs_config, "name");
//                 const char *version = SDATA_GET_STR(hs_config, "version");
//                 const char *description = SDATA_GET_STR(hs_config, "description");
//                 json_t *jn_content = SDATA_GET_JSON(hs_config, "zcontent");
//                 char *content = json2uglystr(jn_content);
//                 gbuffer_t *gbuf_base64 = gbuf_string2base64(content, (size_t)strlen(content));
//                 const char *p = gbuffer_cur_rd_pointer(gbuf_base64);
//
//                 fprintf(file,
//                     "{\"command\": \"%screate-config '%s' version='%s' description='%s' content64=%s\"}\n",
//                     upgrade?"-":"",
//                     name,
//                     version,
//                     description,
//                     p
//                 );
//                 gbmem_free(content);
//                 gbuffer_decref(gbuf_base64);
//
//                 json_array_append_new(jn_added_configs, json_integer(config_id));
//
//                 /*
//                  *  Next config
//                  */
//                 i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs_config);
//             }
//         }
//
//         /*
//          *  Save yuno's binary.
//          */
//         json_int_t binary_id = xsdata_read_uint64(hs_yuno, "binary_id");
//         hsdata hs_binary = gobj_get_node(priv->resource, "binaries", 0, binary_id);
//         if(hs_binary) {
//             if(!int_in_jn_list(binary_id, jn_added_binaries)) {
//                 const char *role = SDATA_GET_STR(hs_binary, "role");
//                 char temp[NAME_MAX];
//                 snprintf(temp, sizeof(temp), "/yuneta/development/output/yunos/%s", role);
//                 if(access(temp, 0)==0) {
//                     fprintf(file,
//                         "{\"command\": \"%sinstall-binary '%s' content64=$$(%s)\"}\n",
//                         upgrade?"-":"",
//                         role,
//                         role
//                     );
//                 } else {
//                     const char *binary = SDATA_GET_STR(hs_binary, "binary");
//                     gbuffer_t *gbuf_base64 = gbuf_file2base64(binary);
//                     char *p = gbuffer_cur_rd_pointer(gbuf_base64);
//
//                     fprintf(file,
//                         "{\"command\": \"%sinstall-binary '%s' content64=%s\"}\n",
//                         upgrade?"-":"",
//                         role,
//                         p
//                     );
//                     gbuffer_decref(gbuf_base64);
//                 }
//
//                 json_array_append_new(jn_added_binaries, json_integer(binary_id));
//             }
//         }
//
//         /*
//          *  Order: create-yuno.
//          */
//         fprintf(file,
//             "{\"command\": \"%screate-yuno realm_id='%s' yuno_role='%s' yuno_name='%s' yuno_tag='%s' disabled=%d\"}\n",
//             upgrade?"-":"",
//             realm_id,
//             yuno_role,
//             yuno_name,
//             yuno_tag,
//             yuno_disabled?1:0
//         );
//
//         if(upgrade) {
//             /*
//              *  Order: top-last con filtro.
//              */
//             json_t *jn_filter = json_pack("{s:s, s:s, s:s, s:b, s:b}",
//                 "realm_id", realm_id,
//                 "yuno_role", yuno_role,
//                 "yuno_name", yuno_name,
//                 "yuno_running", 1,
//                 "yuno_playing", 1
//             );
//             char *filter = json2uglystr(jn_filter);
//             fprintf(file,
//                 "{\"command\": \"-top-last-yuno realm_id='%s' yuno_role='%s' yuno_name='%s'\", \"response_filter\":%s}\n\n",
//                 realm_id,
//                 yuno_role,
//                 yuno_name,
//                 filter
//             );
//             json_decref(jn_filter);
//             gbmem_free(filter);
//         } else {
//             fprintf(file,
//                 "{\"command\": \"-run-yuno realm_id='%s' yuno_role='%s' yuno_name='%s'\"}\n\n",
//                 realm_id,
//                 yuno_role,
//                 yuno_name
//             );
//         }
//
//         /*
//          *  Next
//          */
//         i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs_yuno);
//     }
//     fprintf(file, "\n\n");
//     gobj_free_iter(iter_yunos, TRUE, 0);
//
//     /*----------------------------------*
//      *      Close
//      *----------------------------------*/
//     json_decref(jn_added_binaries);
//     json_decref(jn_added_configs);
//
//     fclose(file);
//     KW_DECREF(kw_ids);
//
//     /*----------------------------------*
//      *      Execute the file
//      *----------------------------------*/
//     if(!empty_string(url)) {
//         //ybatch_json_command_file(gobj, url, path);
//         // exec json command
//     }
//
//     return msg_iev_build_response(
//         gobj,
//         0,
//         json_sprintf("%d realms replicated in '%s' filename", realm_replicates, path),
//         0,
//         0,
//         kw  // owned
//     );
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_replicate_binaries(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
//     int binary_replicates = 0;
//     json_t *kw_ids = 0;
//
//     const char *role = kw_get_str(gobj, kw, "role", 0, 0);
//     if(!empty_string(role)) {
//         json_int_t binary_id = find_last_id_by_name(gobj, "binaries", "role", role);
//         if(!binary_id) {
//             return msg_iev_build_response(
//                 gobj,
//                 -1,
//                 json_sprintf("Binary %s not found", role),
//                 0,
//                 0,
//                 kw  // owned
//             );
//         }
//         kw_ids = kwids_id2kwids(binary_id);
//     } else {
//         kw_ids = kwids_extract_and_expand_ids(kw);
//     }
//
//     dl_list_t *iter_binaries = gobj_list_nodes(priv->resource, "binaries", kw_ids);
//     binary_replicates = dl_size(iter_binaries);
//     if(binary_replicates==0) {
//         return msg_iev_build_response(
//             gobj,
//             -1,
//             json_sprintf("No binary found"),
//             0,
//             0,
//             kw  // owned
//         );
//     }
//
//     const char *filename = kw_get_str(gobj, kw, "filename", 0, 0);
//     const char *url = kw_get_str(gobj, kw, "url", 0, 0);
//
//     /*----------------------------------*
//      *      Build destination file
//      *----------------------------------*/
//     char fecha[32];
//     char source_[NAME_MAX];
//     if(empty_string(filename)) {
//         /*
//         *  Mask "DD/MM/CCYY-hh:mm:ss-w-ddd"
//         */
//         time_t t;
//         time(&t);
//         strftime(fecha, sizeof(fecha), "%Y-%m-%d", localtime(&t));
//
//         if(!empty_string(role)) {
//             snprintf(source_, sizeof(source_), "%s-%s-binary-%s.json",
//                 "replicate",
//                 fecha,
//                 role
//             );
//         } else {
//             gbuffer_t *gbuf_ids = gbuffer_create(4*1024, 32*1024);
//
//             hsdata hs_binary; rc_instance_t *i_hs;
//             i_hs = rc_first_instance(iter_binaries, (rc_resource_t **)&hs_binary);
//             while(i_hs) {
//                 json_int_t binary_id = SDATA_GET_ID(hs_binary);
//                 gbuffer_printf(gbuf_ids, "-%d", (int)binary_id);
//
//                 /*
//                  *  Next binary
//                  */
//                 i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs_binary);
//             }
//
//             char *binary_ids = gbuffer_cur_rd_pointer(gbuf_ids);
//             snprintf(source_, sizeof(source_), "%s-%s-binaries%s.json",
//                 "replicate",
//                 fecha,
//                 binary_ids
//             );
//             gbuffer_decref(gbuf_ids);
//         }
//         filename = source_;
//     }
//     char path[NAME_MAX];
//     yuneta_store_file(path, sizeof(path), "replicates", "", filename, TRUE);
//
//     /*----------------------------------*
//      *      Create json script file
//      *----------------------------------*/
//     FILE *file = fopen(path, "w");
//     if(!file) {
//         gobj_free_iter(iter_binaries, TRUE, 0);
//         return msg_iev_build_response(
//             gobj,
//             -1,
//             json_sprintf("Cannot create '%s' file", path),
//             0,
//             0,
//             kw  // owned
//         );
//     }
//
//     /*----------------------------*
//      *      Fill binaries
//      *----------------------------*/
//     hsdata hs_binary; rc_instance_t *i_hs;
//     i_hs = rc_first_instance(iter_binaries, (rc_resource_t **)&hs_binary);
//     while(i_hs) {
//         json_int_t binary_id = SDATA_GET_ID(hs_binary);
//         /*
//          *  Save yuno's binary.
//          */
//         hsdata hs_binary = gobj_get_node(priv->resource, "binaries", 0, binary_id);
//         if(hs_binary) {
//             const char *role = SDATA_GET_STR(hs_binary, "role");
//             char temp[NAME_MAX];
//             snprintf(temp, sizeof(temp), "/yuneta/development/output/yunos/%s", role);
//             if(access(temp, 0)==0) {
//                 fprintf(file,
//                     "{\"command\": \"install-binary '%s' content64=$$(%s)\"}\n",
//                     role,
//                     role
//                 );
//             } else {
//                 const char *binary = SDATA_GET_STR(hs_binary, "binary");
//                 gbuffer_t *gbuf_base64 = gbuf_file2base64(binary);
//                 char *p = gbuffer_cur_rd_pointer(gbuf_base64);
//
//                 fprintf(file,
//                     "{\"command\": \"install-binary '%s' content64=%s\"}\n",
//                     role,
//                     p
//                 );
//                 gbuffer_decref(gbuf_base64);
//             }
//         }
//
//         /*
//          *  Next binary
//          */
//         i_hs = rc_next_instance(i_hs, (rc_resource_t **)&hs_binary);
//     }
//
//     fprintf(file, "\n\n");
//
//     /*----------------------------------*
//      *      Close
//      *----------------------------------*/
//
//     fclose(file);
//
//     /*----------------------------------*
//      *      Execute the file
//      *----------------------------------*/
//     if(!empty_string(url)) {
//         //ybatch_json_command_file(gobj, url, path);
//         // exec json command
//     }
//
//     return msg_iev_build_response(
//         gobj,
//         0,
//         json_sprintf("%d binaries replicated in '%s' filename", binary_replicates, path),
//         0,
//         0,
//         kw  // owned
//     );
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_public_services(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "public_services";

    /*
     *  Get a iter of matched resources
     */
    json_t *jn_data = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s", cmd),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_update_public_service(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "public_services";

    /*
     *  Get a iter of matched resources.
     */
    if(!kw_has_key(kw, "__filter__")) {
        json_object_set_new(kw, "__filter__", json_object());
    }
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Select some public service please"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Update database, with new data from kw
     *      (fields allowed defined in command parameters)
     */
    int result = 0;
    json_t *jn_data = json_array();

    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_t *update = kw_duplicate(gobj, kw);
        json_object_set(update, "id", kw_get_dict_value(gobj, node, "id", 0, KW_REQUIRED));

        json_t *public_service = gobj_update_node(
            priv->resource,
            resource,
            update,
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
        if(public_service) {
            json_array_append_new(jn_data, public_service);
        } else {
            result += -1;
        }
    }
    json_decref(iter);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        result,
        result<0?
            json_sprintf("%s", gobj_log_last_message()):
            json_sprintf("Updated"),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_public_service(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "public_services";

    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Select some public service please"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Delete
     */
    int result = 0;
    json_t *jn_data = json_array();

    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_array_append_new(jn_data, json_string(kw_get_str(gobj, node, "id", "", 0)));
        if(gobj_delete_node(
                priv->resource, resource, node, json_pack("{s:b}", "force", force), src)<0) {
            result += -1;
            break;
        }
    }

    JSON_DECREF(iter)

    return msg_iev_build_response(
        gobj,
        result,
        result<0?
            json_sprintf("%s", gobj_log_last_message()):
            json_sprintf("%d public services deleted", idx),
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_realms(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "realms";

    /*
     *  Get a iter of matched resources
     */
    json_t *jn_data = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s", cmd),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_check_realm(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "realms";

    const char *id = kw_get_str(gobj, kw, "id", "", 0);
    if(empty_string(id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What realm id?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *node = gobj_get_node(
        priv->resource,
        resource,
        json_incref(kw),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    int ret = node?0:-1;

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        ret,
        ret<0?json_sprintf("Not found!"):json_sprintf("Exists!"),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        json_pack("[o]", node), // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_create_realm(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "realms";

    const char *realm_owner = kw_get_str(gobj, kw, "realm_owner", "", 0);
    const char *realm_role = kw_get_str(gobj, kw, "realm_role", "", 0);
    const char *realm_name = kw_get_str(gobj, kw, "realm_name", "", 0);
    const char *realm_env = kw_get_str(gobj, kw, "realm_env", "", 0);

    if(empty_string(realm_owner)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What realm owner?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(realm_role)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What realm role?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(realm_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What realm name?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(realm_env)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What realm environment?"),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Check if already exists
     *------------------------------------------------*/
    json_t *kw_find = json_pack("{s:s, s:s, s:s, s:s}",
        "realm_owner", realm_owner,
        "realm_role", realm_role,
        "realm_name", realm_name,
        "realm_env", realm_env
    );

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_find, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)) {
        /*
         *  1 o more records, yuno already stored and without overwrite.
         */
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "Realm already exists"
            ),
            tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
            iter,
            kw  // owned
        );
    }
    JSON_DECREF(iter)

    /*
     *  Add to database
     */
    KW_INCREF(kw);
    json_t *node = gobj_create_node(
        priv->resource,
        resource,
        kw,
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(!node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Cannot create realm: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Convert result in json
     */
    json_t *jn_data = json_array();
    json_array_append_new(jn_data, node);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_update_realm(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "realms";

    /*
     *  Get a iter of matched resources.
     */
    if(!kw_has_key(kw, "__filter__")) {
        json_object_set_new(kw, "__filter__", json_object());
    }
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Select some realm please"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Update database, with new data from kw
     *      (fields allowed defined in command parameters)
     */
    int result = 0;
    json_t *jn_data = json_array();

    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_t *update = kw_duplicate(gobj, kw);
        json_object_set(update, "id", kw_get_dict_value(gobj, node, "id", 0, KW_REQUIRED));

        json_t *realm = gobj_update_node(
            priv->resource,
            resource,
            update,
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
        if(realm) {
            json_array_append_new(jn_data, realm);
        } else {
            result += -1;
        }
    }
    json_decref(iter);

    /*
     *  Inform
     */

    return msg_iev_build_response(
        gobj,
        result,
        result<0?
            json_sprintf("%s", gobj_log_last_message()):
            json_sprintf("Updated"),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_realm(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "realms";

    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Select some realm please"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Check conditions to delete
     */
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_t *yunos = kw_get_list(gobj, node, "yunos", 0, KW_REQUIRED);
        int use = json_array_size(yunos);

        if(use > 0) {
            json_t *comment = json_sprintf(
                "Cannot delete realm '%s'. Using in %d yunos",
                kw_get_str(gobj, node, "id", "", KW_REQUIRED),
                use
            );
            JSON_DECREF(iter)
            return msg_iev_build_response(
                gobj,
                -1,
                comment,
                0,
                0,
                kw  // owned
            );
        }
    }

    /*
     *  Delete
     */
    int result = 0;
    json_t *jn_data = json_array();
    json_array_foreach(iter, idx, node) {
        json_array_append_new(jn_data, json_string(kw_get_str(gobj, node, "id", "", 0)));
        if(gobj_delete_node(
                priv->resource, resource, node, json_pack("{s:b}", "force", force), src)<0) {
            result += -1;
            break;
        }
    }

    JSON_DECREF(iter)

    return msg_iev_build_response(
        gobj,
        0,
        result<0?
            json_sprintf("%s", gobj_log_last_message()):
            json_sprintf("%d realms deleted", idx),
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_binaries(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "binaries";

    /*
     *  Get a iter of matched resources
     */
    json_t *jn_data = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s", cmd),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *  Get the information returned of executing  ``yuno --role``
 ***************************************************************************/
PRIVATE json_t *yuno_basic_information(hgobj gobj, const char *cmd)
{
    /*
     *  Execute cmd --role
     */
    size_t size = gbmem_get_maximum_block();
    char *cmd_output = gbmem_malloc(gbmem_get_maximum_block());
    if(!cmd_output) {
        // Error already logged
        return 0;
    }
    char command[NAME_MAX];
    snprintf(command, sizeof(command), "%s --print-role  2>&1", cmd);
    if(run_command(command, cmd_output, size)<0 || !*cmd_output) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Incorrect role bin path",
            "command",      "%s", command,
            "output",       "%s", cmd_output,
            NULL
        );
        gbmem_free(cmd_output);
        return 0;
    }

    /*
     *  Parse output
     */
    char *p = strchr(cmd_output, '{');
    if(!p) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Dict not found",
            "command",      "%s", command,
            "output",       "%s", cmd_output,
            NULL
        );
        gbmem_free(cmd_output);
        return 0;
    }
    json_t *jn_basic_info = legalstring2json(p, TRUE);
    gbmem_free(cmd_output);
    return jn_basic_info;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_install_binary(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "binaries";

    const char *id = kw_get_str(gobj, kw, "id", "", 0);

    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    if(empty_string(content64)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No data in content64"),
            0,
            0,
            kw  // owned
        );
    }
    gbuffer_t *gbuf_content = gbuffer_base64_to_string(content64, strlen(content64));
    if(!gbuf_content) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Bad data in content64"),
            0,
            0,
            kw  // owned
        );
    }
    char path[NAME_MAX];
    yuneta_realm_file(path, sizeof(path), "temp", id, TRUE);
    gbuf2file(
        gobj,
        gbuf_content, // owned
        path,
        yuneta_xpermission(),
        TRUE
    );

    json_t *jn_basic_info = yuno_basic_information(gobj, path);
    if(!jn_basic_info) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "It seems a wrong yuno binary"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Binary received.
     *------------------------------------------------*/
    const char *binary_role = kw_get_str(gobj, jn_basic_info, "role", 0, 0);
    const char *binary_version = kw_get_str(gobj, jn_basic_info, "version", "", 0);
    json_t *jn_tags = kw_get_dict_value(gobj, jn_basic_info, "tags", 0, 0);

    /*------------------------------------------------*
     *      Check if already exists
     *------------------------------------------------*/
    json_t *kw_find = json_pack("{s:s, s:s}",
        "id", binary_role,
        "version", binary_version
    );

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_find, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)) {
        /*
         *  1 o more records, yuno already stored and without overwrite.
         */
        json_t *comment = json_sprintf(
            "Binary already exists: role %s, version %s",
            binary_role, binary_version
        );
        JSON_DECREF(iter)
        json_t *msg_webix = msg_iev_build_response(
            gobj,
            -1,
            comment,
            tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
            iter,
            kw  // owned
        );
        JSON_DECREF(jn_basic_info);
        return msg_webix;
    }
    JSON_DECREF(iter)

    /*------------------------------------------------*
     *      Store in filesystem
     *------------------------------------------------*/
    /*
     *  Destination: inside of /yuneta/repos
     *      {{tags}}/{{role}}/{{version}}/binary.exe
     */
    char destination[NAME_MAX];
    yuneta_repos_yuno_dir(
        destination,
        sizeof(destination),
        jn_tags,
        binary_role,
        binary_version,
        TRUE
    );
    if(access(destination,0)!=0) {
        JSON_DECREF(jn_basic_info);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "Cannot create '%s' directory",
                destination
            ),
            0,
            0,
            kw  // owned
        );
    }
    yuneta_repos_yuno_file(
        destination,
        sizeof(destination),
        jn_tags,
        binary_role,
        binary_version,
        binary_role,
        TRUE
    );
    /*
     *  Overwrite, the overwrite filter was above.
     */
    if(copyfile(path, destination, yuneta_xpermission(), TRUE)<0) {
         gobj_log_error(gobj, 0,
             "function",     "%s", __FUNCTION__,
             "msgset",       "%s", MSGSET_SYSTEM_ERROR,
             "msg",          "%s", "copyfile() FAILED",
             "path",         "%s", path,
             "destination",  "%s", destination,
             NULL
         );
         JSON_DECREF(jn_basic_info);
         return msg_iev_build_response(
             gobj,
             -1,
             json_sprintf(
                 "Cannot copy '%s' to '%s'",
                 path,
                 destination
             ),
             0,
             0,
             kw  // owned
         );
     }

    /*------------------------------------------------*
     *      Store in db
     *------------------------------------------------*/
    json_object_set_new(
        jn_basic_info,
        "size",
        json_integer(
            (json_int_t)filesize(destination)
        )
    );
    json_object_set_new(
        jn_basic_info,
        "binary",
        json_string(destination)
    );
    if(id) {
        json_object_set_new(
            jn_basic_info,
            "id",
            json_string(id)
        );
    }

    /*
     *  Add to database
     */
    json_t *node = gobj_create_node(
        priv->resource,
        resource,
        jn_basic_info,
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(!node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Cannot create binary: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Convert result in json
     */
    json_t *jn_data = json_array();
    json_array_append_new(jn_data, node);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("version: %s", kw_get_str(gobj, node, "version", "", KW_REQUIRED)),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_update_binary(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "binaries";

    // TODO legacy permite hacer update solo al binario activo!, con las config igual creo.
    const char *id = kw_get_str(gobj, kw, "id", "", 0);
    if(!id) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("'id' required"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *node = gobj_get_node(
        priv->resource,
        resource,
        json_incref(kw),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(!node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Binary not found"),
            0,
            0,
            kw  // owned
        );
    }
    const char *role = kw_get_str(gobj, node, "id", "", KW_REQUIRED);

    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    if(empty_string(content64)) {
        json_decref(node);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No data in content64"),
            0,
            0,
            kw  // owned
        );
    }
    gbuffer_t *gbuf_content = gbuffer_base64_to_string(content64, strlen(content64));
    if(!gbuf_content) {
        json_decref(node);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Bad data in content64"),
            0,
            0,
            kw  // owned
        );
    }
    char path[NAME_MAX];
    yuneta_realm_file(path, sizeof(path), "temp", role, TRUE);
    gbuf2file(
        gobj,
        gbuf_content, // owned
        path,
        yuneta_xpermission(),
        TRUE
    );

    json_t *jn_basic_info = yuno_basic_information(gobj, path);
    if(!jn_basic_info) {
        json_decref(node);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "It seems a wrong yuno binary: %s", path
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Binary received.
     *------------------------------------------------*/
    const char *binary_role = kw_get_str(gobj, jn_basic_info, "role", 0, 0);
    const char *binary_version = kw_get_str(gobj, jn_basic_info, "version", "", 0);
    json_t *jn_tags = kw_get_dict_value(gobj, jn_basic_info, "tags", 0, 0);

    /*------------------------------------------------*
     *      Store in filesystem
     *------------------------------------------------*/
    /*
     *  Destination: inside of /yuneta/repos
     *      {{tags}}/{{role}/{{version}}/binary.exe
     */
    char destination[NAME_MAX];
    yuneta_repos_yuno_dir(
        destination,
        sizeof(destination),
        jn_tags,
        binary_role,
        binary_version,
        TRUE
    );
    if(access(destination,0)!=0) {
        JSON_DECREF(jn_basic_info);
        json_decref(node);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "Cannot create '%s' directory",
                destination
            ),
            0,
            0,
            kw  // owned
        );
    }
    yuneta_repos_yuno_file(
        destination,
        sizeof(destination),
        jn_tags,
        binary_role,
        binary_version,
        binary_role,
        TRUE
    );
    /*
     *  Overwrite, the overwrite filter was above.
     */
    if(copyfile(path, destination, yuneta_xpermission(), TRUE)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "copyfile() FAILED",
            "path",         "%s", path,
            "destination",  "%s", destination,
            NULL
        );
        JSON_DECREF(jn_basic_info);
        json_decref(node);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "Cannot copy '%s' to '%s'",
                path,
                destination
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *  Update the resource
     *------------------------------------------------*/
    json_object_set_new(
        jn_basic_info,
        "size",
        json_integer(
            (json_int_t)filesize(destination)
        )
    );
    json_object_set_new(
        jn_basic_info,
        "binary",
        json_string(destination)
    );
    //"tags"
    json_object_update(node, jn_basic_info);
    JSON_DECREF(jn_basic_info);

    node = gobj_update_node(
        priv->resource,
        resource,
        node,
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Convert result in json
     */
    json_t *jn_data = json_array();
    json_array_append_new(jn_data, node);

    /*
     *  Inform
     */
    json_t *webix = msg_iev_build_response(
        gobj,
        0,
        json_sprintf("version: %s", kw_get_str(gobj, node, "version", "", KW_REQUIRED)),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );

    return webix;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_binary(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "binaries";

    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw),  // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Select some binary please"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Check conditions to delete
     */
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_t *yunos = kw_get_list(gobj, node, "yunos", 0, KW_REQUIRED);
        int use = json_array_size(yunos);
        if(use > 0) {
            json_t *comment = json_sprintf(
                "Cannot delete binary '%s'. Using in %d yunos",
                kw_get_str(gobj, node, "id", "", KW_REQUIRED),
                use
            );
            JSON_DECREF(iter)
            return msg_iev_build_response(
                gobj,
                -1,
                comment,
                0,
                0,
                kw  // owned
            );
        }
    }

    /*
     *  Delete
     */
    int result = 0;
    json_t *jn_data = json_array();
    json_array_foreach(iter, idx, node) {
        json_t *jn_tags = kw_get_dict_value(gobj, node, "tags", 0, KW_REQUIRED);
        const char *role = kw_get_str(gobj, node, "id", "", KW_REQUIRED);
        const char *version = kw_get_str(gobj, node, "version", "", KW_REQUIRED);
        char destination[NAME_MAX];
        yuneta_repos_yuno_dir(
            destination,
            sizeof(destination),
            jn_tags,
            role,
            version,
            FALSE
        );

        if(gobj_delete_node(
                priv->resource, resource, node, json_pack("{s:b}", "force", force), src)<0) {
            result += -1;
            break;
        }
        json_array_append_new(jn_data, json_string(destination));

        /*
         *  Remove from store in filesystem
         */
        if(access(destination,0)==0) {
            rmrdir(destination);
        }
    }

    JSON_DECREF(iter)

    return msg_iev_build_response(
        gobj,
        result,
        result<0?
            json_sprintf("%s", gobj_log_last_message()):
            json_sprintf("%d binaries deleted", idx),
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_configs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "configurations";

    /*
     *  Get a iter of matched resources
     */
    json_t *jn_data = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s", cmd),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_create_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "configurations";

    const char *id = kw_get_str(gobj, kw, "id", "", 0);
    if(empty_string(id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Configuration Id is required"),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *  Firstly get content in base64 and decode
     *------------------------------------------------*/
    json_t *jn_config;
    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    if(!empty_string(content64)) {
        gbuffer_t *gbuf_content = gbuffer_base64_to_string(content64, strlen(content64));
        jn_config = gbuf2json(
            gbuf_content,  // owned
            2
        );
        if(!jn_config) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Bad json in content64"),
                0,
                0,
                kw  // owned
            );
        }
    } else {
        jn_config = json_object();
    }

    /*
     *  NEW: get version and description from config file
     */
    const char *version = kw_get_str(gobj, jn_config, "__version__", "", 0);
    const char *description = kw_get_str(gobj, jn_config, "__description__", "", 0);
    if(empty_string(version)) {
        JSON_DECREF(jn_config);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Configuration version is required"),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Check if already exists
     *------------------------------------------------*/
    json_t *kw_find = json_pack("{s:s, s:s}",
        "id", id,
        "version", version
    );
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_find, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)) {
        /*
         *  1 o more records, yuno already stored and without overwrite.
         */
        JSON_DECREF(jn_config);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "Configuration already exists"
            ),
            tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
            iter,
            kw  // owned
        );
    }
    JSON_DECREF(iter)

    /*------------------------------------------------*
     *      Create record
     *------------------------------------------------*/
    json_t *kw_configuration = json_pack("{s:s, s:s, s:s}",
        "id", id,
        "version", version,
        "description", description
    );

    char current_date[22];
    current_timestamp(current_date, sizeof(current_date));  // "CCYY/MM/DD hh:mm:ss"
    json_object_set_new(
        kw_configuration,
        "date",
        json_string(current_date)
    );
    json_object_set_new(
        kw_configuration,
        "zcontent",
        jn_config // owned
    );
    if(id) {
        json_object_set_new(
            kw_configuration,
            "id",
            json_string(id)
        );
    }

    /*
     *  Add to database
     */
    json_t *node = gobj_create_node(
        priv->resource,
        resource,
        kw_configuration,
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(!node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Cannot create config: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Inform
     */
    json_t *jn_data = json_array();
    json_array_append_new(jn_data, node);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("version: %s", kw_get_str(gobj, node, "version", "", KW_REQUIRED)),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_update_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "configurations";

    /*
     *  Get resources to delete.
     *  Search is restricted to id only
     */
    const char *id = kw_get_str(gobj, kw, "id", 0, 0);
    if(!id) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("'id' required"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get new config
     */
    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    json_t *jn_config = 0;
    if(!empty_string(content64)) {
        gbuffer_t *gbuf_content = gbuffer_base64_to_string(content64, strlen(content64));
        jn_config = gbuf2json(
            gbuf_content,  // owned
            2
        );
    }
    if(!jn_config) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Bad json in content64"),
            0,
            0,
            kw  // owned
        );
    }

    const char *version = kw_get_str(gobj, jn_config, "__version__", "", 0);

    json_t *kw_find = json_pack("{s:s, s:s}",
        "id", id,
        "version", version
    );
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_find, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)!=1) {
        JSON_DECREF(jn_config);
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "Configuration not found"
            ),
            tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
            iter,
            kw  // owned
        );
    }

    json_t *node = json_array_get(iter, 0);
    json_object_set_new(node, "zcontent", jn_config);

    char current_date[22];
    current_timestamp(current_date, sizeof(current_date));  // "CCYY/MM/DD hh:mm:ss"
    json_object_set_new(
        node,
        "date",
        json_string(current_date)
    );

    /*
     *  Update config
     */
    node = gobj_update_node(
        priv->resource,
        resource,
        json_incref(node),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    JSON_DECREF(iter)

    /*
     *  Inform
     */
    json_t *jn_data = json_array();
    json_array_append_new(jn_data, node);

    json_t *webix = msg_iev_build_response(
        gobj,
        0,
        json_sprintf("version: %s", kw_get_str(gobj, node, "version", "", KW_REQUIRED)),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );

    return webix;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_config(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "configurations";

    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw),  // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Select some configuration please"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Check conditions to delete
     */
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_t *yunos = kw_get_list(gobj, node, "yunos", 0, KW_REQUIRED);
        int use = json_array_size(yunos);
        if(use > 0) {
            json_t *comment = json_sprintf(
                "Cannot delete configuration '%s'. Using in %d yunos",
                kw_get_str(gobj, node, "id", "", KW_REQUIRED),
                use
            );
            JSON_DECREF(iter)
            return msg_iev_build_response(
                gobj,
                -1,
                comment,
                0,
                0,
                kw  // owned
            );
        }
    }

    /*
     *  Delete
     */
    json_t *jn_data = json_array();
    json_array_foreach(iter, idx, node) {
        const char *id = kw_get_str(gobj, node, "id", "", KW_REQUIRED);
        const char *version = kw_get_str(gobj, node, "version", "", KW_REQUIRED);

        if(gobj_delete_node(
                priv->resource, resource, node, json_pack("{s:b}", "force", force), src)<0) {
            json_t *comment = json_sprintf(
                "Cannot delete the configuration: %s %s",
                id, version
            );
            JSON_DECREF(iter)
            return msg_iev_build_response(
                gobj,
                -1,
                comment,
                0,
                0,
                kw  // owned
            );
        }
        json_array_append_new(jn_data, json_string(id));
    }

    JSON_DECREF(iter)

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%d configurations deleted", idx),
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_tag(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    const char *yuno_tag = kw_get_str(gobj, kw, "tag", 0, 0);
    if(!yuno_tag) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("What tag?"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw),  // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Select some yuno please"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Update database, with the same node modified.
     */
    json_t *jn_data = json_array();

    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_object_set_new(node, "yuno_tag", json_string(yuno_tag));
        json_array_append_new(
            jn_data,
            gobj_update_node( // Node updated to collection.
                priv->resource,
                resource,
                json_incref(node),  // You cannot lose old node, belongs to iter.
                json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                src
            )
        );
    }
    json_decref(iter);

    /*
     *  Inform
     */

    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_multiple(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    if(!kw_has_key(kw, "yuno_multiple")) {
        return msg_iev_build_response(gobj,
            -210,
            json_sprintf("What multiple?"),
            0,
            0,
            kw  // owned
        );
    }
    BOOL multiple = kw_get_bool(gobj, kw, "yuno_multiple", 0, 0);
    kw_delete(gobj, kw, "yuno_multiple");

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw),  // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -210,
            json_sprintf("Select some yuno please"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Update database, with the same node modified.
     */
    json_t *jn_data = json_array();

    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_object_set_new(node, "yuno_multiple", json_boolean(multiple));
        json_array_append_new(
            jn_data,
            gobj_update_node( // Node updated to collection.
                priv->resource,
                resource,
                json_incref(node),  // You cannot lose old node, belongs to iter.
                json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                src
            )
        );
    }
    json_decref(iter);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *yuno2multiselect(
    json_t *node // not owned
)
{
    hgobj gobj = 0; // TODO
    json_t * multiselect_element = json_object();
    json_object_set_new(
        multiselect_element,
        "id",
        json_string(kw_get_str(gobj, node, "id", "", KW_REQUIRED))
    );
    char value[NAME_MAX];
    snprintf(value, sizeof(value), "%s^%s",
        kw_get_str(gobj, node, "yuno_role", "", KW_REQUIRED),
        kw_get_str(gobj, node, "yuno_name", "", KW_REQUIRED)
    );
    json_object_set_new(multiselect_element, "value", json_string(value));

    return multiselect_element;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_top_yunos(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL webix = kw_get_bool(gobj, kw, "webix", 0, KW_WILD_NUMBER);
    char *resource = "yunos";

    /*
     *  Get a iter of matched resources
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    json_t *jn_data = json_array();
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        BOOL disabled = kw_get_bool(gobj, node, "yuno_disabled", 0, KW_REQUIRED);
        const char *yuno_tag = kw_get_str(gobj, node, "yuno_tag", 0, KW_REQUIRED);
        if(!disabled || !empty_string(yuno_tag)) {
            json_array_append(jn_data, webix?yuno2multiselect(node):node);
        }
    }
    JSON_DECREF(iter)

    json_t *schema = webix?
        0:tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource)
    ;
    return msg_iev_build_response(
        gobj,
        0,
        0,
        schema,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *yunos2multilselect(
    json_t *iter  // owned
)
{
    json_t *jn_data = json_array();
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_array_append(jn_data, yuno2multiselect(node));
    }
    JSON_DECREF(iter)

    return jn_data;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_yunos(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL webix = kw_get_bool(gobj, kw, "webix", 0, KW_WILD_NUMBER);
    char *resource = "yunos";

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    json_t *jn_data = webix?yunos2multilselect(iter):iter;

    json_t *schema = webix?
        0:tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource)
    ;
    return msg_iev_build_response(
        gobj,
        0,
        0,
        schema,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *  Get numeric version
 ***************************************************************************/
PRIVATE int get_n_v(const char *sversion)
{
    int version = 0;

    int list_size;
    const char **segments = split2(sversion, ".-", &list_size);

    int power = 1;
    for(int i=list_size-1; i>=0; i--, power *=1000) {
        version += atoi(segments[i]) * power;
    }

    split_free2(segments);

    return version;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_find_new_yunos(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL create = kw_get_bool(gobj, kw, "create", 0, KW_WILD_NUMBER);

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        "yunos",
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    json_t *jn_data = json_array();

    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        const char *id = SDATA_GET_ID(yuno);
        const char *realm_id = SDATA_GET_STR(yuno, "realm_id`0");
        const char *yuno_role = SDATA_GET_STR(yuno, "yuno_role");
        const char *yuno_name = SDATA_GET_STR(yuno, "yuno_name");
        const char *role_version = SDATA_GET_STR(yuno, "role_version");
        const char *name_version = SDATA_GET_STR(yuno, "name_version");

        /*
         *  Find a greater config version
         */
        char config_name[NAME_MAX];
        snprintf(config_name, sizeof(config_name), "%s.%s", yuno_role, yuno_name);
        json_t *configs = gobj_list_instances(
            priv->resource,
            "configurations",
            "",
            json_pack("{s:s}", "id", config_name),
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
        json_t *config_found = 0;
        int ix; json_t *config;
        json_array_foreach(configs, ix, config) {
            const char *name_version_ = SDATA_GET_STR(config, "version");
            if(config_found) {
                if(get_n_v(SDATA_GET_STR(config_found, "version")) < get_n_v(name_version_)) {
                    config_found = config;
                }
            } else {
                if(get_n_v(name_version) < get_n_v(name_version_)) {
                    config_found = config;
                }
            }
        }
        json_incref(config_found);
        JSON_DECREF(configs);

        /*
         *  Find a greater role version
         */
        json_t *binaries = gobj_list_instances(
            priv->resource,
            "binaries",
            "",
            json_pack("{s:s}", "id", yuno_role),
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
        json_t *binary_found = 0;
        json_t *binary;
        json_array_foreach(binaries, ix, binary) {
            const char *role_version_ = SDATA_GET_STR(binary, "version");
            if(binary_found) {
                if(get_n_v(SDATA_GET_STR(binary_found, "version")) < get_n_v(role_version_)) {
                    binary_found = binary;
                }
            } else {
                if(get_n_v(role_version) < get_n_v(role_version_)) {
                    binary_found = binary;
                }
            }
        }
        json_incref(binary_found);
        JSON_DECREF(binaries);

        if(!config_found && !binary_found) {
            continue;
        }
        const char *new_name_version = config_found?
            SDATA_GET_STR(config_found, "version"):
            SDATA_GET_STR(yuno, "name_version");

        const char *new_role_version = binary_found?
            SDATA_GET_STR(binary_found, "version"):
            SDATA_GET_STR(yuno, "role_version");

        json_array_append_new(
            jn_data,
            json_sprintf(
                "create-yuno id=%s realm_id=%s yuno_role=%s role_version=%s "
                "yuno_name=%s name_version=%s yuno_tag=%s yuno_multiple=%d",
                id,
                realm_id,
                yuno_role,
                new_role_version,
                yuno_name,
                new_name_version,
                SDATA_GET_STR(yuno, "yuno_tag"),
                SDATA_GET_BOOL(yuno, "yuno_multiple")
            )
        );

        json_decref(binary_found);
        json_decref(config_found);
    }
    json_decref(iter);

    int ret = 0;
    json_t *schema = 0;
    if(create) {
        if(json_array_size(jn_data)) {
            json_t *new_jn_data = json_array();
            int idx; json_t *jn_command;
            json_array_foreach(jn_data, idx, jn_command) {
                const char *command = json_string_value(jn_command);
                json_t *webix = gobj_command(
                    gobj,
                    command,
                    0,
                    gobj
                );
                if(kw_get_int(gobj, webix, "result", 0, KW_REQUIRED)<0) {
                    json_t *c = kw_get_dict_value(gobj, webix, "comment", 0, KW_REQUIRED);
                    json_array_append(new_jn_data, c);
                    ret += -1;
                } else {
                    json_t *d = kw_get_dict_value(gobj, webix, "data", 0, KW_REQUIRED);
                    json_array_extend(new_jn_data, d);
                }
                json_decref(webix);
            }
            json_decref(jn_data);
            jn_data = new_jn_data;
            if(ret == 0) {
                schema = tranger2_list_topic_desc_cols(
                    gobj_read_pointer_attr(priv->resource, "tranger"),
                    "yunos"
                );
            }
        }
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        ret,
        0,
        schema,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
json_t* cmd_create_yuno(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*-----------------------------*
     *      Parameter's filter
     *-----------------------------*/
    const char *realm_id = kw_get_str(gobj, kw, "realm_id", "", 0);
    const char *yuno_role = kw_get_str(gobj, kw, "yuno_role", "", 0);
    const char *yuno_name = kw_get_str(gobj, kw, "yuno_name", "", 0);
    const char *role_version = kw_get_str(gobj, kw, "role_version", "", 0);
    const char *name_version = kw_get_str(gobj, kw, "name_version", "", 0);

    /*---------------------------------------------*
     *      Check Realm
     *---------------------------------------------*/
    json_t *hs_realm = gobj_get_node(
        priv->resource,
        "realms",
        json_pack("{s:s}", "id", realm_id),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );
    if(!hs_realm) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Realm not found: '%s' ", realm_id
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*---------------------------------------------*
     *      Role
     *---------------------------------------------*/
    json_t *hs_binary = find_binary_version(gobj, yuno_role, role_version);
    if(!hs_binary) {
        json_t *comment = json_sprintf(
            "Binary '%s%s%s' not found",
            yuno_role,
            empty_string(role_version)?"":"-",
            empty_string(role_version)?"":role_version
        );
        json_decref(hs_realm);
        return msg_iev_build_response(gobj,
            -1,
            comment,
            0,
            0,
            kw  // owned
        );
    }

    /*---------------------------------------------*
     *      Name
     *---------------------------------------------*/
    json_t *hs_configuration = find_configuration_version(
        gobj,
        SDATA_GET_STR(hs_binary, "id"),
        yuno_name,
        name_version
    );
    if(!hs_configuration) {
        json_t *comment = json_sprintf(
            "Yuno '%s.%s': configuration not found '%s.%s%s%s.json' ",
            yuno_role, yuno_name,
            yuno_role, yuno_name,
            empty_string(name_version)?"":"-",
            empty_string(name_version)?"":name_version
        );
        json_decref(hs_realm);
        json_decref(hs_binary);
        return msg_iev_build_response(gobj,
            -1,
            comment,
            0,
            0,
            kw  // owned
        );
    }

    /*---------------------------------------------*
     *      Release
     *---------------------------------------------*/
    char yuno_release[120];
    build_release_name(yuno_release, sizeof(yuno_release), hs_binary, hs_configuration);
    json_object_set_new(kw, "yuno_release", json_string(yuno_release));

    if(empty_string(role_version)) {
        json_object_set_new(
            kw,
            "role_version",
            json_string(SDATA_GET_STR(hs_binary, "version"))
        );
    }
    if(empty_string(name_version)) {
        json_object_set_new(
            kw,
            "name_version",
            json_string(SDATA_GET_STR(hs_configuration, "version"))
        );
    }

    /*---------------------------------------------*
     *      Check multiple yuno
     *---------------------------------------------*/
    BOOL multiple = kw_get_bool(gobj, kw, "yuno_multiple", 0, 0);
    if(!multiple) {
        /*
         *  Check if already exists
         */
        json_t *kw_find = json_pack("{s:s, s:s, s:s, s:s}",
            "realm_id", realm_id,
            "yuno_role", yuno_role,
            "yuno_name", yuno_name,
            "yuno_release", yuno_release
        );
        json_t *iter_find = gobj_list_nodes(
            priv->resource,
            resource,
            kw_find, // filter
            json_pack("{s:b, s:b}", "include-instances", 1, "only_id", 1),
            src
        );
        if(json_array_size(iter_find)) {
            /*
             *  1 o more records, yuno already stored and without overwrite.
             */
            json_t *jn_data = iter_find;
            json_t *webix = msg_iev_build_response(
                gobj,
                -1,
                json_sprintf(
                    "Yuno already exists"
                ),
                tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
                jn_data,
                kw  // owned
            );
            json_decref(hs_realm);
            json_decref(hs_binary);
            json_decref(hs_configuration);
            return webix;
        }
        JSON_DECREF(iter_find);
    }

    /*---------------------------------------------*
     *      Create the yuno
     *---------------------------------------------*/
    char current_date[22];
    current_timestamp(current_date, sizeof(current_date));  // "CCYY/MM/DD hh:mm:ss"
    json_object_set_new(
        kw,
        "date",
        json_string(current_date)
    );

    json_t *yuno = gobj_create_node(
        priv->resource,
        resource,
        kw_incref(kw),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(!yuno) {
        json_decref(hs_realm);
        json_decref(hs_binary);
        json_decref(hs_configuration);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Cannot create yuno: %s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------*
     *  Link
     *-----------------------------*/
    int result = 0;
    result += gobj_link_nodes(
        priv->resource, "yunos",
        "realms",
        json_incref(hs_realm),
        "yunos",
        json_incref(yuno),
        src
    );
    result += gobj_link_nodes(
        priv->resource, "binary",
        "yunos",
        json_incref(yuno),
        "binaries",
        json_incref(hs_binary),
        src
    );
    result += gobj_link_nodes(
        priv->resource,
        "configurations",
        "yunos",
        json_incref(yuno),
        "configurations",
        json_incref(hs_configuration),
        src
    );
    json_t *iter = gobj_list_instances(
        priv->resource,
        resource,
        "",
        yuno,
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    yuno = json_array_get(iter, 0);
    json_incref(yuno);
    json_decref(iter);

    /*-----------------------------*
     *  Register public services
     *  TODO legacy bug:
     *  en find-new-yunos create=1 podemos crear yunos que todavía no están activos
     *  (hasta deactivate-snap) y sin embargo se actualizan sus datos de public-service
     *-----------------------------*/
    result += register_public_services(gobj, yuno, hs_binary, hs_realm);

    /*
     *  Inform
     */
    json_t *jn_data = json_array();
    json_array_append_new(jn_data, yuno);

    json_decref(hs_realm);
    json_decref(hs_binary);
    json_decref(hs_configuration);

    json_t *webix = msg_iev_build_response(gobj,
        result,
        result<0?
            json_sprintf("%s", gobj_log_last_message()):
            json_sprintf("Created"),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );

    return webix;
}

/***************************************************************************
 *
 ***************************************************************************/
json_t* cmd_delete_yuno(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw),  // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Select some yuno please"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Check conditions to delete
     */
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        BOOL yuno_running = kw_get_bool(gobj, node, "yuno_running", 0, KW_REQUIRED);
        if(yuno_running > 0) {
            json_t *comment = json_sprintf(
                "Cannot delete yuno '%s', it's running",
                kw_get_str(gobj, node, "id", "", KW_REQUIRED)
            );
            JSON_DECREF(iter)
            return msg_iev_build_response(
                gobj,
                -1,
                comment,
                0,
                0,
                kw  // owned
            );
        }
        json_int_t __tag__ = kw_get_int(gobj, node, "__md_treedb__`__tag__", 0, KW_REQUIRED);
        if(__tag__ && !force) {
            json_t *comment = json_sprintf(
                "Cannot delete yuno '%s', it's tagged (%d)",
                kw_get_str(gobj, node, "id", "", KW_REQUIRED),
                (int)__tag__
            );
            JSON_DECREF(iter)
            return msg_iev_build_response(
                gobj,
                -1,
                comment,
                0,
                0,
                kw  // owned
            );
        }
    }

    /*
     *  Delete
     */
    force = 1; // Aquí no manejamos los delete-links, fuerza el delete
    int result = 0;
    int deleted = 0;
    json_array_foreach(iter, idx, node) {
        json_t *kw_delete = json_pack("{s:s}", "id", SDATA_GET_ID(node));
        if(gobj_delete_node(
            priv->resource,
            resource,
            kw_delete,
            json_pack("{s:b}", "force", force),
            src
        )<0) {
            result += -1;
        } else {
            deleted++;
        }
    }
    JSON_DECREF(iter)

    /*
     *  Inform
     */
    json_t *jn_data = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw),  // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    return msg_iev_build_response(
        gobj,
        result,
        result<0?
            json_sprintf("%s", gobj_log_last_message()):
            json_sprintf("%d yunos deleted", deleted),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data,
        kw  // owned
    );

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_run_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*
     *  Get a iter of matched resources.
     */
    json_object_set_new(kw, "yuno_disabled", json_false());
    json_object_set_new(kw, "yuno_running", json_false());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found or already running"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *  Walk over yunos iter:
     *      run
     *      add filter for future counter.
     *------------------------------------------------*/
    json_t *filterlist = json_array();
    int total_run = 0;

    /*
     *  Update database, with the same node modified.
     */
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Run the yuno
         */
        BOOL disabled = kw_get_bool(gobj, yuno, "yuno_disabled", 0, KW_REQUIRED);
        BOOL yuno_running = kw_get_bool(gobj, yuno, "yuno_running", 0, KW_REQUIRED);
        if(!disabled && !yuno_running) {
            int r = run_yuno(gobj, yuno, src);
            if(r==0) {
                const char *id = SDATA_GET_ID(yuno);
                json_t *jn_EvChkItem = json_pack("{s:s, s:{s:s, s:s, s:I}}",
                    "event", EV_ON_OPEN,
                    "filters",
                        "identity_card`realm_id", kw_get_str(gobj, yuno, "realm_id`0", "", KW_REQUIRED),
                        "identity_card`yuno_id", id,
                        "identity_card`launch_id", kw_get_int(gobj, yuno, "launch_id", 0, KW_REQUIRED)
                );
                json_array_append_new(filterlist, jn_EvChkItem);
                if(src != gobj) {
                    json_object_set_new(yuno, "solicitante", json_string(gobj_name(src)));
                } else {
                    json_object_set_new(yuno, "solicitante", json_string(""));
                }

                // Volatil if you don't want historic data
                // TODO legacy force volatil, sino no aparece el yuno con mas release el primero
                // y falla el deactivate-snap
                json_decref(
                    gobj_update_node(
                        priv->resource,
                        resource,
                        json_incref(yuno),
                        json_pack("{s:b}", "volatil", 1),
                        src
                    )
                );
                total_run++;
            } else {
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_INTERNAL_ERROR,
                    "msg",              "%s", "run_yuno() FAILED",
                    "ret",              "%d", r,
                    NULL
                );
            }

        }
    }

    if(!total_run) {
        JSON_DECREF(filterlist);
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found to run"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*--------------------------------------*
     *  Crea con counter un futuro
     *  que nos indique cuando han arrancado
     *  all yunos arrancados.
     *--------------------------------------*/
    json_t *kw_answer = kw_incref(kw);

    char info[80];
    snprintf(info, sizeof(info), "%d yunos found to run", total_run);
    json_t *kw_counter = json_pack("{s:s, s:i, s:I, s:o, s:{s:o, s:o}}",
        "info", info,
        "max_count", total_run,
        "expiration_timeout", priv->timeout_expiration,
        "input_schema", filterlist, // owned
        "__user_data__",
            "iter", iter,                   // HACK free en diferido, en ac_final_count()
            "kw_answer", kw_answer          // HACK free en diferido, en ac_final_count()
    );

    hgobj gobj_counter = gobj_create("", C_COUNTER, kw_counter, gobj);

    /*
     *  Subcribe al objeto counter a los eventos del router
     */
    json_t *kw_sub = json_pack("{s:{s:s}}",
        "__config__", "__rename_event_name__", "EV_COUNT"
    );
    gobj_subscribe_event(
        gobj_find_service("__input_side__", TRUE),
        EV_ON_OPEN,
        kw_sub,
        gobj_counter
    );

// KKK
    /*
     *  Subcribeme a mí al evento final del counter, con msg_id
     *  msg_id: con que me diga quien es el requester de este comando me vale
     *  HACK: Meto un msg_id en la subscripción al counter.
     *  Así en la publicación recibida recuperamos el msg_id que contiene el 'requester'
     *  que pusimos.
     *  Además le decimos al counter que se suscriba al evento EV_ON_OPEN del router,
     *  pero diciendo que reciba un rename, EV_COUNT, que es el que está definido en la máquina.
     *  Con los filtros le decimos que cuente los eventos recibidos que además
     *  cumplan con los filtros pasados. Es decir, identificamos, entre los posible multiples
     *  eventos recibidos en la publicación, justo al evento que queremos.
     */
    json_t *kw_final_count = json_object();
    if(src != gobj) {
        // Si la petición no viene del propio agente, guarda al requester
        json_t *global = json_object();
        json_object_set_new(kw_final_count, "__global__", global);
        json_t *jn_msg_id = json_pack("{s:s}",
            "requester", gobj_name(src)
        );
        msg_iev_push_stack(
            gobj,
            global,
            "requester_stack",
            jn_msg_id
        );
    }

    gobj_subscribe_event(gobj_counter, "EV_FINAL_COUNT", kw_final_count, gobj);

    gobj_start(gobj_counter);

    KW_DECREF(kw);
    return 0;   // Asynchronous response
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_kill_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    BOOL app = kw_get_bool(gobj, kw, "app", 0, 0);
    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    /*
     *  Get a iter of matched resources.
     */
    json_object_set_new(kw, "yuno_running", json_true());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found or already not running"
            ),
            0,
            0,
            kw  // owned
        );
    }

    int prev_signal2kill = 0;
    if(force) {
        prev_signal2kill = gobj_read_integer_attr(gobj, "signal2kill");
        gobj_write_integer_attr(gobj, "signal2kill", SIGKILL);
    }

    /*------------------------------------------------*
     *  Walk over yunos iter:
     *      kill
     *      add filter for future counter.
     *------------------------------------------------*/
    json_t *filterlist = json_array();
    int total_killed = 0;
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Kill the yuno
         */
        BOOL yuno_running = kw_get_bool(gobj, yuno, "yuno_running", 0, KW_REQUIRED);
        const char *id = SDATA_GET_ID(yuno);
        if(app && atoi(id) < 1000) {
            continue;
        }
        if(yuno_running) {
            if(kill_yuno(gobj, yuno)==0) {
                json_int_t channel_gobj = (json_int_t)(uintptr_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
                json_t *jn_EvChkItem = json_pack("{s:s, s:{s:I}}",
                    "event", EV_ON_CLOSE,
                    "filters",
                        "__temp__`channel_gobj", channel_gobj
                );
                json_array_append_new(filterlist, jn_EvChkItem);
                total_killed++;
            } else {
                if(force) {
                    gobj_write_integer_attr(gobj, "signal2kill", prev_signal2kill);
                }
                JSON_DECREF(iter)
                JSON_DECREF(filterlist);
                return msg_iev_build_response(gobj,
                    -1,
                    json_sprintf(
                        "Can't kill yuno: %s", gobj_log_last_message()
                    ),
                    0,
                    0,
                    kw  // owned
                );
            }
        }
    }

    if(force) {
        gobj_write_integer_attr(gobj, "signal2kill", prev_signal2kill);
    }

    if(!total_killed) {
        JSON_DECREF(iter)
        JSON_DECREF(filterlist);
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found to kill"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*--------------------------------------*
     *  Crea con counter un futuro
     *  que nos indique cuando han arrancado
     *  all yunos arrancados.
     *--------------------------------------*/
    KW_INCREF(kw);
    json_t *kw_answer = kw;

    char info[80];
    snprintf(info, sizeof(info), "%d yunos found to kill", total_killed);
    json_t *kw_counter = json_pack("{s:s, s:i, s:I, s:o, s:{s:o, s:o}}",
        "info", info,
        "max_count", total_killed,
        "expiration_timeout", priv->timeout_expiration,
        "input_schema", filterlist, // owned
        "__user_data__",
            "iter", iter,                   // HACK free en diferido, en ac_final_count()
            "kw_answer", kw_answer          // HACK free en diferido, en ac_final_count()
    );

    hgobj gobj_counter = gobj_create("", C_COUNTER, kw_counter, gobj);

    json_t *kw_sub = json_pack("{s:{s:s}}",
        "__config__", "__rename_event_name__", "EV_COUNT"
    );

    /*
     *  Subcribe al objeto counter a los eventos del router
     */
    gobj_subscribe_event(
        gobj_find_service("__input_side__", TRUE),
        EV_ON_CLOSE,
        kw_sub,
        gobj_counter
    );

// KKK
    /*
     *  Subcribeme a mí al evento final del counter, con msg_id
     *  msg_id: con que me diga quien es el requester de este comando me vale
     */
    json_t *kw_final_count = json_object();
    if(src != gobj) {
        // Si la petición no viene del propio agente, guarda al requester
        json_t *global = json_object();
        json_object_set_new(kw_final_count, "__global__", global);
        json_t *jn_msg_id = json_pack("{s:s}",
            "requester", gobj_name(src)
        );
        msg_iev_push_stack(
            gobj,
            global,
            "requester_stack",
            jn_msg_id
        );
    }

    gobj_subscribe_event(gobj_counter, "EV_FINAL_COUNT", kw_final_count, gobj);

    gobj_start(gobj_counter);

    KW_DECREF(kw);
    return 0;   // Asynchronous response
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_play_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_object_set_new(kw, "yuno_disabled", json_false());
    json_object_set_new(kw, "yuno_playing", json_false());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found or already playing"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *  Walk over yunos iter:
     *      play
     *      add filter for future counter.
     *------------------------------------------------*/
    json_t *filterlist = json_array();
    int total_already_playing = 0;
    int total_to_played = 0;
    int total_to_preplayed = 0;

    /*
     *  Update database, with the same node modified.
     */
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Play the yuno
         */
        if(!kw_get_bool(gobj, yuno, "must_play", 0, KW_REQUIRED)) {
            json_object_set_new(yuno, "must_play", json_true());
            json_decref(gobj_update_node(priv->resource, resource, json_incref(yuno), 0, src));
            total_to_preplayed++;
        }

        BOOL yuno_running = kw_get_bool(gobj, yuno, "yuno_running", 0, KW_REQUIRED);
        if(!yuno_running) {
            continue;
        }
        BOOL yuno_playing = kw_get_bool(gobj, yuno, "yuno_playing", 0, KW_REQUIRED);
        if(!yuno_playing) {
            /*
             *  HACK le meto un id al mensaje de petición PLAY_YUNO
             *  que lo devolverá en el mensaje respuesta PLAY_YUNO_ACK.
             */
            json_int_t filter_ref = (json_int_t)long_reference();
            json_t *jn_msg = json_object();
            kw_set_subdict_value(gobj, jn_msg, "__md_iev__", "__id__", json_integer(filter_ref));
            if(play_yuno(gobj, yuno, jn_msg, src)==0) {
                /*
                 *  HACK Guarda el filtro para el counter.
                 *  Realmente solo se necesita para informar al cliente
                 *  solo después de que se hayan ejecutado sus ordenes.
                 */
                json_t *jn_EvChkItem = json_pack("{s:s, s:{s:I}}",
                    "event", "EV_PLAY_YUNO_ACK",
                    "filters",
                        "__md_iev__`__id__", (json_int_t)filter_ref
                );
                json_array_append_new(filterlist, jn_EvChkItem);
                total_to_played++;
            }
        } else {
            total_already_playing++;
        }
    }

    if(!total_to_played && !total_to_preplayed) {
        JSON_DECREF(iter)
        JSON_DECREF(filterlist);
        return msg_iev_build_response(gobj,
            0,
            json_sprintf(
                "Yuno not found to play"
            ),
            0,
            0,
            kw  // owned
        );
    }
    if(!total_to_played) {
        JSON_DECREF(iter)
        JSON_DECREF(filterlist);
        return msg_iev_build_response(gobj,
            0,
            json_sprintf(
                "%d yunos found to preplay",
                total_to_preplayed
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*--------------------------------------*
     *  Crea con counter un futuro
     *  que nos indique cuando han arrancado
     *  all yunos arrancados.
     *--------------------------------------*/
    KW_INCREF(kw);
    json_t *kw_answer = kw;

    char info[80];
    snprintf(info, sizeof(info), "%d to preplay, %d to play",
        total_to_preplayed,
        total_to_played
    );

    json_t *kw_counter = json_pack("{s:s, s:i, s:I, s:o, s:{s:o, s:o}}",
        "info", info,
        "max_count", total_to_played,
        "expiration_timeout", priv->timeout_expiration,
        "input_schema", filterlist, // owned
        "__user_data__",
            "iter", iter,                   // HACK free en diferido, en ac_final_count()
            "kw_answer", kw_answer          // HACK free en diferido, en ac_final_count()
    );

    hgobj gobj_counter = gobj_create("", C_COUNTER, kw_counter, gobj);
    json_t *kw_sub = json_pack("{s:{s:s}}",
        "__config__", "__rename_event_name__", "EV_COUNT"
    );

    /*
     *  Subcribe al objeto counter a los eventos del router
     */
    gobj_subscribe_event(
        gobj,
        "EV_PLAY_YUNO_ACK",
        kw_sub,
        gobj_counter
    );

// KKK
    /*
     *  Subcribeme a mí al evento final del counter, con msg_id
     *  msg_id: con que me diga quien es el requester de este comando me vale
     */
    json_t *kw_final_count = json_object();
    if(src != gobj) {
        // Si la petición no viene del propio agente, guarda al requester
        json_t *global = json_object();
        json_object_set_new(kw_final_count, "__global__", global);
        json_t *jn_msg_id = json_pack("{s:s}",
            "requester", gobj_name(src)
        );
        msg_iev_push_stack(
            gobj,
            global,
            "requester_stack",
            jn_msg_id
        );
    }

    gobj_subscribe_event(gobj_counter, "EV_FINAL_COUNT", kw_final_count, gobj);

    gobj_start(gobj_counter);

    KW_DECREF(kw);
    return 0;   // Asynchronous response
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_pause_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_object_set_new(kw, "yuno_disabled", json_false());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found or already in pause"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *  Walk over yunos iter:
     *      pause
     *      add filter for future counter.
     *------------------------------------------------*/
    json_t *filterlist = json_array();
    int total_already_pausing = 0;
    int total_to_paused = 0;
    int total_to_prepaused = 0;

    /*
     *  Update database, with the same node modified.
     */
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Pause the yuno
         */
        if(kw_get_bool(gobj, yuno, "must_play", 0, KW_REQUIRED)) {
            json_object_set_new(yuno, "must_play", json_false());
            json_decref(gobj_update_node(priv->resource, resource, json_incref(yuno), 0, src));
            total_to_prepaused++;
        }
        BOOL yuno_playing = kw_get_bool(gobj, yuno, "yuno_playing", 0, KW_REQUIRED);
        if(yuno_playing) {
            json_int_t filter_ref = (json_int_t)long_reference();
            json_t *jn_msg = json_object();
            kw_set_dict_value(gobj, jn_msg, "__md_iev__`__id__", json_integer(filter_ref));
            if(pause_yuno(gobj, yuno, jn_msg, src)==0) {
                json_t *jn_EvChkItem = json_pack("{s:s, s:{s:I}}",
                    "event", "EV_PAUSE_YUNO_ACK",
                    "filters",
                        "__md_iev__`__id__", (json_int_t)filter_ref
                );
                json_array_append_new(filterlist, jn_EvChkItem);
                total_to_paused++;
            }
        } else {
            total_already_pausing++;
        }
    }

    if(!total_to_paused && !total_to_prepaused) {
        JSON_DECREF(filterlist);
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            0,
            json_sprintf(
                "Yuno not found to pause"
            ),
            0,
            0,
            kw  // owned
        );
    }
    if(!total_to_paused) {
        JSON_DECREF(iter)
        JSON_DECREF(filterlist);
        return msg_iev_build_response(gobj,
            0,
            json_sprintf(
                "%d yunos found to prepause",
                total_to_prepaused
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*--------------------------------------*
     *  Crea con counter un futuro
     *  que nos indique cuando han arrancado
     *  all yunos arrancados.
     *--------------------------------------*/
    KW_INCREF(kw);
    json_t *kw_answer = kw;

    char info[80];
    snprintf(info, sizeof(info), "%d to prepause, %d to pause",
        total_to_prepaused,
        total_to_paused
    );
    json_t *kw_counter = json_pack("{s:s, s:i, s:I, s:o, s:{s:o, s:o}}",
        "info", info,
        "max_count", total_to_paused,
        "expiration_timeout", priv->timeout_expiration,
        "input_schema", filterlist, // owned
        "__user_data__",
            "iter", iter,                   // HACK free en diferido, en ac_final_count()
            "kw_answer", kw_answer          // HACK free en diferido, en ac_final_count()
    );

    hgobj gobj_counter = gobj_create("", C_COUNTER, kw_counter, gobj);
    json_t *kw_sub = json_pack("{s:{s:s}}",
        "__config__", "__rename_event_name__", "EV_COUNT"
    );

    /*
     *  Subcribe al objeto counter a los eventos del router
     */
    gobj_subscribe_event(
        gobj,
        "EV_PAUSE_YUNO_ACK",
        kw_sub,
        gobj_counter
    );

// KKK
    /*
     *  Subcribeme a mí al evento final del counter, con msg_id
     *  msg_id: con que me diga quien es el requester de este comando me vale
     */
    json_t *kw_final_count = json_object();
    if(src != gobj) {
        // Si la petición no viene del propio agente, guarda al requester
        json_t *global = json_object();
        json_object_set_new(kw_final_count, "__global__", global);
        json_t *jn_msg_id = json_pack("{s:s}",
            "requester", gobj_name(src)
        );
        msg_iev_push_stack(
            gobj,
            global,
            "requester_stack",
            jn_msg_id
        );
    }

    gobj_subscribe_event(gobj_counter, "EV_FINAL_COUNT", kw_final_count, gobj);

    gobj_start(gobj_counter);

    KW_DECREF(kw);
    return 0;   // Asynchronous response
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_enable_yuno(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_object_set_new(kw, "yuno_disabled", json_true());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found or already enabled"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Update database, with the same node modified.
     */
    json_t *jn_data = json_array();

    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        /*
         *  Enable yuno
         */
        json_object_set_new(node, "yuno_disabled", json_false());

        json_array_append_new(
            jn_data,
            gobj_update_node( // Node updated to collection.
                priv->resource,
                resource,
                json_incref(node),  // You cannot lose old node, belongs to iter.
                json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                src
            )
        );
    }
    json_decref(iter);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_disable_yuno(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_object_set_new(kw, "yuno_disabled", json_false());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found or already disabled"
            ),
            0,
            0,
            kw  // owned
        );
    }

    // Force kill
    int prev_signal2kill = gobj_read_integer_attr(gobj, "signal2kill");
    gobj_write_integer_attr(gobj, "signal2kill", SIGKILL);

    /*
     *  Update database, with the same node modified.
     */
    json_t *jn_data = json_array();

    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        /*
         *  Disable node
         */
        BOOL disabled = kw_get_bool(gobj, node, "yuno_disabled", 0, KW_REQUIRED);
        if(!disabled) {
            BOOL playing = kw_get_bool(gobj, node, "yuno_playing", 0, KW_REQUIRED);
            if(playing) {
                pause_yuno(gobj, node, 0, src);
            }
            BOOL running = kw_get_bool(gobj, node, "yuno_running", 0, KW_REQUIRED);
            if(running) {
                kill_yuno(gobj, node);
            }

            json_object_set_new(node, "yuno_disabled", json_true());

            json_array_append_new(
                jn_data,
                gobj_update_node( // Node updated to collection.
                    priv->resource,
                    resource,
                    json_incref(node),  // You cannot lose old node, belongs to iter.
                    json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                    src
                )
            );
        }
    }
    json_decref(iter);

    // Restore kill
    gobj_write_integer_attr(gobj, "signal2kill", prev_signal2kill);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_trace_on_yuno(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_object_set_new(kw, "traced", json_false());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found or already tracing"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Update database, with the same node modified.
     */
    json_t *jn_data = json_array();

    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Trace on yuno
         */
        json_object_set_new(yuno, "traced", json_true());
        json_t *kw_clone = json_deep_copy(kw);
        (void)msg_iev_clean_metadata(kw_clone);
        trace_on_yuno(gobj, yuno, kw_clone, src);

        json_array_append_new(
            jn_data,
            gobj_update_node( // Node updated to collection.
                priv->resource,
                resource,
                json_incref(yuno),  // You cannot lose old node, belongs to iter.
                json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                src
            )
        );
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_trace_off_yuno(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_object_set_new(kw, "traced", json_true());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found or already not tracing"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Update database, with the same node modified.
     */
    json_t *jn_data = json_array();

    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Trace off yuno
         */
        json_object_set_new(yuno, "traced", json_false());
        json_t *kw_clone = json_deep_copy(kw);
        (void)msg_iev_clean_metadata(kw_clone);
        trace_off_yuno(gobj, yuno, kw_clone, src);

        json_array_append_new(
            jn_data,
            gobj_update_node( // Node updated to collection.
                priv->resource,
                resource,
                json_incref(yuno),  // You cannot lose old node, belongs to iter.
                json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                src
            )
        );
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_command_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    json_t *jn_command = kw_get_dict_value(gobj, kw, "command", 0, KW_EXTRACT);
    if(empty_string(json_string_value(jn_command))) {
        JSON_DECREF(jn_command);
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "What command?"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_object_set_new(kw, "yuno_disabled", json_false());
    json_object_set_new(kw, "yuno_running", json_true());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        JSON_DECREF(jn_command);
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Send command
     *------------------------------------------------*/
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Command to yuno
         */
        json_t *kw_yuno = json_deep_copy(kw);
        command_to_yuno(gobj, yuno, json_string_value(jn_command), kw_yuno, src);
    }
    JSON_DECREF(iter)
    JSON_DECREF(jn_command);

    KW_DECREF(kw);
    return 0;   /* Asynchronous response */
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_stats_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    const char *stats = kw_get_str(gobj, kw, "stats", "", 0);

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_object_set_new(kw, "yuno_disabled", json_false());
    json_object_set_new(kw, "yuno_running", json_true());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Send stats
     *------------------------------------------------*/
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Command to yuno
         */
        json_t *kw_yuno = json_deep_copy(kw);
        stats_to_yuno(gobj, yuno, stats, kw_yuno, src);
    }
    JSON_DECREF(iter)

    KW_DECREF(kw);
    return 0;   /* Asynchronous response */
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_object_set_new(kw, "yuno_disabled", json_false());
    json_object_set_new(kw, "yuno_running", json_true());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Yuno not found"
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Send authzs
     *------------------------------------------------*/
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Command to yuno
         */
        json_t *kw_yuno = json_deep_copy(kw);
        authzs_to_yuno(yuno, kw_yuno, src);
    }
    JSON_DECREF(iter)

    KW_DECREF(kw);
    return 0;   /* Asynchronous response */
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_command_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *command = kw_get_str(gobj, kw, "command", 0, 0);
    if(empty_string(command)) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "What command?"
            ),
            0,
            0,
            kw  // owned
        );
    }
    if(strcasecmp(command, cmd)==0) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "Loop command"
            ),
            0,
            0,
            kw  // owned
        );
    }
    const char *service = kw_get_str(gobj, kw, "service", "", 0);

    hgobj service_gobj;
    if(empty_string(service)) {
        service_gobj = gobj_default_service();
    } else {
        service_gobj = gobj_find_service(service, FALSE);
        if(!service_gobj) {
            return msg_iev_build_response(gobj,
                -1,
                json_sprintf("Service '%s' not found", service),
                0,
                0,
                kw  // owned
            );
        }
    }

    json_t *webix = gobj_command(
        service_gobj,
        command,
        kw, // owned
        src
    );
    return webix;

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_stats_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *stats = kw_get_str(gobj, kw, "stats", "", 0);
    const char *service = kw_get_str(gobj, kw, "service", "", 0);

    hgobj service_gobj;
    if(empty_string(service)) {
        service_gobj = gobj_default_service();
    } else {
        service_gobj = gobj_find_service(service, FALSE);
        if(!service_gobj) {
            return msg_iev_build_response(gobj,
                -1,
                json_sprintf("Service '%s' not found", service),
                0,
                0,
                kw  // owned
            );
        }
    }

    json_t *webix = gobj_stats(
        service_gobj,
        stats,
        kw, // owned
        src
    );
    return webix;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_okill(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    gobj_write_integer_attr(gobj, "signal2kill", SIGQUIT);
    return msg_iev_build_response(gobj,
        0,
        json_sprintf("Set kill mode = ordered (with SIGQUIT)"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_qkill(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    gobj_write_integer_attr(gobj, "signal2kill", SIGKILL);
    return msg_iev_build_response(gobj,
        0,
        json_sprintf("Set kill mode = quick (with SIGKILL)"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_check_json(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int max_refcount = kw_get_int(gobj, kw, "max_refcount", 1, KW_WILD_NUMBER);

    json_t *tranger = gobj_read_pointer_attr(priv->resource, "tranger");
    int result = 0;
    json_check_refcounts(tranger, max_refcount, &result)?0:-1;
    return msg_iev_build_response(gobj,
        result,
        json_sprintf("check refcounts of tranger: %s", result==0?"Ok":"Bad"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_snap_content(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return gobj_command(priv->resource, cmd, kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_snaps(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return gobj_command(priv->resource, cmd, kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_shoot_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return gobj_command(priv->resource, cmd, kw, src);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_activate_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_command(priv->resource, cmd, kw, src);
    int ret = kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    if(ret>=0) {
        ret = restart_nodes(gobj);
    }
    return jn_resp;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_deactivate_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_command(priv->resource, cmd, kw, src);
    int ret = kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    if(ret>=0) {
        ret = restart_nodes(gobj);
    }
    return jn_resp;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_realms_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "realms";

    /*
     *  Get a iter of matched resources
     */
    json_t *jn_data = gobj_list_instances(
        priv->resource,
        resource,
        "",
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s", cmd),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_yunos_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*
     *  Get a iter of matched resources.
     */
    json_t *jn_data = gobj_list_instances(
        priv->resource,
        resource,
        "",
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s", cmd),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_binaries_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "binaries";

    /*
     *  Get a iter of matched resources
     */
    json_t *jn_data = gobj_list_instances(
        priv->resource,
        resource,
        "",
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s", cmd),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_configs_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "configurations";

    /*
     *  Get a iter of matched resources
     */
    json_t *jn_data = gobj_list_instances(
        priv->resource,
        resource,
        "",
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s", cmd),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_public_services_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "public_services";

    /*
     *  Get a iter of matched resources
     */
    json_t *jn_data = gobj_list_instances(
        priv->resource,
        resource,
        "",
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s", cmd),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_consoles(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "list-consoles";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  List consoles
     *----------------------------------------*/
    int result = 0;
    json_t *jn_data = json_object();

    const char *name; json_t *jn_console;
    json_object_foreach(priv->list_consoles, name, jn_console) {
        json_t *jn_dup_console = json_deep_copy(jn_console);
        json_object_set_new(jn_data, name, jn_dup_console);

        json_t *jn_routes = kw_get_dict(gobj, jn_dup_console, "routes", 0, KW_REQUIRED);
        json_t *jn_gobjs = kw_get_dict(gobj, jn_dup_console, "gobjs", json_object(), KW_CREATE);

        const char *route_name; json_t *jn_route;
        json_object_foreach(jn_routes, route_name, jn_route) {
            const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
            const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
            hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
            if(gobj_route_service) {
                hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
                if(!gobj_input_gate) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "no route child found",
                        "service",      "%s", route_service,
                        "child",        "%s", route_child,
                        NULL
                    );
                    json_object_set_new(jn_gobjs, route_name, json_string("ERROR route_child not found"));
                    result = -1;
                    continue;
                }
                json_t *jn_consoles = gobj_kw_get_user_data(gobj_input_gate, "consoles", 0, 0);
                json_object_set_new(jn_gobjs, route_name, json_deep_copy(jn_consoles));
            } else {
                json_object_set_new(jn_gobjs, route_name, json_string("ERROR route_service not found"));
                result = -1;
            }
        }
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        result,
        json_sprintf("==> List consoles of agent: '%s'", node_uuid()),
        0,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_open_console(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "open-console";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Open console
     *----------------------------------------*/
    const char *name = kw_get_str(gobj, kw, "name", "", 0);
    const char *process = kw_get_str(gobj, kw, "process", "bash", 0);
    const char *cwd = kw_get_str(gobj, kw, "cwd", "/home/yuneta", 0);
    BOOL hold_open = kw_get_bool(gobj, kw, "hold_open", 0, KW_WILD_NUMBER);
    int cx = kw_get_int(gobj, kw, "cx", 80, KW_WILD_NUMBER);
    int cy = kw_get_int(gobj, kw, "cy", 24, KW_WILD_NUMBER);

    if(empty_string(name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What console name?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(process)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What process?"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get a iter of matched resources
     */
    hgobj gobj_console = 0;

    if(!kw_has_key(priv->list_consoles, name)) {
        /*
         *  New console
         */
        if(kw_size(priv->list_consoles) > gobj_read_integer_attr(gobj, "max_consoles")) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf(
                    "Too much opened consoles: %d",
                    (int)kw_size(priv->list_consoles)
                ),
                0,
                0,
                kw  // owned
            );
        }

        /*
         *  Create pseudoterminal
         */
        json_t *kw_pty = json_pack("{s:s, s:s, s:i, s:i}",
            "process", process,
            "cwd", cwd,
            "cols", cx,
            "rows", cy
        );
        gobj_console = gobj_create_service(name, C_PTY, kw_pty, gobj);
        if(!gobj_console) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Cannot open console: '%s'", name),
                0,
                0,
                kw  // owned
            );
        }
        gobj_set_volatil(gobj_console, TRUE);

        /*
         *  Save console
         */
        json_t *jn_console = json_pack("{s:s, s:b, s:{}}",
            "process", process,
            "hold_open", hold_open,
            "routes"
        );

        add_console_route(gobj, name, jn_console, src, kw);

        json_object_set(priv->list_consoles, name, jn_console); // save in local list

        json_decref(jn_console);

        gobj_start(gobj_console);

    } else {
        /*
         *  Console already exists
         */
        json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, name, 0, KW_REQUIRED);
        gobj_console = gobj_find_service(name, FALSE);
        if(!gobj_console) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Console gobj not found: '%s'", name),
                0,
                0,
                kw  // owned
            );
        }
        int ret = add_console_route(gobj, name, jn_console, src, kw);
        if(ret < 0) {
            if(ret == -2) {
                return msg_iev_build_response(
                    gobj,
                    -1,
                    json_sprintf("Console already open: '%s'", name),
                    0,
                    0,
                    kw  // owned
                );
            } else {
                return msg_iev_build_response(
                    gobj,
                    -1,
                    json_sprintf("Error opening console: '%s'", name),
                    0,
                    0,
                    kw  // owned
                );
            }
        }
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        json_sprintf("Console opened: '%s'", name),  // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_close_console(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "close-console";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }


    /*----------------------------------------*
     *  Close console
     *----------------------------------------*/
    const char *name = kw_get_str(gobj, kw, "name", "", 0);
    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    if(empty_string(name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What console name?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, name, 0, 0);
    if(!jn_console) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Console not found: '%s'", name),
            0,
            0,
            kw  // owned
        );
    }

    BOOL hold_open = kw_get_bool(gobj, jn_console, "hold_open", 0, KW_REQUIRED);
    if(force) {
        hold_open = FALSE;
    }

    /*
     *  Delete console or route
     */
    int ret = 0;
    if(hold_open) {
        const char *route_service = gobj_name(gobj_nearest_top_service(src));
        const char *route_child = gobj_name(src);
        ret = remove_console_route(gobj, name, route_service, route_child);
    } else {
        hgobj gobj_console = gobj_find_service(name, TRUE);
        gobj_stop(gobj_console); // volatil, auto-destroy
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        ret,
        json_sprintf("Console closed: '%s'", name),
        0,
        0, // owned
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/*****************************************************************
 *
 *****************************************************************/
PRIVATE BOOL is_yuneta_agent(pid_t pid)
{
    int ret = kill(pid, 0);
    if(ret == 0) {
        char cmdline[1024];
        read_process_cmdline(cmdline, sizeof(cmdline), pid);
        if(strstr(cmdline, " yagent ")) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void remove_pid_file(void)
{
    unlink(pidfile);
}

/***************************************************************************
 *      long age reference
 ***************************************************************************/
PRIVATE uint64_t long_reference(void)
{
    static uint64_t __long_reference__ = 0;
    return ++__long_reference__;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_console_route(
    hgobj gobj,
    const char *name,
    json_t *jn_console_,
    hgobj src,
    json_t *kw
)
{
    json_t *jn_routes = kw_get_dict(gobj, jn_console_, "routes", 0, KW_REQUIRED);

    const char *route_service = gobj_name(gobj_nearest_top_service(src));
    const char *route_child = gobj_name(src);

    char route_name[NAME_MAX];
    snprintf(route_name, sizeof(route_name), "%s.%s", route_service, route_child);

    if(kw_has_key(jn_routes, route_name)) {
        return -2;
    }

    /*
     *  add in local list
     */
    json_t *jn_route = json_pack("{s:s, s:s, s:O}",
        "route_service", route_service,
        "route_child", route_child,
        "__md_iev__", kw_get_dict(gobj, kw, "__md_iev__", 0, KW_REQUIRED)
    );
    if(!jn_route) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "cannot create route",
            "route_service","%s", route_service,
            "route_child",  "%s", route_child,
            "kw",           "%j", kw,
            NULL
        );
        return -1;
    }


    json_object_set_new(jn_routes, route_name, jn_route);

    /*
     *  add in input gate
     */
    return add_console_in_input_gate(gobj, name, src);
}

/***************************************************************************
 *  Delete route in local list and input gate
 ***************************************************************************/
PRIVATE int remove_console_route(
    hgobj gobj,
    const char *name,
    const char *route_service,
    const char *route_child
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_console_ = kw_get_dict(gobj, priv->list_consoles, name, 0, 0);
    json_t *jn_routes = kw_get_dict(gobj, jn_console_, "routes", 0, KW_REQUIRED);

    char route_name[NAME_MAX];
    snprintf(route_name, sizeof(route_name), "%s.%s", route_service, route_child);

    /*
     *  delete in local list
     */
    if(kw_has_key(jn_routes, route_name)) {
        json_object_del(jn_routes, route_name);
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "route not exist in local list",
            "name",         "%s", route_name,
            NULL
        );
    }

    /*
     *  delete in input gate
     */
    hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
    if(gobj_route_service) {
        hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
        if(gobj_input_gate) {
            json_t *consoles = gobj_kw_get_user_data(gobj_input_gate, "consoles", 0, 0);
            if(consoles) {
                json_object_del(consoles, route_name);
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "no route found in child gobj",
                    "route_name",   "%s", route_name,
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
            }
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "no route child gobj found",
                "service",      "%s", route_service,
                "child",        "%s", route_child,
                NULL
            );
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_console_in_input_gate(hgobj gobj, const char *name, hgobj src)
{
    char name_[NAME_MAX];
    snprintf(name_, sizeof(name_), "consoles`%s", name);
    gobj_kw_get_user_data( // save in input gate
        src,
        name_,
        json_true(), // owned
        KW_CREATE
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int delete_console(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  delete in local list
     */
    json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, name, 0, KW_EXTRACT);

    hgobj gobj_console = gobj_find_service(name, FALSE);

    if(!jn_console) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "no console conf found",
            "name",         "%s", name,
            NULL
        );
        if(gobj_console) {
            gobj_stop(gobj_console); // volatil, auto-destroy
        }
        return -1;
    }

    /*
     *  delete routes in input gates
     */
    json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);

    const char *route; json_t *jn_route; void *n;
    json_object_foreach_safe(jn_routes, n, route, jn_route) {
        const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
        const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
        hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
        if(gobj_route_service) {
            hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
            if(!gobj_input_gate) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "no route child found",
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
                continue;
            }
            json_t *consoles = gobj_kw_get_user_data(gobj_input_gate, "consoles", 0, 0);
            if(consoles) {
                json_object_del(consoles, name);
            }

        }
    }

    if(gobj_console) {
        gobj_stop(gobj_console); // volatil, auto-destroy
    }
    json_decref(jn_console);

    return 0;
}

/***************************************************************************
 *  From input gate
 ***************************************************************************/
PRIVATE int delete_consoles_on_disconnection(hgobj gobj, json_t *kw, hgobj src_)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj gobj_channel = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    json_t *consoles = gobj_kw_get_user_data(gobj_channel, "consoles", 0, 0);
    if(!consoles) {
        return 0;
    }

    const char *route_service = gobj_name(gobj_nearest_top_service(gobj_channel));
    const char *route_child = gobj_name(gobj_channel);

    const char *name; json_t *jn_; void *n;
    json_object_foreach_safe(consoles, n, name, jn_) {
        json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, name, 0, 0);

        BOOL hold_open = kw_get_bool(gobj, jn_console, "hold_open", 0, 0);
        if(hold_open) {
            remove_console_route(gobj, name, route_service, route_child);
        } else {
            delete_console(gobj, name);
        }
    }

    return 0;
}

/***************************************************************************
 *      Execute command
 ***************************************************************************/
PRIVATE int exec_startup_command(hgobj gobj)
{
    const char *startup_command = gobj_read_str_attr(gobj, "startup_command");
    if(!empty_string(startup_command)) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_STARTUP,
            "msg",          "%s", "exec_startup_command",
            "cmd",          "%s", startup_command,
            NULL
        );
        if(system(startup_command)!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "system() FAILED",
                "script",       "%s", startup_command,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int build_role_plus_name(char *bf, int bf_len, json_t *yuno)
{
    hgobj gobj = 0; // TODO
    const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);
    const char *yuno_name = kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED);

    if(empty_string(yuno_name)) {
        snprintf(bf, bf_len, "%s",
            yuno_role
        );
    } else {
        snprintf(bf, bf_len, "%s^%s",
            yuno_role,
            yuno_name
        );
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int build_role_plus_id(char *bf, int bf_len, json_t *yuno)
{
    hgobj gobj = 0; // TODO
    const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);
    const char *yuno_name = kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED);
    const char *yuno_id = kw_get_str(gobj, yuno, "id", yuno_name, KW_REQUIRED);

    if(empty_string(yuno_id)) {
        snprintf(bf, bf_len, "%s",
            yuno_role
        );
    } else {
        snprintf(bf, bf_len, "%s^%s",
            yuno_role,
            yuno_id
        );
    }
    return 0;
}

/***************************************************************************
 *  Convert json list of names into path
 ***************************************************************************/
PRIVATE char *multiple_dir(char* bf, int bflen, json_t* jn_l)
{
    char *p = bf;
    int ln;

    *bf = 0;

    /*--------------------------------------------------------*
     *  Add domain
     *--------------------------------------------------------*/
    if(jn_l) {
        size_t index;
        json_t *jn_s_domain_name;

        if(!json_is_array(jn_l)) {
            return 0;
        }
        json_array_foreach(jn_l, index, jn_s_domain_name) {
            if(!json_is_string(jn_s_domain_name)) {
                return 0;
            }
            if(index == 0) {
                ln = snprintf(p, bflen, "%s", json_string_value(jn_s_domain_name));
            } else {
                ln = snprintf(p, bflen, "/%s", json_string_value(jn_s_domain_name));
            }
            if(ln<0) {
                *bf = 0;
                return 0;
            }

            p += ln;
            bflen -= ln;
        }
    }
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *yuneta_repos_yuno_dir(
    char *bf,
    int bfsize,
    json_t *jn_tags,  // not owned
    const char *yuno_role,
    const char *yuno_version,
    BOOL create)
{
    const char *work_dir = yuneta_root_dir();
    char tags[NAME_MAX];
    multiple_dir(tags, sizeof(tags), jn_tags);

    build_path(bf, bfsize, work_dir, "repos", tags, yuno_role, yuno_version, NULL);

    if(create) {
        if(access(bf, 0)!=0) {
            mkrdir(bf, yuneta_xpermission());
            if(access(bf, 0)!=0) {
                *bf = 0;
                return 0;
            }
        }
    }
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *yuneta_repos_yuno_file(
    char * bf,
    int bfsize,
    json_t *jn_tags, // not owned
    const char *yuno_role,
    const char *yuno_version,
    const char *filename,
    BOOL create)
{
    char repos_dir[PATH_MAX];
    yuneta_repos_yuno_dir(
        repos_dir,
        sizeof(repos_dir),
        jn_tags,
        yuno_role,
        yuno_version,
        create
    );

    build_path(bf, bfsize, repos_dir, filename, NULL);
    return bf;
}

/***************************************************************************
 *  Build the private domain of yuno (defined by his realm)
 ***************************************************************************/
PRIVATE char * build_yuno_private_domain(
    hgobj gobj,
    json_t *yuno,
    char *bf,
    int bfsize
)
{
    json_t *realm = get_yuno_realm(gobj, yuno);
    if(!realm) {
        *bf = 0;
        return 0;
    }
    const char *realm_owner = kw_get_str(gobj, realm, "realm_owner", 0, KW_REQUIRED);
    const char *realm_role = kw_get_str(gobj, realm, "realm_role", 0, KW_REQUIRED);
    const char *realm_name = kw_get_str(gobj, realm, "realm_name", 0, KW_REQUIRED);
    const char *realm_env = kw_get_str(gobj, realm, "realm_env", 0, KW_REQUIRED);

    char url[NAME_MAX];
    snprintf(url, sizeof(url), "%s.%s.%s", realm_name, realm_role, realm_env);

    char role_plus_id[NAME_MAX];
    build_role_plus_id(role_plus_id, sizeof(role_plus_id), yuno);

    json_decref(realm);

    return build_path(bf, bfsize, "realms", realm_owner, url, role_plus_id, NULL);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char * build_yuno_bin_path(hgobj gobj, json_t *yuno, char *bf, int bfsize, BOOL create_dir)
{
    char private_domain[PATH_MAX];
    build_yuno_private_domain(gobj, yuno, private_domain, sizeof(private_domain));

    const char *work_dir = yuneta_root_dir();
    build_path(bf, bfsize, work_dir, private_domain, "bin", NULL);

    if(create_dir) {
        if(mkrdir(bf, yuneta_xpermission())<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create directory",
                "path",         "%s", bf,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
    }
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char * build_yuno_log_path(hgobj gobj, json_t *yuno, char *bf, int bfsize, BOOL create_dir)
{
    char private_domain[PATH_MAX];
    build_yuno_private_domain(gobj, yuno, private_domain, sizeof(private_domain));

    const char *work_dir = yuneta_root_dir();
    build_path(bf, bfsize, work_dir, private_domain, "logs", NULL);

    if(create_dir) {
        if(mkrdir(bf, yuneta_xpermission())<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create directory",
                "path",         "%s", bf,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
    }
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *find_required_services_size(
    hgobj gobj,
    json_t *hs_binary,
    json_t *jn_config_required_services  // owned
)
{
    json_t *jn_required_services = json_array();

    json_t *_jn_required_services = SDATA_GET_JSON(hs_binary, "required_services");
    if(_jn_required_services) {
        json_array_extend(jn_required_services, _jn_required_services);
    }
    int idx; json_t *jn_service;
    json_array_foreach(jn_config_required_services, idx, jn_service) {
        const char *service = json_string_value(jn_service);
        if(!json_str_in_list(gobj, jn_required_services, service, FALSE)) {
            json_array_append_new(jn_required_services, json_string(service));
        }
    }
    JSON_DECREF(jn_config_required_services)

    return jn_required_services;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_yuno_realm(hgobj gobj, json_t *yuno)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *realm_id = kw_get_str(gobj, yuno, "realm_id`0", "", KW_REQUIRED);
    json_t *hs_realm = gobj_get_node(
        priv->resource,
        "realms",
        json_pack("{s:s}", "id", realm_id),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );
    if(!hs_realm) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "no realm found",
            "realm_id",     "%s", realm_id,
            NULL
        );
        return 0;
    }
    return hs_realm;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_yuno_binary(hgobj gobj, json_t *yuno)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *snaps = gobj_list_snaps(
        priv->resource,
        json_pack("{s:b}", "active", 1),
        gobj
    );
    BOOL is_snap_activated = json_array_size(snaps)?TRUE:FALSE;
    JSON_DECREF(snaps);

    json_t *kw_find = json_pack("{s:s, s:s}",
        "id", SDATA_GET_STR(yuno, "yuno_role"),
        "version", SDATA_GET_STR(yuno, "role_version")
    );
    json_t *binaries = gobj_list_nodes(
        priv->resource,
        "binaries",
        json_incref(kw_find), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );
    if(json_array_size(binaries)==0) {
        if(is_snap_activated) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "primary binary not found",
                NULL
            );
            JSON_DECREF(kw_find);
            JSON_DECREF(binaries);
            return 0;
        }

        JSON_DECREF(binaries);
        binaries = gobj_list_instances(
            priv->resource,
            "binaries",
            "",
            json_incref(kw_find), // filter
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            gobj
        );
        if(json_array_size(binaries)==0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "secondary binary not found",
                NULL
            );
            JSON_DECREF(kw_find);
            JSON_DECREF(binaries);
            return 0;
        }
    }

    json_t *hs_binary = json_array_get(binaries, 0);
    json_incref(hs_binary);
    json_decref(kw_find);
    json_decref(binaries);
    return hs_binary;

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_yuno_config(hgobj gobj, json_t *yuno)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *snaps = gobj_list_snaps(
        priv->resource,
        json_pack("{s:b}", "active", 1),
        gobj
    );
    BOOL is_snap_activated = json_array_size(snaps)?TRUE:FALSE;
    JSON_DECREF(snaps);

    char config_name[80];
    snprintf(config_name, sizeof(config_name), "%s.%s",
        SDATA_GET_STR(yuno, "yuno_role"),
        SDATA_GET_STR(yuno, "yuno_name")
    );

    json_t *kw_find = json_pack("{s:s, s:s}",
        "id", config_name,
        "version", SDATA_GET_STR(yuno, "name_version")
    );
    json_t *configurations = gobj_list_nodes(
        priv->resource,
        "configurations",
        json_incref(kw_find), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    if(json_array_size(configurations)==0) {
        if(is_snap_activated) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "primary configuration not found",
                NULL
            );
            JSON_DECREF(kw_find);
            JSON_DECREF(configurations);
            return 0;
        }

        JSON_DECREF(configurations);
        configurations = gobj_list_instances(
            priv->resource,
            "configurations",
            "",
            json_incref(kw_find), // filter
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            gobj
        );
        if(json_array_size(configurations)==0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "secondary configuration not found",
                NULL
            );
            JSON_DECREF(kw_find);
            JSON_DECREF(configurations);
            return 0;
        }
    }

    json_t *hs_configuration = json_array_get(configurations, 0);
    json_incref(hs_configuration);
    json_decref(kw_find);
    json_decref(configurations);
    return hs_configuration;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *find_public_service(
    hgobj gobj,
    const char *yuno_id,
    const char *service
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "public_services";

    json_t *kw_find = json_pack("{s:s}",
        "service", service
    );
    if(!empty_string(yuno_id)) {
        json_object_set_new(kw_find, "yuno_id", json_string(yuno_id));
    }

    json_t *iter_find = gobj_list_nodes(
        priv->resource,
        resource,
        kw_find, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    json_t *hs = json_array_get(iter_find, 0);

    json_incref(hs);
    JSON_DECREF(iter_find);
    return hs;
}

/***************************************************************************
 *  Find a service for client
 ***************************************************************************/
PRIVATE json_t *find_service_for_client(
    hgobj gobj,
    const char *service_
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "public_services";

    char *service = gbmem_strdup(service_);
    char *service_yuno_name = strchr(service, '.'); // TODO legacy add alternative parameter: dict (jn_filter)
    if(service_yuno_name) {
        *service_yuno_name = 0;
        service_yuno_name++; // yuno_name of service required
    }

    json_t *kw_find = json_pack("{s:s}",
        "service", service
    );
    if(service_yuno_name) {
        json_object_set_new(kw_find, "yuno_name", json_string(service_yuno_name));
    }

    json_t *iter_find = gobj_list_nodes(
        priv->resource,
        resource,
        kw_find, // filter
        json_pack("{s:b}", "only-id", 1),
        gobj
    );

    json_t *hs = json_array_get(iter_find, 0);
    json_incref(hs);
    json_decref(iter_find);
    gbmem_free(service);

    return hs;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int write_service_client_connectors(
    hgobj gobj,
    json_t *yuno,
    const char *config_path,
    json_t *jn_required_services
)
{
    const char *realm_id_ = kw_get_str(gobj, yuno, "realm_id`0", "", KW_REQUIRED);
    const char *yuno_role_ = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);
    const char *yuno_name_ = kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED);
    const char *yuno_id_ = kw_get_str(gobj, yuno, "id", "", KW_REQUIRED);

    json_t *hs_binary = get_yuno_binary(gobj, yuno);
    json_t *jn_services = json_array();
    json_t *jn_yuno_services = json_pack("{s:o}",
        "services", jn_services
    );
    gbuffer_t *gbuf_config = gbuffer_create((size_t)4*1024, (size_t)256*1024);
    size_t index;
    json_t *jn_service;
    json_array_foreach(jn_required_services, index, jn_service) {
        const char *yuno_service = json_string_value(jn_service);
        if(empty_string(yuno_service)) {
            continue;
        }
        json_t *hs_service = find_service_for_client(gobj, yuno_service);
        if(!hs_service) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_SERVICE_ERROR,
                "msg",              "%s", "required service NOT FOUND",
                "required service", "%s", yuno_service,
                "required yuno_id", "%s", yuno_id_,
                "realm_id",         "%s", realm_id_,
                "yuno_role",        "%s", yuno_role_,
                "yuno_name",        "%s", yuno_name_,
                NULL
            );
            continue;
        }
        json_t *jn_connector = SDATA_GET_JSON(hs_service, "connector");
        if(!jn_connector) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_SERVICE_ERROR,
                "msg",              "%s", "service connector NULL",
                "required service", "%s", yuno_service,
                "required yuno_id", "%s", yuno_id_,
                "realm_id",         "%s", realm_id_,
                "yuno_role",        "%s", yuno_role_,
                "yuno_name",        "%s", yuno_name_,
                NULL
            );
            continue;
        }
        const char *url = SDATA_GET_STR(hs_service, "url");
        const char *realm_id = SDATA_GET_STR(hs_service, "realm_id");
        const char *yuno_role = SDATA_GET_STR(hs_service, "yuno_role");
        const char *yuno_name = SDATA_GET_STR(hs_service, "yuno_name");
        const char *schema = SDATA_GET_STR(hs_service, "schema");
        const char *ip =  SDATA_GET_STR(hs_service, "ip"); // TODO legacy sácalo de la url
        uint32_t port_ =  SDATA_GET_INT(hs_service, "port");
        char port[32];
        snprintf(port, sizeof(port), "%d", port_);
        json_t * jn_config_variables = json_pack("{s:{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}}",
            "__json_config_variables__",
                "__realm_id__", realm_id,
                "__yuno_name__", yuno_name,
                "__yuno_role__", yuno_role,
                "__yuno_service__", yuno_service,
                "__ip__", ip,
                "__port__", port,
                "__schema__", schema,
                "__url__", url
        );

        json_t *kw_connector = 0; // TODO kw_apply_json_config_variables(jn_connector, jn_config_variables);
        json_decref(jn_config_variables);
        json_array_append_new(jn_services, kw_connector);
        json_decref(hs_service);
    }

    gbuffer_append_json(
        gbuf_config,
        jn_yuno_services // owned
    );

    gbuf2file( // save: service connectors
        gobj,
        gbuf_config, // owned
        config_path,
        yuneta_rpermission(),
        TRUE
    );

    json_decref(hs_binary);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *assigned_yuno_global_service_variables(
    hgobj gobj,
    const char *yuno_id,
    const char *yuno_name,
    const char *yuno_role
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *jn_global_content = json_object();

    /*
     *  Busca los servicios públicos de este yuno.
     */

    /*
     *  Crea una entrada en global por servicio: "__service__`__json_config_variables__"
     *  con
     *      __realm_id__
     *      __yuno_name__
     *      __yuno_role__
     *      __yuno_service__
     *      __url__
     *      __port__
     *      __ip__
     *
     */
    json_t *kw_find = json_pack("{s:s}",
        "yuno_id", yuno_id
    );
    json_t *iter = gobj_list_nodes(
        priv->resource,
        "public_services",
        kw_find, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    int idx; json_t *hs;
    json_array_foreach(iter, idx, hs) {
        /*
         *  Add the service variables
         */
        const char *service = SDATA_GET_STR(hs, "service");
        char key[256];
        snprintf(key, sizeof(key), "%s.__json_config_variables__", service);
        json_t *jn_variables = json_object();
        json_object_set_new(jn_global_content, key, jn_variables);

        const char *ip = SDATA_GET_STR(hs, "ip");
        const char *realm_id = SDATA_GET_STR(hs, "realm_id");
        uint32_t port_ = SDATA_GET_INT(hs, "port");
        char port[32];
        snprintf(port, sizeof(port), "%d", port_);
        const char *url = SDATA_GET_STR(hs, "url");

        json_object_set_new(jn_variables, "__realm_id__", json_string(realm_id));
        json_object_set_new(jn_variables, "__yuno_name__", json_string(yuno_name));
        json_object_set_new(jn_variables, "__yuno_role__", json_string(yuno_role));
        json_object_set_new(jn_variables, "__yuno_service__", json_string(service));
        json_object_set_new(jn_variables, "__ip__", json_string(ip));
        json_object_set_new(jn_variables, "__port__", json_string(port));
        json_object_set_new(jn_variables, "__url__", json_string(url));
    }
    JSON_DECREF(iter)

    return jn_global_content;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gbuffer_t *build_yuno_running_script(
    hgobj gobj,
    gbuffer_t *gbuf_script,
    json_t *yuno,
    char *bfbinary,
    int bfbinary_size,
    BOOL only_double_quotes
)
{
    const char *work_dir = yuneta_root_dir();
    const char *yuno_id = SDATA_GET_ID(yuno);

    /*
     *  Build the domain of yuno (defined by his realm)
     */
    char domain_dir[PATH_MAX];
    build_yuno_private_domain(gobj, yuno, domain_dir, sizeof(domain_dir));

    char yuno_bin_path[PATH_MAX];
    build_yuno_bin_path(gobj, yuno, yuno_bin_path, sizeof(yuno_bin_path), TRUE);

    json_t *hs_realm = get_yuno_realm(gobj, yuno);
    if(!hs_realm) {
        return 0;
    }
    const char *bind_ip = SDATA_GET_STR(hs_realm, "bind_ip");
    const char *realm_owner = kw_get_str(gobj, hs_realm, "realm_owner", "", KW_REQUIRED);
    const char *realm_role = kw_get_str(gobj, hs_realm, "realm_role", "", KW_REQUIRED);
    const char *realm_name = kw_get_str(gobj, hs_realm, "realm_name", "", KW_REQUIRED);
    const char *realm_env = kw_get_str(gobj, hs_realm, "realm_env", "", KW_REQUIRED);

    BOOL multiple = kw_get_bool(gobj, yuno, "yuno_multiple", 0, KW_REQUIRED);
    const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);
    const char *yuno_name = kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED);
    const char *yuno_tag = kw_get_str(gobj, yuno, "yuno_tag", "", KW_REQUIRED);
    const char *yuno_release = kw_get_str(gobj, yuno, "yuno_release", "", KW_REQUIRED);
    json_int_t launch_id = kw_get_int(gobj, yuno, "launch_id", 0, KW_REQUIRED);

    json_t *binary = get_yuno_binary(gobj, yuno);
    if(!binary) {
        json_decref(hs_realm);
        return 0;
    }
    const char *binary_path = kw_get_str(gobj, binary, "binary", "", KW_REQUIRED);
    char role_plus_name[NAME_MAX];
    build_role_plus_name(role_plus_name, sizeof(role_plus_name), yuno);

    /*
     *  Build the run script
     */
    snprintf(bfbinary, bfbinary_size, "%s", binary_path);

    char config_file_name[PATH_MAX+15];
    char config_path[(PATH_MAX+15)*2];
    int n_config = 0;
    gbuffer_printf(gbuf_script, "[");

    if(1) {
        /*-------------------------------------------*
         *      Put environment and yuno variables
         *-------------------------------------------*/
        snprintf(config_file_name, sizeof(config_file_name), "%d-%s",
            n_config+1,
            role_plus_name
        );
        snprintf(config_path, sizeof(config_path), "%s/%s.json", yuno_bin_path, config_file_name);

        gbuffer_t *gbuf_config = gbuffer_create(4*1024, 256*1024);

        json_t *jn_global = assigned_yuno_global_service_variables(
            gobj,
            yuno_id,
            yuno_name,
            yuno_role
        );
        json_t *jn_node_variables = gobj_read_json_attr(gobj, "node_variables");
        if(jn_node_variables) {
            json_object_update(jn_global, jn_node_variables);
        }

        const char *node_owner = gobj_yuno_node_owner();
        if(empty_string(node_owner)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "node_owner EMPTY",
                NULL
            );
            node_owner = "none";
        }

        json_t *jn_environment = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
            "work_dir", work_dir,
            "domain_dir", domain_dir,
            "node_owner", node_owner,
            "realm_id", SDATA_GET_STR(yuno, "realm_id`0"),
            "realm_owner", realm_owner,
            "realm_role", realm_role,
            "realm_name", realm_name,
            "realm_env", realm_env
        );
        json_t *jn_content = json_pack("{s:o, s:o, s:{s:s, s:s, s:s, s:s, s:s, s:b, s:I}}",
            "global", jn_global,
            "environment", jn_environment,
            "yuno",
                "yuno_id", yuno_id,
                "yuno_name", yuno_name,
                "yuno_tag", yuno_tag,
                "yuno_release", yuno_release,
                "bind_ip", bind_ip,
                "yuno_multiple", multiple,
                "launch_id", (json_int_t)launch_id
        );
        json_t *jn_agent_environment = gobj_read_json_attr(gobj, "agent_environment");
        if(jn_agent_environment) {
            // HACK Override the yuno environment by agent environment.
            json_object_update(jn_environment, jn_agent_environment);
        }

        gbuffer_append_json(
            gbuf_config,
            jn_content  //owned
        );

        gbuf2file( // save: environment and yuno variables
            gobj,
            gbuf_config, // owned
            config_path,
            yuneta_rpermission(),
            TRUE
        );
        if(n_config > 0) {
            gbuffer_printf(gbuf_script, ",");
        }
        if(only_double_quotes) {
            gbuffer_printf(gbuf_script, "\\\"%s\\\"", config_path);
        } else {
            gbuffer_printf(gbuf_script, "\"%s\"", config_path);
        }
        n_config++;
    }

    if(1) {
        /*------------------------------------*
         *      Put agent client service
         *------------------------------------*/
        snprintf(config_file_name, sizeof(config_file_name), "%d-%s",
            n_config+1,
            role_plus_name
        );
        snprintf(config_path, sizeof(config_path), "%s/%s.json", yuno_bin_path, config_file_name);

        gbuffer_t *gbuf_config = gbuffer_create((size_t)4*1024, 256*1024);
        char *client_agent_config = gbmem_strdup(agent_filter_chain_config);
        helper_quote2doublequote(client_agent_config);

        gbuffer_printf(
            gbuf_config,
            client_agent_config,
            SDATA_GET_STR(yuno, "realm_id`0"),
            yuno_id
        );

        gbuf2file( // save: agent connector
            gobj,
            gbuf_config, // owned
            config_path,
            yuneta_rpermission(),
            TRUE
        );
        if(n_config > 0) {
            gbuffer_printf(gbuf_script, ",");
        }
        if(only_double_quotes) {
            gbuffer_printf(gbuf_script, "\\\"%s\\\"", config_path);
        } else {
            gbuffer_printf(gbuf_script, "\"%s\"", config_path);
        }

        n_config++;

        gbmem_free(client_agent_config);

        /*--------------------------------------*
         *      Put yuno configuration
         *--------------------------------------*/
        json_t *jn_config_required_services = 0;
        json_t *hs_config = get_yuno_config(gobj, yuno);
        if(hs_config) {
            gbuf_config = gbuffer_create(4*1024, 256*1024);
            snprintf(config_file_name, sizeof(config_file_name), "%d-%s",
                n_config+1,
                role_plus_name
            );
            snprintf(config_path, sizeof(config_path),
                "%s/%s.json",
                yuno_bin_path,
                config_file_name
            );

            json_t *content = SDATA_GET_JSON(hs_config, "zcontent");
            JSON_INCREF(content);
            gbuffer_append_json(
                gbuf_config,
                content // owned
            );

            jn_config_required_services = kw_get_list(gobj, content, "yuno`required_services", 0, 0);
            if(jn_config_required_services) {
                json_incref(jn_config_required_services);
            }

            gbuf2file( // save: user configurations
                gobj,
                gbuf_config, // owned
                config_path,
                yuneta_rpermission(),
                TRUE
            );
            if(n_config > 0) {
                gbuffer_printf(gbuf_script, ",");
            }
            if(only_double_quotes) {
                gbuffer_printf(gbuf_script, "\\\"%s\\\"", config_path);
            } else {
                gbuffer_printf(gbuf_script, "\"%s\"", config_path);
            }
            n_config++;

            json_decref(hs_config);
        }

        /*--------------------------------------*
         *      Put required service clients
         *--------------------------------------*/
        json_t *jn_required_services = find_required_services_size(gobj, binary, jn_config_required_services);
        if(json_array_size(jn_required_services)>0) {
            snprintf(config_file_name, sizeof(config_file_name), "%d-%s",
                n_config+1,
                role_plus_name
            );
            snprintf(config_path, sizeof(config_path), "%s/%s.json", yuno_bin_path, config_file_name);
            write_service_client_connectors( // save: service connectors
                gobj,
                yuno,
                config_path,
                jn_required_services
            );
            if(n_config > 0) {
                gbuffer_printf(gbuf_script, ",");
            }
            if(only_double_quotes) {
                gbuffer_printf(gbuf_script, "\\\"%s\\\"", config_path);
            } else {
                gbuffer_printf(gbuf_script, "\"%s\"", config_path);
            }

            n_config++;
        }
        JSON_DECREF(jn_required_services)
    }

    gbuffer_printf(gbuf_script, "]");

    json_decref(hs_realm);
    json_decref(binary);

    return gbuf_script;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int run_yuno(
    hgobj gobj,
    json_t *yuno, // not owned
    hgobj src
)
{
    /*
     *  Launch id
     *  TODO legacy cuando un yuno no arranca y no encuentra una .so, aparece como running al agente
     */

    static uint16_t counter = 0;
    uint64_t t;
    time((time_t*)&t);
    t = t<<(sizeof(uint16_t)*8);
    t += ++counter;
    json_object_set_new(yuno, "launch_id", json_integer(t));

    char bfbinary[NAME_MAX];
    gbuffer_t *gbuf_sh = gbuffer_create(4*1024, 32*1024);
    build_yuno_running_script(gobj, gbuf_sh, yuno, bfbinary, sizeof(bfbinary), FALSE);

    const char *realm_id = kw_get_str(gobj, yuno, "realm_id`0", "", KW_REQUIRED);
    const char *yuno_id = kw_get_str(gobj, yuno, "id", "", KW_REQUIRED);
    const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);
    const char *yuno_name = kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED);
    const char *yuno_tag = kw_get_str(gobj, yuno, "yuno_tag", "", KW_REQUIRED);
    const char *yuno_release = kw_get_str(gobj, yuno, "yuno_release", "", KW_REQUIRED);

    char yuno_bin_path[NAME_MAX];
    build_yuno_bin_path(gobj, yuno, yuno_bin_path, sizeof(yuno_bin_path), TRUE);

    char role_plus_name[NAME_MAX];
    build_role_plus_name(role_plus_name, sizeof(role_plus_name), yuno);

    char script_path[NAME_MAX*2 + 10];
    snprintf(script_path, sizeof(script_path), "%s/%s.sh", yuno_bin_path, role_plus_name);

    char exec_cmd[PATH_MAX];
    snprintf(exec_cmd, sizeof(exec_cmd), "%s --start", script_path);

    gobj_log_debug(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", "run yuno",
        "realm_id",     "%s", realm_id,
        "yuno_id",      "%s", yuno_id,
        "exec_cmd",     "%s", exec_cmd,
        NULL
    );

    char *bfarg = gbuffer_cur_rd_pointer(gbuf_sh);
    char *const argv[]={(char *)yuno_role, "-f", bfarg, "--start", 0};

    int ret = run_process2(bfbinary, argv);
    if(ret != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot run the yuno",
            "realm_id",     "%s", realm_id,
            "yuno_id",      "%s", yuno_id,
            "yuno_role",    "%s", yuno_role,
            "yuno_name",    "%s", yuno_name?yuno_name:"",
            "yuno_tag",   "%s", yuno_name?yuno_tag:"",
            "yuno_release", "%s", yuno_release?yuno_release:"",
            "ret",          "%d", ret,
            NULL
        );
    }

    int fd = newfile(script_path, yuneta_xpermission(), TRUE);
    if(fd<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "newfile() FAILED",
            "path",         "%s", script_path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    } else {
        write(fd, bfbinary, strlen(bfbinary));
        write(fd, " --config-file='", strlen(" --config-file='"));
        write(fd, bfarg, strlen(bfarg));
        write(fd, "' $1\n", strlen("' $1\n"));
        close(fd);
    }
    gbuffer_decref(gbuf_sh);
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int kill_yuno(hgobj gobj, json_t *yuno)
{
    /*
     *  Get some yuno data
     */
    int signal2kill = gobj_read_integer_attr(gobj, "signal2kill");
    if(!signal2kill) {
        signal2kill = SIGQUIT;
    }
    const char *realm_id = kw_get_str(gobj, yuno, "realm_id`0", "", KW_REQUIRED);
    const char *yuno_id = kw_get_str(gobj, yuno, "id", "", KW_REQUIRED);
    const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);
    const char *yuno_name = kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED);
    const char *yuno_release = kw_get_str(gobj, yuno, "yuno_release", "", KW_REQUIRED);
    uint32_t pid = kw_get_int(gobj, yuno, "yuno_pid", 0, KW_REQUIRED);
    uint32_t watcher_pid = kw_get_int(gobj, yuno, "watcher_pid", 0, KW_REQUIRED);
    if(!pid) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "yuno PID NULL",
            "yuno_id",      "%s", yuno_id,
            "pid",          "%d", (int)pid,
            "yuno_role",    "%s", yuno_role,
            "yuno_name",    "%s", yuno_name?yuno_name:"",
            "yuno_release", "%s", yuno_release?yuno_release:"",

            NULL
        );
        return -1;
    }
    gobj_log_debug(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", "kill yuno",
        "realm_id",     "%s", realm_id,
        "yuno_id",      "%s", yuno_id,
        "pid",          "%d", (int)pid,
        "watcher_pid",  "%d", (int)watcher_pid,
        NULL
    );

    int ret = 0;

    if(kill(pid, signal2kill)<0) {
        int last_errno = errno;
        if(last_errno != ESRCH) { // No such process
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Cannot kill yuno",
                "yuno_id",      "%s", yuno_id,
                "pid",          "%d", (int)pid,
                "yuno_role",    "%s", yuno_role,
                "yuno_name",    "%s", yuno_name?yuno_name:"",
                "yuno_release", "%s", yuno_release?yuno_release:"",
                "error",        "%d", last_errno,
                "strerror",     "%s", strerror(last_errno),
                NULL
            );
            // TODO gobj_set_message_error(gobj, strerror(last_errno));
            ret = -1;
        }
    }

    if(signal2kill == SIGKILL) {
        // Kill the watcher
        if(watcher_pid) {
            if(kill(watcher_pid, signal2kill)<0) {
                int last_errno = errno;
                if(last_errno != ESRCH) { // No such process
                    gobj_log_info(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "Cannot kill watcher yuno",
                        "yuno_id",      "%s", yuno_id,
                        "watcher_pid",  "%d", (int)watcher_pid,
                        "yuno_role",    "%s", yuno_role,
                        "yuno_name",    "%s", yuno_name?yuno_name:"",
                        "yuno_release", "%s", yuno_release?yuno_release:"",
                        "error",        "%d", last_errno,
                        "strerror",     "%s", strerror(last_errno),
                        NULL
                    );
                    // TODO gobj_set_message_error(gobj, strerror(last_errno));
                    ret = -1;
                }
            }
        }
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int play_yuno(hgobj gobj, json_t *yuno, json_t *kw, hgobj src)
{
    const char *realm_id = kw_get_str(gobj, yuno, "realm_id`0", "", KW_REQUIRED);
    const char *yuno_id = kw_get_str(gobj, yuno, "id", "", KW_REQUIRED);
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
    if(!channel_gobj) {
        KW_DECREF(kw);
        return -1;
    }

    gobj_log_debug(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", "play yuno",
        "realm_id",     "%s", realm_id,
        "yuno_id",      "%s", yuno_id,
        NULL
    );

    return gobj_send_event(
        channel_gobj,
        EV_PLAY_YUNO,
        kw,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int pause_yuno(hgobj gobj, json_t *yuno, json_t *kw, hgobj src)
{
    const char *realm_id = kw_get_str(gobj, yuno, "realm_id`0", "", KW_REQUIRED);
    const char *yuno_id = kw_get_str(gobj, yuno, "id", "", KW_REQUIRED);
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
    if(!channel_gobj) {
        KW_DECREF(kw);
        return -1;
    }

    gobj_log_debug(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", "pause yuno",
        "realm_id",     "%s", realm_id,
        "yuno_id",      "%s", yuno_id,
        NULL
    );

    return gobj_send_event(
        channel_gobj,
        "EV_PAUSE_YUNO",
        kw,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int trace_on_yuno(hgobj gobj, json_t *yuno, json_t *kw,  hgobj src)
{
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
    if(!channel_gobj) {
        return -1;
    }
    json_object_set_new(kw, "service", json_string("__root__"));

    char command[256];
    snprintf(
        command,
        sizeof(command),
        "set-gobj-trace "
        "level=%d set=1",
        TRACE_USER_LEVEL
    );
    json_t *webix = gobj_command( // debe retornar siempre 0.
        channel_gobj,
        command,
        kw,
        gobj
    );
    JSON_DECREF(webix);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int trace_off_yuno(hgobj gobj, json_t *yuno, json_t *kw,  hgobj src)
{
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
    if(!channel_gobj) {
        return -1;
    }
    json_object_set_new(kw, "service", json_string("__root__"));

    char command[256];
    snprintf(
        command,
        sizeof(command),
        "set-gobj-trace "
        "level=%d set=0",
        TRACE_USER_LEVEL
    );
    json_t *webix = gobj_command( // debe retornar siempre 0.
        channel_gobj,
        command,
        kw,
        src //gobj
    );
    JSON_DECREF(webix);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int command_to_yuno(hgobj gobj, json_t *yuno, const char *command, json_t *kw, hgobj src)
{
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
    if(!channel_gobj) {
        KW_DECREF(kw);
        return -1;
    }
    json_t *webix = gobj_command( // debe retornar siempre 0.
        channel_gobj,
        command,
        kw,
        src //gobj
    );
    JSON_DECREF(webix);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int stats_to_yuno(hgobj gobj, json_t *yuno, const char* stats, json_t* kw, hgobj src)
{
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
    if(!channel_gobj) {
        KW_DECREF(kw);
        return -1;
    }
    json_t *webix =  gobj_stats(  // debe retornar siempre 0.
        channel_gobj,
        stats,
        kw,
        src // gobj
    );
    JSON_DECREF(webix);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int authzs_to_yuno(
    json_t *yuno,
    json_t* kw,
    hgobj src
)
{
    hgobj gobj = 0; // TODO
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
    if(!channel_gobj) {
        KW_DECREF(kw);
        return -1;
    }
    json_t *webix = gobj_command( // debe retornar siempre 0.
        channel_gobj,
        "authzs",
        kw,
        src //gobj
    );
    JSON_DECREF(webix);
    return 0;
}

/***************************************************************************
 *  Try to run the activated yunos.
 *  This function is periodically called by timer
 ***************************************************************************/
PRIVATE int run_enabled_yunos(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    gobj_log_debug(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", "run_enabled_yunos",
        NULL
    );

    /*
     *  Esta bien así, no le paso nada, que devuelva all yunos de all reinos.
     */
    json_t *iter_yunos = gobj_list_nodes(
        priv->resource,
        resource,
        0, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );
    int idx; json_t *yuno;
    json_array_foreach(iter_yunos, idx, yuno) {
        /*
         *  Activate the yuno
         */
        BOOL disabled = kw_get_bool(gobj, yuno, "yuno_disabled", 0, KW_REQUIRED);
        if(!disabled) {
            BOOL running = kw_get_bool(gobj, yuno, "yuno_running", 0, KW_REQUIRED);
            if(!running) {
                run_yuno(gobj, yuno, 0);
            }
        }
    }
    JSON_DECREF(iter_yunos);

    get_last_public_port(gobj);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int audit_command_cb(const char *command, json_t *kw, void *user_data)
{
    hgobj gobj = user_data;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->audit_file) {
        if(!kw) {
            kw = json_object();
        } else {
            KW_INCREF(kw);
        }
        char fecha[32];
        current_timestamp(fecha, sizeof(fecha));
        json_t *jn_cmd = json_pack("{s:s, s:s, s:o}",
            "command", command,
            "date", fecha,
            "kw", kw
        );
        if(jn_cmd) {
            char *audit = json2uglystr(jn_cmd);
            if(audit) {
                rotatory_write(priv->audit_file, LOG_AUDIT, audit, strlen(audit));
                rotatory_write(priv->audit_file, LOG_AUDIT, "\n", 1);  // double new line: the separator field
                gbmem_free(audit);
            }
            json_decref(jn_cmd);
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *find_binary_version(
    hgobj gobj,
    const char *role,
    const char *version
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_find = json_pack("{s:s}",
        "id", role
    );

    json_t *iter_find = gobj_list_instances(
        priv->resource,
        "binaries",
        "",
        kw_find, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );
    int idx; json_t *hs;
    json_array_foreach(iter_find, idx, hs) {
        const char *version_ = SDATA_GET_STR(hs, "version");
        if(empty_string(version)) {
            // Get the last if no version wanted.
            break;
        }
        if(strcmp(version, version_)==0) {
            /*
             *  Found the wanted version.
             *  FUTURE we can manage operators like python (>=, =, <=)
             */
            break;
        }
    }

    json_incref(hs);
    JSON_DECREF(iter_find);

    return hs;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *find_configuration_version(
    hgobj gobj,
    const char *role,
    const char *name,
    const char *version
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Search with role prefix.
     */
    char with_prefix[120];
    snprintf(with_prefix, sizeof(with_prefix), "%s.%s", role, name);
    json_t *kw_find = json_pack("{s:s}",
        "id", with_prefix
    );
    json_t *iter_find = gobj_list_instances(
        priv->resource,
        "configurations",
        "",
        kw_find, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    int idx; json_t *hs=0;
    json_array_foreach(iter_find, idx, hs) {
        const char *version_ = SDATA_GET_STR(hs, "version");
        if(empty_string(version)) {
            // Get the last if no version wanted.
            break;
        }
        if(strcmp(version, version_)==0) {
            /*
             *  Found the wanted version.
             *  FUTURE we can manage operators like python (>=, =, <=)
             */
            break;
        }
    }

    json_incref(hs);
    JSON_DECREF(iter_find);

    return hs;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int build_release_name(char *bf, int bfsize, json_t *hs_binary, json_t *hs_config)
{
    hgobj gobj = 0; // TODO
    int len;
    char *p = bf;

    const char *binary_version = SDATA_GET_STR(hs_binary, "version");
    snprintf(p, bfsize, "%s", binary_version);
    len = (int)strlen(p); p += len; bfsize -= len;

    const char *version_ = SDATA_GET_STR(hs_config, "version");

    snprintf(p, bfsize, "-%s", version_);
    len = (int)strlen(p); p += len; bfsize -= len;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_last_public_port(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "public_services";

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        json_object(), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return 0;
    }

    /*
     *  Delete
     */
    int last_port = 0;

    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        int port = (int)kw_get_int(gobj, node, "port", 0, KW_REQUIRED);
        if(port > last_port) {
            last_port = port;
        }
    }

    gobj_write_integer_attr(gobj, "last_port", last_port);

    JSON_DECREF(iter)
    return last_port;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE unsigned get_new_service_port(
    hgobj gobj,
    json_t *hs_realm // NOT owned
)
{
    json_t *jn_range_ports = kw_get_dict_value(gobj, hs_realm, "range_ports", 0, KW_REQUIRED);
    json_t *jn_port_list = json_listsrange2set(jn_range_ports);

    unsigned new_port = 0;
    unsigned last_port = get_last_public_port(gobj);
    if(!last_port) {
        new_port = json_list_int(jn_port_list, 0);
    } else {
        int idx = json_list_int_index(jn_port_list, last_port);
        if(idx < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "integer not in list",
                NULL
            );
            JSON_DECREF(jn_port_list);
            return 0;
        }
        idx ++;
        if(idx >= json_array_size(jn_port_list)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SERVICE_ERROR,
                "msg",          "%s", "Range of ports are exhausted",
                NULL
            );
            JSON_DECREF(jn_port_list);
            return 0;
        }
        new_port = json_list_int(jn_port_list, idx);
    }

    gobj_write_integer_attr(gobj, "last_port", new_port);

    JSON_DECREF(jn_port_list);
    return new_port;
}

/***************************************************************************
 *  TODO legacy bug: si registramos un servicio con cambios en el conector
 *  afectará al snap shoot actual
 ***************************************************************************/
PRIVATE int register_public_services(
    hgobj gobj,
    json_t *yuno, // not owned
    json_t *hs_binary, // not owned
    json_t *hs_realm // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "public_services";

    int ret = 0;

    const char *yuno_id = SDATA_GET_ID(yuno);
    const char *yuno_role = SDATA_GET_STR(yuno, "yuno_role");
    const char *yuno_name = SDATA_GET_STR(yuno, "yuno_name");

    const char *realm_id = SDATA_GET_ID(hs_realm);
    json_t *jn_public_services = SDATA_GET_JSON(hs_binary, "public_services");
    json_t *jn_service_descriptor = SDATA_GET_JSON(hs_binary, "service_descriptor");
    if(jn_public_services) {
        size_t index;
        json_t *jn_service;
        json_array_foreach(jn_public_services, index, jn_service) {
            const char *service = json_string_value(jn_service);
            if(empty_string(service)) {
                continue;
            }

            json_t *jn_descriptor = kw_get_dict_value(gobj, jn_service_descriptor, service, 0, 0);
            const char *description = kw_get_str(gobj, jn_descriptor, "description", "", 0);
            const char *schema = kw_get_str(gobj, jn_descriptor, "schema", "", 0);
            json_t *jn_connector = kw_get_dict_value(gobj, jn_descriptor, "connector", 0, 0);

            int port = 0;

            /*
             *  Check if already exists the service
             */
            json_t *hs_service = find_public_service(
                gobj,
                yuno_id,
                service
            );
            if(hs_service) {
                SDATA_SET_STR(hs_service, "description", description);
                SDATA_SET_STR(hs_service, "schema", schema);
                SDATA_SET_JSON(hs_service, "connector", jn_connector);
                port = SDATA_GET_INT(hs_service, "port");

            } else {
                json_t *kw_write_service = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}",
                    "realm_id", realm_id,
                    "service", service,
                    "description", description,
                    "schema", schema,
                    "yuno_role", yuno_role,
                    "yuno_name", yuno_name
                );
                if(jn_connector) {
                    json_object_set(kw_write_service, "connector", jn_connector);
                } else {
                    json_object_set_new(kw_write_service, "connector", json_object());
                }

                hs_service = gobj_create_node(
                    priv->resource,
                    resource,
                    kw_write_service,
                    json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                    gobj
                );
                if(!hs_service) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SERVICE_ERROR,
                        "msg",          "%s", "Cannot create service",
                        "yuno_role",    "%s", yuno_role,
                        "yuno_name",    "%s", yuno_name,
                        "service",      "%s", service,
                        NULL
                    );
                    ret += -1;
                    continue;
                }
                port = get_new_service_port(gobj, hs_realm);
                //SDATA_SET_INT(hs_realm, "last_port", port); DEPRECATED
                //hs_realm = gobj_update_node(
                //    priv->resource,
                //    "realms",
                //    hs_realm,
                //    json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                //    gobj
                //);
            }

            /*
             *  Write calculated fields: ip, port (__service_ip__, __service_port__)
             */
            const char *ip;
            BOOL public_ = SDATA_GET_BOOL(yuno, "global");
            if(public_) {
                ip = SDATA_GET_STR(hs_realm, "bind_ip");
            } else {
                ip = "127.0.0.1";
            }

            SDATA_SET_STR(hs_service, "ip", ip);
            SDATA_SET_INT(hs_service, "port", port);
            char url[128];
            snprintf(url, sizeof(url), "%s://%s:%d", schema, ip, port);
            SDATA_SET_STR(hs_service, "url", url);

            /*
             *  yuno_id will change with each new yuno release
             */
            json_object_set_new(hs_service, "yuno_id", json_string(yuno_id));
            json_decref(gobj_update_node(priv->resource, resource, hs_service, 0, gobj));
        }
    }
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int restart_nodes(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    /*----------------------------*
     *  Kill force all the yunos
     *----------------------------*/
    json_t *iter = gobj_list_nodes(
        priv->resource,
        "yunos",
        0, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    // Force kill
    int prev_signal2kill = gobj_read_integer_attr(gobj, "signal2kill");
    gobj_write_integer_attr(gobj, "signal2kill", SIGKILL);

    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Kill yuno
         */
        BOOL running = kw_get_bool(gobj, yuno, "yuno_running", 0, KW_REQUIRED);
        if(running) {
            hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
            if(channel_gobj) {
                gobj_write_user_data( // HACK release yuno info connection
                    channel_gobj,
                    "__yuno__",
                    json_string("")
                );
            }
            kill_yuno(gobj, yuno);
        }
    }
    JSON_DECREF(iter)
    // Restore kill
    gobj_write_integer_attr(gobj, "signal2kill", prev_signal2kill);

    /*----------------------------*
     *  Restart treedb
     *----------------------------*/
    gobj_stop(priv->resource);
    gobj_start(priv->resource);
    run_enabled_yunos(gobj);

    return ret;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_edit_config(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "configurations";

    const char *id = kw_get_str(gobj, kw, "id", 0, 0);
    if(id==0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("'id' required"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    KW_INCREF(kw);
    json_t *iter = gobj_list_instances(
        priv->resource,
        resource,
        "",
        kw, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    int found = json_array_size(iter);
    if(found != 1) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s",
                    found==0?
                        "Configuration not found":
                        "Too many configurations. Select only one"
                ),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    /*
     *  Convert result in json
     */
    json_t *jn_data = iter;

    /*
     *  Inform
     */
    json_t *webix = msg_iev_build_response(gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );

    return gobj_send_event(
        src,
        event,
        webix,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_view_config(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "configurations";

    const char *id = kw_get_str(gobj, kw, "id", 0, 0);
    if(id==0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("'id' required"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    KW_INCREF(kw);
    json_t *iter = gobj_list_instances(
        priv->resource,
        resource,
        "",
        kw, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    int found = json_array_size(iter);
    if(found != 1) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s",
                    found==0?
                        "Configuration not found":
                        "Too many configurations. Select only one"
                ),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    /*
     *  Convert result in json
     */
    json_t *jn_data = iter;

    /*
     *  Inform
     */
    json_t *webix = msg_iev_build_response(gobj,
        0,
        0,
        0,
        jn_data, // owned
        kw  // owned
    );

    return gobj_send_event(
        src,
        event,
        webix,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_edit_yuno_config(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf(
                    "Yuno not found"
                ),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    if(json_array_size(iter)!=1) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Select only one yuno please"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    json_t *yuno = json_array_get(iter, 0);

    /*------------------------------------------*
     *  Found the yuno, now get his config
     *------------------------------------------*/
    resource = "configurations";

    json_t *iter_config_ids = SDATA_GET_ITER(yuno, "config_ids");
    int found = json_array_size(iter_config_ids);
    if(found == 0) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Yuno without configuration"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    if(found > 1) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Yuno with too much configurations. Not supported"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    /*
     *  Convert result in json
     */
    json_t *jn_data = iter;

    /*
     *  Inform
     */
    json_t *webix = msg_iev_build_response(gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );

    return gobj_send_event(
        src,
        event,
        webix,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_view_yuno_config(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf(
                    "Yuno not found"
                ),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    if(json_array_size(iter)!=1) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Select only one yuno please"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    json_t *yuno = json_array_get(iter, 0);

    /*------------------------------------------*
     *  Found the yuno, now get his config
     *------------------------------------------*/
    resource = "configurations";

    json_t *iter_config_ids = SDATA_GET_ITER(yuno, "config_ids");
    int found = json_array_size(iter_config_ids);
    if(found == 0) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Yuno without configuration"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    if(found > 1) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Yuno with too much configurations. Not supported"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    /*
     *  Convert result in json
     */
    json_t *jn_data = iter;

    /*
     *  Inform
     */
    json_t *webix = msg_iev_build_response(gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );

    return gobj_send_event(
        src,
        event,
        webix,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_json(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    const char *filename = kw_get_str(gobj, kw, "filename", 0, 0);
    if(!filename) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("filename required"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    if(access(filename, 0)!=0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("File '%s' not found", filename),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    int fp = open(filename, 0);
    if(fp<0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Cannot open '%s', %s", filename, strerror(errno)),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    // TODO legacy optimiza preguntando el tamaño del fichero
    size_t len = gbmem_get_maximum_block();
    char *s = gbmem_malloc(len);
    if(!s) {
        close(fp);
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("No memory"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    int readed = read(fp, s, len-1);
    if(!readed) {
        int err = errno;
        close(fp);
        gbmem_free(s);
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Error with file '%s': %s", filename, strerror(err)),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    close(fp);

    char *p = strrchr(filename, '/');
    if(!p) {
        p = (char *)filename;
    } else {
        p++;
    }
    json_t *jn_s = 0; // TODO nonlegalstring2json(s, TRUE);
    json_t *jn_data = json_pack("{s:s, s:o}",
        "name", p,
        "zcontent", jn_s?jn_s:json_string("Invalid json in filename")
    );
    gbmem_free(s);

    /*
     *  Inform
     */
    return gobj_send_event(
        src,
        event,
        msg_iev_build_response(gobj,
            0,
            0,
            0,
            jn_data, // owned
            kw  // owned
        ),
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_file(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    const char *filename = kw_get_str(gobj, kw, "filename", 0, 0);
    if(!filename) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("filename required"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    if(access(filename, 0)!=0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("File '%s' not found", filename),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    int fp = open(filename, 0);
    if(fp<0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Cannot open '%s', %s", filename, strerror(errno)),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    // TODO legacy optimiza preguntando el tamaño del fichero
    size_t len = gbmem_get_maximum_block();
    char *s = gbmem_malloc(len);
    if(!s) {
        close(fp);
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("No memory"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    int readed = read(fp, s, len-1);
    if(!readed) {
        int err = errno;
        close(fp);
        gbmem_free(s);
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Error with file '%s': %s", filename, strerror(err)),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    close(fp);

    char *p = strrchr(filename, '/');
    if(!p) {
        p = (char *)filename;
    } else {
        p++;
    }
    json_t *jn_s = json_string(s);
    json_t *jn_data = json_pack("{s:s, s:o}",
        "name", p,
        "zcontent", jn_s?jn_s:json_string("Invalid content in filename")
    );
    gbmem_free(s);

    /*
     *  Inform
     */
    return gobj_send_event(
        src,
        event,
        msg_iev_build_response(gobj,
            0,
            0,
            0,
            jn_data, // owned
            kw  // owned
        ),
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_binary_file(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    const char *filename = kw_get_str(gobj, kw, "filename", 0, 0);
    if(!filename) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -200,
                json_sprintf("filename required"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    if(access(filename, 0)!=0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -201,
                json_sprintf("File '%s' not found", filename),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    size_t max_size = gbmem_get_maximum_block();
    uint64_t size = filesize(filename);
    if(size > max_size) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -205,
                json_sprintf("File '%s' too large. Maximum supported size is %zu",
                    filename,
                    max_size
                ),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    int fp = open(filename, 0);
    if(fp<0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -202,
                json_sprintf("Cannot open '%s', %s", filename, strerror(errno)),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    char *s = gbmem_malloc(size);
    if(!s) {
        close(fp);
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -203,
                json_sprintf("No memory"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    int readed = read(fp, s, size);
    if(readed!=size) {
        int err = errno;
        close(fp);
        gbmem_free(s);
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -204,
                json_sprintf("Error with file '%s': %s", filename, strerror(err)),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    close(fp);

    char *p = strrchr(filename, '/');
    if(!p) {
        p = (char *)filename;
    } else {
        p++;
    }

    gbuffer_t *gbuf_base64 = gbuffer_string_to_base64(s, size);

    json_t *jn_s = json_string(gbuffer_cur_rd_pointer(gbuf_base64));
    json_t *jn_data = json_pack("{s:s, s:o}",
        "name", p,
        "content64", jn_s?jn_s:json_string("Invalid content64")
    );
    gbmem_free(s);
    gbuffer_decref(gbuf_base64);

    /*
     *  Inform
     */
    return gobj_send_event(
        src,
        event,
        msg_iev_build_response(gobj,
            0,
            0,
            0,
            jn_data, // owned
            kw  // owned
        ),
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_running_keys(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf(
                    "Yuno not found"
                ),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    if(json_array_size(iter)!=1) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Select only one yuno please"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    json_t *yuno = json_array_get(iter, 0);

    /*------------------------------------------------*
     *  Walk over yunos iter
     *------------------------------------------------*/
    char bfbinary[NAME_MAX];
    gbuffer_t *gbuf_sh = gbuffer_create(4*1024, 32*1024);
    build_yuno_running_script(gobj, gbuf_sh, yuno, bfbinary, sizeof(bfbinary), TRUE);
    char *s = gbuffer_cur_rd_pointer(gbuf_sh);

    char temp[4*1024];
    snprintf(temp, sizeof(temp), "--config-file=\"%s\"", s);
    json_t *jn_s = json_string(temp);
    json_t *jn_data = json_pack("{s:s, s:o}",
        "name", "running-keys2",
        "zcontent", jn_s?jn_s:json_string("Invalid content in filename")
    );
    gbuffer_decref(gbuf_sh);
    JSON_DECREF(iter)

    /*
     *  Inform
     */
    return gobj_send_event(
        src,
        "EV_READ_FILE",
        msg_iev_build_response(gobj,
            0,
            0,
            0,
            jn_data, // owned
            kw  // owned
        ),
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_running_bin(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    /*------------------------------------------------*
     *      Get the yunos
     *------------------------------------------------*/
    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf(
                    "Yuno not found"
                ),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    if(json_array_size(iter)!=1) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("Select only one yuno please"),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    json_t *yuno = json_array_get(iter, 0);

    /*------------------------------------------------*
     *  Walk over yunos iter
     *------------------------------------------------*/
    char bfbinary[NAME_MAX];
    gbuffer_t *gbuf_sh = gbuffer_create(4*1024, 32*1024);
    build_yuno_running_script(gobj, gbuf_sh, yuno, bfbinary, sizeof(bfbinary), FALSE);

    json_t *jn_s = json_string(bfbinary);
    json_t *jn_data = json_pack("{s:s, s:o}",
        "name", "running-bin",
        "zcontent", jn_s?jn_s:json_string("Invalid content in filename")
    );
    gbuffer_decref(gbuf_sh);
    JSON_DECREF(iter)

    /*
     *  Inform
     */
    return gobj_send_event(
        src,
        "EV_READ_FILE",
        msg_iev_build_response(gobj,
            0,
            0,
            0,
            jn_data, // owned
            kw  // owned
        ),
        gobj
    );
}

/***************************************************************************
 *  Este mensaje llega directamente del channel superior (ievent_srv)
 ***************************************************************************/
PRIVATE int ac_play_yuno_ack(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Saco al originante por el user_data del canal.
     *  HACK aquí nos viene directamente el evento del canal,
     *  sin pasar por C_IOGATE (spiderden), y por lo tanto sin "_channel_gobj"
     *  porque el iev_srv no eleva ON_MESSAGE como los gossamer a spiderden,
     *  se lo queda, y procesa el inter-evento.
     *  Los mensajes ON_OPEN y ON_CLOSE del iogate:route nos llegan porque estamos
     *  suscritos a all ellos.
     */
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    const char *__yuno__ = json_string_value(gobj_read_user_data(channel_gobj, "__yuno__"));
    json_t *yuno = gobj_get_node(
        priv->resource,
        "yunos",
        json_pack("{s:s}", "id", __yuno__),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(!yuno) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "hs_yuno NULL",
            NULL
        );
        KW_DECREF(kw);
        return 0;
    }

    int action_return = kw_get_int(gobj, kw, "result", -1, 0);
    if(action_return == 0) {
        json_object_set_new(yuno, "yuno_playing", json_true());

        // Volatil if you don't want historic data
        // TODO legacy force volatil, sino no aparece el yuno con mas release el primero
        // y falla el deactivate-snap
        json_decref(
            gobj_update_node(
                priv->resource,
                "yunos",
                yuno,
                json_pack("{s:b}", "volatil", 1),
                src
            )
        );

        gobj_publish_event(
            gobj,
            event,
            kw // own kw
        );
    } else {
        json_object_set_new(yuno, "yuno_playing", json_false());

        // Volatil if you don't want historic data
        json_decref(
            gobj_update_node(
                priv->resource,
                "yunos",
                yuno,
                json_pack("{s:b}", "volatil", 1),
                src
            )
        );

        // TODO legacy no publique pues sale como ok, que dé error por timeout
        KW_DECREF(kw);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_pause_yuno_ack(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int action_return = kw_get_int(gobj, kw, "result", -1, 0);
    if(action_return == 0) {
        /*
         *  Saco al originante por el user_data del canal.
         */
        hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
        const char *__yuno__ = json_string_value(gobj_read_user_data(channel_gobj, "__yuno__"));
        json_t *yuno = gobj_get_node(
            priv->resource,
            "yunos",
            json_pack("{s:s}", "id", __yuno__),
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
        if(!yuno) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "hs_yuno NULL",
                NULL
            );
            KW_DECREF(kw);
            return 0;
        }
        json_object_set_new(yuno, "yuno_playing", json_false());
        // Volatil if you don't want historic data
        // TODO legacy force volatil, sino no aparece el yuno con mas release el primero
        // y falla el deactivate-snap
        json_decref(
            gobj_update_node(
                priv->resource,
                "yunos",
                yuno,
                json_pack("{s:b}", "volatil", 1),
                src
            )
        );

        gobj_publish_event(
            gobj,
            event,
            kw // own kw
        );
    } else {
        KW_DECREF(kw);
    }
    return 0;
}

/***************************************************************************
 *  HACK nodo intermedio
 ***************************************************************************/
PRIVATE int ac_stats_yuno_answer(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);

    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);

    hgobj gobj_requester = gobj_child_by_name(
        gobj_find_service("__input_side__", TRUE),
        dst_service
    );
    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "child not found",
            "child",        "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_ievent_id);

    KW_INCREF(kw);
    json_t *kw_redirect = 0; // TODO msg_iev_answer(gobj, kw, kw, 0);

    return gobj_send_event(
        gobj_requester,
        event,
        kw_redirect,
        gobj
    );
}

/***************************************************************************
 *  HACK nodo intermedio
 ***************************************************************************/
PRIVATE int ac_command_yuno_answer(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);

    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
    if(strcmp(dst_service, gobj_name(gobj))==0) {
        // Comando directo del agente
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }

    hgobj gobj_requester = gobj_child_by_name(
        gobj_find_service("__input_side__", TRUE),
        dst_service
    );
    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "child not found",
            "child",        "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_ievent_id);

    KW_INCREF(kw);
    json_t *kw_redirect = NULL; // TODO msg_iev_answer(gobj, kw, kw, 0);

    return gobj_send_event(
        gobj_requester,
        event,
        kw_redirect,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *client_yuno_role = kw_get_str(gobj, kw, "client_yuno_role", "", 0);
    if(strcasecmp(client_yuno_role, "yuneta_cli")==0 ||
            strcasecmp(client_yuno_role, "yuneta_agent")==0 ||
            strcasecmp(client_yuno_role, "ybatch")==0 ||
            strcasecmp(client_yuno_role, "ystats")==0 ||
            strcasecmp(client_yuno_role, "ycommand")==0 ||
            strcasecmp(client_yuno_role, "ytests")==0 ||
            strcasecmp(client_yuno_role, "GUI")==0 ||
            strcasecmp(client_yuno_role, "yuneta_gui")==0) {
        // let it.

        KW_DECREF(kw);
        return 0;
    }

    const char *yuno_id = kw_get_str(gobj, kw, "identity_card`yuno_id", 0, KW_REQUIRED);
    json_int_t pid = kw_get_int(gobj, kw, "identity_card`pid", 0, KW_REQUIRED);
    json_int_t watcher_pid = kw_get_int(gobj, kw, "identity_card`watcher_pid", 0, 0);
    BOOL playing = kw_get_bool(gobj, kw, "identity_card`playing", 0, KW_REQUIRED);
    const char *realm_id = kw_get_str(gobj, kw, "identity_card`realm_id", "", KW_REQUIRED);
    const char *yuno_role = kw_get_str(gobj, kw, "identity_card`yuno_role", "", KW_REQUIRED);
    const char *yuno_name = kw_get_str(gobj, kw, "identity_card`yuno_name", "", KW_REQUIRED);
    const char *yuno_release = kw_get_str(gobj, kw, "identity_card`yuno_release", "", KW_REQUIRED);
    const char *yuno_startdate= kw_get_str(gobj, kw, "identity_card`yuno_startdate", "", KW_REQUIRED);
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);

    json_t *kw_find = json_pack("{s:s, s:s, s:s, s:b, s:s, s:s}",
        "yuno_role", yuno_role,
        "yuno_name", yuno_name,
        "yuno_release", yuno_release,
        "yuno_disabled", 0,
        "id", yuno_id,
        "realm_id", realm_id
    );

    json_t *iter_yunos = gobj_list_nodes(
        priv->resource,
        "yunos",
        kw_find, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        channel_gobj
    );


    int found = json_array_size(iter_yunos);
    if(found==0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "yuno NOT FOUND",
            "yuno_id",      "%s", yuno_id,
            NULL
        );
        if(pid) {
            kill(pid, SIGKILL);
        }
        JSON_DECREF(iter_yunos);
        KW_DECREF(kw);
        return -1;
    }

    json_t *yuno = json_array_get(iter_yunos, 0);

    /*
     *  Check if it's already live.
     */
    uint32_t _pid = SDATA_GET_INT(yuno, "yuno_pid");
    if(_pid && getpgid(_pid) >= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "yuno ALREADY living, killing new yuno",
            "yuno_id",      "%s", yuno_id,
            "current pid",  "%d", (int)_pid,
            "new pid",      "%d", (int)pid,
            "yuno_role",    "%s", yuno_role,
            "yuno_name",    "%s", yuno_name,
            "yuno_release", "%s", yuno_release,
            NULL
        );
        if(pid) {
            kill(pid, SIGKILL);
        }
        KW_DECREF(kw);
        JSON_DECREF(iter_yunos);
        return -1;
    }

    if(strcmp(yuno_role, SDATA_GET_STR(yuno, "yuno_role"))!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "yuno_role not match",
            "yuno_role registered", "%s", SDATA_GET_STR(yuno, "yuno_role"),
            "yuno_role incoming",   "%s", yuno_role,
            "yuno_id",      "%s", yuno_id,
            "current pid",  "%d", (int)_pid,
            "new pid",      "%d", (int)pid,
            NULL
        );
        if(pid) {
            kill(pid, SIGKILL);
        }
        KW_DECREF(kw);
        JSON_DECREF(iter_yunos);
        return -1;
    }
    if(strcmp(yuno_name, SDATA_GET_STR(yuno, "yuno_name"))!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "yuno_name not match",
            "yuno_name registered", "%s", SDATA_GET_STR(yuno, "yuno_name"),
            "yuno_name incoming",   "%s", yuno_name,
            "yuno_role",    "%s", yuno_role,
            "yuno_id",      "%s", yuno_id,
            "current pid",  "%d", (int)_pid,
            "new pid",      "%d", (int)pid,
            NULL
        );
        if(pid) {
            kill(pid, SIGKILL);
        }
        KW_DECREF(kw);
        JSON_DECREF(iter_yunos);
        return -1;
    }
    if(strcmp(yuno_release, SDATA_GET_STR(yuno, "yuno_release"))!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "yuno_release not match",
            "yuno_release registered", "%s", SDATA_GET_STR(yuno, "yuno_release"),
            "yuno_release incoming","%s", yuno_release,
            "yuno_role",    "%s", yuno_role,
            "yuno_name",    "%s", yuno_name,
            "yuno_id",      "%s", yuno_id,
            "current pid",  "%d", (int)_pid,
            "new pid",      "%d", (int)pid,
            NULL
        );
        if(pid) {
            kill(pid, SIGKILL);
        }
        KW_DECREF(kw);
        JSON_DECREF(iter_yunos);
        return -1;
    }
    if(strcmp(realm_id, SDATA_GET_STR(yuno, "realm_id`0"))!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "realm_id not match",
            "realm_id registered", "%s", SDATA_GET_STR(yuno, "realm_id`0"),
            "realm_id incoming","%s", realm_id,
            "yuno_role",    "%s", yuno_role,
            "yuno_name",    "%s", yuno_name,
            "yuno_id",      "%s", yuno_id,
            "current pid",  "%d", (int)_pid,
            "new pid",      "%d", (int)pid,
            NULL
        );
        if(pid) {
            kill(pid, SIGKILL);
        }
        KW_DECREF(kw);
        JSON_DECREF(iter_yunos);
        return -1;
    }

    json_object_set_new(yuno, "yuno_startdate", json_string(yuno_startdate));
    json_object_set_new(yuno, "yuno_running", json_true());
    json_object_set_new(yuno, "yuno_playing", playing?json_true():json_false());
    json_object_set_new(yuno, "yuno_pid", json_integer(pid));
    json_object_set_new(yuno, "watcher_pid", json_integer(watcher_pid));

    json_object_set_new(yuno, "_channel_gobj", json_integer((json_int_t)(uintptr_t)channel_gobj));
    if(channel_gobj) {
        gobj_write_user_data(
            channel_gobj,
            "__yuno__",
            json_string(SDATA_GET_ID(yuno))
        );
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "channel_gobj NULL",
            NULL
        );
    }

    // Volatil if you don't want historic data
    // TODO legacy force volatil, sino no aparece el yuno con mas release el primero
    // y falla el deactivate-snap
    json_decref(
        gobj_update_node(
            priv->resource,
            "yunos",
            json_incref(yuno),
            json_pack("{s:b}", "volatil", 1),
            src
        )
    );

    gobj_log_debug(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", "yuno up",
        "yuno_id",      "%s", yuno_id,
        "pid",          "%d", (int)pid,
        "yuno_role",    "%s", yuno_role,
        "yuno_name",    "%s", yuno_name?yuno_name:"",
        "yuno_release", "%s", yuno_release?yuno_release:"",
        NULL
    );

    /*---------------*
     *  Check play
     *---------------*/
    if(!playing) {
        const char *solicitante = kw_get_str(gobj, yuno, "solicitante", "", 0);
        BOOL must_play = SDATA_GET_BOOL(yuno, "must_play");
        if(must_play) {
            hgobj gobj_requester = 0;
            if(!empty_string(solicitante)) {
                gobj_requester = gobj_child_by_name(
                    gobj_find_service("__input_side__", TRUE),
                    solicitante
                );
            }
            if(!gobj_requester) {
                play_yuno(gobj, yuno, 0, src);
            } else {
                json_t *kw_play = json_pack("{s:s, s:s}",
                    "realm_id", realm_id,
                    "id", yuno_id
                );
                cmd_play_yuno(gobj, "play-yuno", kw_play, gobj_requester);
            }
        }
        // se pierde, no existe el campo solicitante, change your mind! TODO legacy
        json_object_set_new(yuno, "solicitante", json_string(""));
        // Volatil if you don't want historic data
        // TODO legacy force volatil, sino no aparece el yuno con mas release el primero
        // y falla el deactivate-snap
        json_decref(
            gobj_update_node(
                priv->resource,
                "yunos",
                json_incref(yuno),
                json_pack("{s:b}", "volatil", 1),
                src
            )
        );
    }

    JSON_DECREF(iter_yunos);
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_shutdowning()) {
        KW_DECREF(kw);
        return 0;
    }
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    if(!channel_gobj) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "channel_gobj NULL",
            NULL
        );
        KW_DECREF(kw);
        return 0;
    }

    const char *__yuno__ = json_string_value(gobj_read_user_data(channel_gobj, "__yuno__"));
    json_t *yuno = gobj_get_node(
        priv->resource,
        "yunos",
        json_pack("{s:s}", "id", __yuno__),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(!yuno) {
        // Must be yuneta_cli or a yuno refused!.
        delete_consoles_on_disconnection(gobj, kw, src);
        KW_DECREF(kw);
        return 0;
    }

    gobj_write_user_data( // HACK release yuno info connection
        channel_gobj,
        "__yuno__",
        json_string("")
    );

    const char *realm_id = SDATA_GET_STR(yuno, "realm_id`0");
    const char *yuno_role = SDATA_GET_STR(yuno, "yuno_role");
    const char *yuno_name = SDATA_GET_STR(yuno, "yuno_name");
    const char *yuno_release = SDATA_GET_STR(yuno, "yuno_release");
    gobj_log_debug(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", "yuno down",
        "realm_id",     "%s", realm_id,
        "yuno_id",      "%s", SDATA_GET_STR(yuno, "id"),
        "pid",          "%d", (int)SDATA_GET_INT(yuno, "yuno_pid"),
        "yuno_role",    "%s", yuno_role,
        "yuno_name",    "%s", yuno_name?yuno_name:"",
        "yuno_release", "%s", yuno_release?yuno_release:"",
        NULL
    );

    json_object_set_new(yuno, "yuno_running", json_false());
    json_object_set_new(yuno, "yuno_playing", json_false());
    json_object_set_new(yuno, "yuno_pid", json_integer(0));
    json_object_set_new(yuno, "_channel_gobj", json_integer(0));

    // Volatil if you don't want historic data
    // TODO legacy force volatil, sino no aparece el yuno con mas release el primero
    // y falla el deactivate-snap
    json_decref(
        gobj_update_node(
            priv->resource,
            "yunos",
            yuno,
            json_pack("{s:b}", "volatil", 1),
            src
        )
    );

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_final_count(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *info = kw_get_str(gobj, kw, "info", "", 0);
    int max_count = kw_get_int(gobj, kw, "max_count", 0, 0);
    int cur_count = kw_get_int(gobj, kw, "cur_count", 0, 0);

// KKK

    json_t *iter_yunos = gobj_kw_get_user_data(src, "iter", 0, KW_EXTRACT);
    json_t *kw_answer = gobj_kw_get_user_data(src, "kw_answer", 0, KW_EXTRACT);

    json_t *jn_request = msg_iev_pop_stack(gobj, kw, "requester_stack");
    if(!jn_request) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "no requester_stack",
            NULL
        );
        gobj_trace_json(gobj, iter_yunos, "no requester_stack");
        JSON_DECREF(iter_yunos);
        KW_DECREF(kw_answer);
        KW_DECREF(kw);
        return -1;
    }

    const char *requester = kw_get_str(gobj, jn_request, "requester", 0, 0);
    hgobj gobj_requester = gobj_child_by_name(
        gobj_find_service("__input_side__", TRUE),
        requester
    );
    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "child not found",
            "child",        "%s", requester,
            NULL
        );
        JSON_DECREF(iter_yunos);
        JSON_DECREF(jn_request);
        KW_DECREF(kw_answer);
        KW_DECREF(kw);
        return 0;
    }

    BOOL ok = (max_count>0 && max_count==cur_count);

    json_t *jn_comment = json_sprintf("%s%s (%d raised, %d reached)\n",
        ok?"OK: ":"",
        info,
        max_count,
        cur_count
    );

    json_t *jn_data = gobj_list_nodes(
        priv->resource,
        "yunos",
        json_pack("{s:o}",
            "id", iter_yunos // owned
        ),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    /*
     *  Inform
     */
    json_t *webix = msg_iev_build_response(gobj,
        ok?0:-1,
        jn_comment, // owned
        tranger2_list_topic_desc_cols(
            gobj_read_pointer_attr(priv->resource, "tranger"),
            "yunos"
        ),
        jn_data,
        kw_answer  // owned
    );

    JSON_DECREF(jn_request);
    KW_DECREF(kw);

    return gobj_send_event(
        gobj_requester,
        "EV_MT_COMMAND_ANSWER",
        webix,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, gobj_name(src), 0, KW_REQUIRED);
    if(jn_console) {
        json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);

        const char *route_name; json_t *jn_route;
        json_object_foreach(jn_routes, route_name, jn_route) {
            const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
            const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
            hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
            if(!gobj_route_service) {
                continue;
            }
            hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
            if(!gobj_input_gate) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "no route child found",
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
                continue;
            }

            gobj_send_event(
                gobj_input_gate,
                "EV_TTY_OPEN",
                msg_iev_build_response(gobj,
                    0,  // result
                    0,  // comment
                    0,  // schema
                    json_incref(kw), // owned
                    json_incref(jn_route)  // owned
                ),
                gobj
            );
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_shutdowning()) {
        KW_DECREF(kw);
        return 0;
    }

    json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, gobj_name(src), 0, KW_EXTRACT);
    if(jn_console) {
        json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);

        const char *route_name; json_t *jn_route; void *n;
        json_object_foreach_safe(jn_routes, n, route_name, jn_route) {
            const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
            const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
            hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
            if(!gobj_route_service) {
                continue;
            }
            hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
            if(!gobj_input_gate) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "no route child found",
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
                continue;
            }

            json_t *consoles = gobj_kw_get_user_data(gobj_input_gate, "consoles", 0, 0);

            if(consoles) {
                json_object_del(consoles, gobj_name(src));
            }

            gobj_send_event(
                gobj_input_gate,
                "EV_TTY_CLOSE",
                msg_iev_build_response(gobj,
                    0,  // result
                    0,  // comment
                    0,  // schema
                    json_incref(kw), // owned
                    json_incref(jn_route)  // owned
                ),
                gobj
            );
        }

        json_decref(jn_console);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_console = kw_get_dict(gobj, priv->list_consoles, gobj_name(src), 0, KW_REQUIRED);
    if(jn_console) {
        json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);

        const char *route_name; json_t *jn_route;
        json_object_foreach(jn_routes, route_name, jn_route) {
            const char *route_service = kw_get_str(gobj, jn_route, "route_service", "", KW_REQUIRED);
            const char *route_child = kw_get_str(gobj, jn_route,  "route_child", "", KW_REQUIRED);
            hgobj gobj_route_service = gobj_find_service(route_service, TRUE);
            if(!gobj_route_service) {
                continue;
            }
            hgobj gobj_input_gate = gobj_child_by_name(gobj_route_service, route_child);
            if(!gobj_input_gate) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "no route child found",
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
                continue;
            }

            gobj_send_event(
                gobj_input_gate,
                EV_TTY_DATA,
                msg_iev_build_response(gobj,
                    0,  // result
                    0,  // comment
                    0,  // schema
                    json_incref(kw), // owned
                    json_incref(jn_route)  // owned
                ),
                gobj
            );
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_write_tty(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    const char *name= kw_get_str(gobj, kw, "name", 0, 0);
    const char *content64 = kw_get_str(gobj, kw, "content64", 0, 0);
    if(empty_string(content64)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "content64 required",
            "name",         "%s", name,
            NULL
        );
        gobj_send_event(src, EV_DROP, 0, gobj);
        KW_DECREF(kw);
        return 0;
    }

    hgobj gobj_console = gobj_find_service(name, FALSE);
    if(!gobj_console) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "console not found",
            "name",         "%s", name,
            NULL
        );
        gobj_send_event(src, EV_DROP, 0, gobj);
        KW_DECREF(kw);
        return 0;
    }

    gbuffer_t *gbuf = gbuffer_base64_to_string(content64, strlen(content64));
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "Bad data",
            "name",         "%s", name,
            NULL
        );
        gobj_send_event(src, EV_DROP, 0, gobj);
        KW_DECREF(kw);
        return 0;
    }

    json_t *kw_tty = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    gobj_send_event(gobj_console, EV_WRITE_TTY, kw_tty, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!priv->enabled_yunos_running) {
        priv->enabled_yunos_running = 1;
        run_enabled_yunos(gobj);
        exec_startup_command(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***********************************************************************
 *                          FSM
 ***********************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create   = mt_create,
    .mt_destroy  = mt_destroy,
    .mt_start    = mt_start,
    .mt_stop     = mt_stop,
    .mt_writing  = mt_writing,
    .mt_trace_on = mt_trace_on,
    .mt_trace_off= mt_trace_off,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(GCLASS_AGENT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_READ_RUNNING_KEYS);
GOBJ_DEFINE_EVENT(EV_READ_RUNNING_BIN);

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
        {EV_EDIT_CONFIG,          ac_edit_config,         0},
        {EV_VIEW_CONFIG,          ac_view_config,         0},
        {EV_EDIT_YUNO_CONFIG,     ac_edit_yuno_config,    0},
        {EV_VIEW_YUNO_CONFIG,     ac_view_yuno_config,    0},
        {EV_READ_JSON,            ac_read_json,           0},
        {EV_READ_FILE,            ac_read_file,           0},
        {EV_READ_BINARY_FILE,     ac_read_binary_file,    0},
        {EV_READ_RUNNING_KEYS,    ac_read_running_keys,   0},
        {EV_READ_RUNNING_BIN,     ac_read_running_bin,    0},

        {EV_PLAY_YUNO_ACK,        ac_play_yuno_ack,       0},
        {EV_PAUSE_YUNO_ACK,       ac_pause_yuno_ack,      0},
        {EV_MT_STATS_ANSWER,      ac_stats_yuno_answer,   0},
        {EV_MT_COMMAND_ANSWER,    ac_command_yuno_answer, 0},
        {EV_ON_COMMAND,           ac_command_yuno_answer, 0},

        {EV_ON_OPEN,              ac_on_open,             0},
        {EV_ON_CLOSE,             ac_on_close,            0},
        {EV_FINAL_COUNT,          ac_final_count,         0},
        {EV_TTY_DATA,             ac_tty_data,            0},
        {EV_TTY_OPEN,             ac_tty_open,            0},
        {EV_TTY_CLOSE,            ac_tty_close,           0},
        {EV_WRITE_TTY,            ac_write_tty,           0},
        {EV_TIMEOUT,              ac_timeout,             0},
        {EV_STOPPED,              0,                      0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                 st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events table
     *------------------------*/
    event_type_t event_types[] = {
        /* Public input (requests) */
        {EV_EDIT_CONFIG,          EVF_PUBLIC_EVENT},
        {EV_VIEW_CONFIG,          EVF_PUBLIC_EVENT},
        {EV_EDIT_YUNO_CONFIG,     EVF_PUBLIC_EVENT},
        {EV_VIEW_YUNO_CONFIG,     EVF_PUBLIC_EVENT},
        {EV_READ_JSON,            EVF_PUBLIC_EVENT},
        {EV_READ_FILE,            EVF_PUBLIC_EVENT},
        {EV_READ_BINARY_FILE,     EVF_PUBLIC_EVENT},
        {EV_READ_RUNNING_KEYS,    EVF_PUBLIC_EVENT},
        {EV_READ_RUNNING_BIN,     EVF_PUBLIC_EVENT},
        {EV_ON_COMMAND,           EVF_PUBLIC_EVENT},

        /* Non-public inputs */
        {EV_TTY_DATA,             0},
        {EV_TTY_OPEN,             0},
        {EV_TTY_CLOSE,            0},
        {EV_WRITE_TTY,            0},
        {EV_ON_OPEN,              0},
        {EV_ON_CLOSE,             0},
        {EV_FINAL_COUNT,          0},
        {EV_TIMEOUT,              0},
        {EV_STOPPED,              0},

        /* Publications (outputs) */
        {EV_PLAY_YUNO_ACK,        EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_PAUSE_YUNO_ACK,       EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_MT_STATS_ANSWER,      EVF_OUTPUT_EVENT},
        {EV_MT_COMMAND_ANSWER,    EVF_OUTPUT_EVENT},

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
        0,              /* lmt (none) */
        tattr_desc,
        sizeof(PRIVATE_DATA),
        authz_table,    /* acl */
        command_table,  /* command table */
        s_user_trace_level,
        0               /* gcflag */
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *          Public access
 ***************************************************************************/
PUBLIC int register_c_agent(void)
{
    return create_gclass(GCLASS_AGENT);
}
