/***********************************************************************
 *          C_AGENT.C
 *          Agent GClass.
 *
 *          Yuneta Agent, the first authority of realms and yunos in a host
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <grp.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include <c_pty.h>
#include "c_agent.h"
#include "agent_treedb.h"
#include "audit_record.h"
#include "dir_listing.h"
#include "file_content.h"
#include "find_new_yunos.h"
#include "restart_plan.h"
#include "watch_request.h"
#include "yuno_config_file.h"

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

#define AUDIT_TTY_BURST_SECONDS 60  // console writes: one record per burst, see audit_tty_command()


/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE const char *sequence_name(hgobj gobj);
PRIVATE const char *last_failure_cause(void);
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
PRIVATE BOOL is_safe_binary_name(hgobj gobj, const char *what, const char *name);
PRIVATE char *binary_version_dir(
    hgobj gobj,
    char *bf,
    int bfsize,
    json_t *jn_tags, // not owned
    const char *role,
    const char *version
);
PRIVATE json_t *get_yuno_realm(hgobj gobj, json_t *yuno);
PRIVATE char * build_yuno_private_domain(hgobj gobj, json_t *yuno, char *bf, int bfsize);
PRIVATE int build_role_plus_name(hgobj gobj, char *bf, int bf_len, json_t *yuno);
PRIVATE int build_role_plus_id(hgobj gobj, char *bf, int bf_len, json_t *yuno);
PRIVATE char * build_yuno_bin_path(hgobj gobj, json_t *yuno, char *bf, int bfsize, BOOL create_dir);
PRIVATE int remove_temp_config_files_of_yunos(hgobj gobj);
PRIVATE char * build_yuno_log_path(hgobj gobj, json_t *yuno, char *bf, int bfsize, BOOL create_dir);
PRIVATE void sort_yunos_by_start_priority(hgobj gobj, json_t *iter, BOOL ascending);
PRIVATE int run_yuno(
    hgobj gobj,
    json_t *yuno,
    hgobj src
);
PRIVATE BOOL is_launching(hgobj gobj, const char *yuno_id);
PRIVATE json_t *find_living_yuno_pids(hgobj gobj, json_t *yuno);
PRIVATE BOOL yuno_lives_unregistered(hgobj gobj, json_t *yuno);
PRIVATE json_t *signal_unconnected_yunos(hgobj gobj, json_t *kw_filter, BOOL app, int signal2kill, hgobj src);
PRIVATE void kill_yuno_unregistered(hgobj gobj, json_t *yuno, json_t *jn_killed);
PRIVATE void restart_wait_tick(hgobj gobj);
PRIVATE int read_process_stat(hgobj gobj, pid_t pid, char *state, uint64_t *start_time);
PRIVATE BOOL process_is_gone(hgobj gobj, pid_t pid, uint64_t start_time);
PRIVATE void restart_spare_tick(hgobj gobj);
PRIVATE BOOL launch_spared_yuno(hgobj gobj, const char *yuno_id, json_t *jn_pids);
PRIVATE void forget_spared_yuno(hgobj gobj, const char *yuno_id);
PRIVATE void hold_restarted_yuno(hgobj gobj, const char *yuno_id, BOOL only_waited, const char *why);
PRIVATE void unhold_restarted_yuno(hgobj gobj, const char *yuno_id, const char *why);
PRIVATE void launch_enabled_yuno(hgobj gobj, json_t *yuno, BOOL spare_the_living);
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
PRIVATE json_t *unreached_yunos_response(
    hgobj gobj,
    const char *what,
    json_t *jn_unreached,
    int sent,
    json_t *kw
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
    hgobj gobj, json_t *yuno, json_t* kw, hgobj src
);
PRIVATE int audit_command_cb(const char *command, json_t *kw, void *user_data);
PRIVATE void write_audit_records(hgobj gobj, json_t *jn_records);
PRIVATE int audit_newfile_cb(void *user_data, const char *old_filename, const char *new_filename);
PRIVATE int remove_old_audit_files(hgobj gobj);

PRIVATE int build_release_name(
    hgobj gobj, char *bf, int bfsize, json_t *hs_binary, json_t *hs_config
);

PRIVATE int register_public_services(
    hgobj gobj,
    json_t *yuno, // not owned
    json_t *hs_binary, // not owned
    json_t *hs_realm // not owned
);
PRIVATE int restart_nodes(hgobj gobj, BOOL all);
PRIVATE void note_killed(hgobj gobj, json_t *jn_killed, const char *yuno_id, json_t *jn_pids);

PRIVATE int add_console_in_input_gate(hgobj gobj, const char *name, hgobj src);
PRIVATE int add_console_route(
    hgobj gobj,
    const char *name,
    json_t *jn_console,
    hgobj src,
    json_t *kw
);
PRIVATE int delete_console(hgobj gobj, const char *name);
PRIVATE BOOL requester_is_listening(hgobj gobj, hgobj requester, gobj_event_t event);
PRIVATE int watch_rearm_timer(hgobj gobj);
PRIVATE int watch_tick(hgobj gobj, const char *only_route);
PRIVATE int publications_arm(hgobj gobj, json_t *yuno, const char *yuno_id, json_t *jn_services);
PRIVATE BOOL publication_is_on(hgobj gobj, const char *yuno_id, const char *service);
PRIVATE int publications_stop(hgobj gobj, const char *yuno_id);
PRIVATE int publication_answer(hgobj gobj, json_t *jn_publish_md, json_t *kw);
PRIVATE int publication_ask(hgobj gobj, json_t *yuno, const char *yuno_id, const char *service, json_int_t pid, BOOL stop);
GOBJ_DECLARE_EVENT(EV_WATCH_READING);
PRIVATE int watch_answer(hgobj gobj, json_t *jn_watch_md, json_t *kw);
PRIVATE int delete_watch_of_channel(hgobj gobj, hgobj channel_gobj);
PRIVATE int get_last_public_port(hgobj gobj);

/***************************************************************************
 *              Resources
 ***************************************************************************/


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE const char *pidfile = "/yuneta/realms/agent/yuneta_agent.pid";

// Let insecure connection
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
                'remote_yuno_role': 'yuneta_agent',           \n\
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
                                    'children': [                           \n\
                                        {                                   \n\
                                            'name': 'agent_client',         \n\
                                            'gclass': 'C_TCP',              \n\
                                            'kw': {                         \n\
                                                'url':'ws://127.0.0.1:1991' \n\
                                            }                               \n\
                                        }                                   \n\
                                    ]                                       \n\
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
PRIVATE json_t *cmd_watch_yuno_stats(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_command_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_stats_agent(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_graceful_kill(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
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

PRIVATE json_t *cmd_cert_sync_now(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_cert_sync_status(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE int cert_sync_tick(hgobj gobj);
PRIVATE int cert_sync_arm_timer(hgobj gobj);
PRIVATE int cert_sync_broadcast_reload(hgobj gobj);

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
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id (required)"),
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
SDATAPM (DTP_BOOLEAN,   "play",         0,              "1",        "Auto-play on connect (set play=0 to only launch; issue play-yuno later)"),
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
SDATAPM (DTP_STRING,    "service",      0,              0,          "Target service (default: matched yuno_role; pass `__yuno__` to query the top C_YUNO attrs)"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm Id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno Role"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno Name"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno Release"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_watch_yuno_stats[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "ids",          0,              "",         "Yuno ids to watch, comma separated. `id:service` reads that service's counters instead of the one named as the role"),
SDATAPM (DTP_INTEGER,   "period",       0,              "2000",     "Milliseconds between two readings (never under watch_min_period)"),
SDATAPM (DTP_BOOLEAN,   "stop",         0,              "0",        "Stop the watch of this requester"),
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
SDATAPM (DTP_STRING,    "content64",    SDF_SECRET,     0,          "Content64 data to write to tty: keystrokes, a typed password among them"),
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
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id (required)"),
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
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id (required)"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "Binary version: that version alone; none, or the primary's, deletes every version"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force delete: in use, or tagged by a snap"),
SDATAPM (DTP_BOOLEAN,   "record_only",  0,              0,          "Delete the record only, its files stay: the way to remove a record with an unsafe name"),
SDATA_END()
};


PRIVATE sdata_desc_t pm_create_config[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id"),
SDATAPM (DTP_STRING,    "content64",    SDF_SECRET,     0,          "Content in base64: a configuration, its credentials among it"),
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
SDATAPM (DTP_STRING,    "content64",    SDF_SECRET,     0,          "Content in base64: a configuration, its credentials among it"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_delete_config[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id (required)"),
SDATAPM (DTP_STRING,    "version",      0,              0,          "Config version: that version alone; none, or the primary's, deletes every version"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force delete"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_find_new_yunos[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_BOOLEAN,   "create",       0,              0,          "Create new found yunos"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Take a release that does NOT move forward"),
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
SDATAPM (DTP_INTEGER,   "start_priority",0,             0,          "Launch tier (0..9, ascending); inherited on version bump"),
SDATAPM (DTP_INTEGER,   "sched_priority",0,             0,          "sched_setscheduler priority (applied only when cpu_core > 0)"),
SDATAPM (DTP_INTEGER,   "cpu_core",     0,              0,          "CPU core to pin to (0 = no pinning)"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_delete_yuno[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "id",           0,              0,          "Id (required)"),
SDATAPM (DTP_STRING,    "realm_id",     0,              0,          "Realm id"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              0,          "Yuno role"),
SDATAPM (DTP_STRING,    "role_version", 0,              0,          "Role version"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              0,          "Yuno name"),
SDATAPM (DTP_STRING,    "name_version", 0,              0,          "Name version"),
SDATAPM (DTP_STRING,    "yuno_tag",     0,              0,          "Yuno Tag"),
SDATAPM (DTP_STRING,    "yuno_release", 0,              0,          "Yuno release; if given, delete that exact release (not with whole)"),
SDATAPM (DTP_BOOLEAN,   "whole",        0,              0,          "Delete the WHOLE yuno and all of its releases (not with yuno_release)"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force delete (bypass the snap-tag guard; does NOT imply whole)"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_check_json[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER, "max_refcount",   0,              "0",        "Also report the jsons held more than this many times (0: only the corrupted ones)"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_snap_content[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "snap_id",      0,              0,          "Snap id (numeric, as listed by `snaps`)"),
SDATAPM (DTP_STRING,    "id",           0,              0,          "Snap id (alias of snap_id)"),
SDATAPM (DTP_STRING,    "name",         0,              0,          "Snap name (resolved to its id)"),
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name (optional: omit for the per-topic overview)"),
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
SDATAPM (DTP_BOOLEAN,   "all",          0,              "0",        "Restart every yuno, not only the ones whose launch changes"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_deactivate_snap[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_BOOLEAN,   "all",          0,              "0",        "Restart every yuno, not only the ones whose launch changes"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_open_console[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              "",         "Name of console"),
SDATAPM (DTP_STRING,    "process",      0,              "bash",     "Process to execute"),
SDATAPM (DTP_STRING,    "cwd",          0,              0,          "Current work directory"),
SDATAPM (DTP_INTEGER,   "cx",           0,              "80",       "Columns"),
SDATAPM (DTP_INTEGER,   "cy",           0,              "24",       "Rows"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_close_console[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              "",         "Name of console"),
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

PRIVATE const char *a_create_config[] = {"install-config", 0};

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
SDATACM2 (DTP_SCHEMA,   "set-graceful-kill",0,                  0,                  0,              cmd_set_graceful_kill, "Set kill mode = graceful (SIGQUIT, yuno shuts down itself)"),
SDATACM2 (DTP_SCHEMA,   "set-quick-kill",   0,                  0,                  0,              cmd_set_qkill,  "Kill yunos with SIGKILL, quick kill"),
SDATACM2 (DTP_SCHEMA,   "cert-sync-now",    SDF_AUTHZ_X,        0,                  0,              cmd_cert_sync_now,"Run cert auto-sync immediately (copy letsencrypt + reload every yuno)"),
SDATACM2 (DTP_SCHEMA,   "cert-sync-status", SDF_AUTHZ_X,        0,                  0,              cmd_cert_sync_status,"Show cert auto-sync state (last check, last action, failures)"),
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
SDATACM2 (DTP_SCHEMA,   "running-keys",    0,                   a_read_running_keys,pm_running_keys,0,              "Read yuno running parameters"),
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
SDATACM2 (DTP_SCHEMA,   "create-config",    0,                  a_create_config,    pm_create_config,cmd_create_config, "Create configuration (alias install-config, by analogy with install-binary)"),
SDATACM2 (DTP_SCHEMA,   "edit-config",      0,                  a_edit_config,      pm_edit_config, 0,              "Edit configuration"),
SDATACM2 (DTP_SCHEMA,   "view-config",      0,                  a_view_config,      pm_view_config, 0,              "View configuration"),
SDATACM2 (DTP_SCHEMA,   "update-config",    0,                  0,                  pm_update_config,cmd_update_config, "Update configuration"),
SDATACM2 (DTP_SCHEMA,   "delete-config",    0,                  0,                  pm_delete_config,cmd_delete_config, "Delete configuration"),
SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              ""),
SDATACM2 (DTP_SCHEMA,   "find-new-yunos",   0,                  0,                  pm_find_new_yunos,cmd_find_new_yunos, "List the create-yuno rows for binaries/configs newer than the primary"),
SDATACM2 (DTP_SCHEMA,   "create-yuno",      0,                  0,                  pm_create_yuno, cmd_create_yuno, "Create a yuno; with role_version/name_version, a new release of one existing"),
SDATACM2 (DTP_SCHEMA,   "delete-yuno",      0,                  0,                  pm_delete_yuno, cmd_delete_yuno, "Delete a release (yuno_release=...) or the whole yuno (whole=1), one of them"),
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
SDATACM2 (DTP_SCHEMA,   "activate-snap",    0,                  0,                  pm_activate_snap,cmd_activate_snap,"Activate snap; restarts the yunos whose launch changes (all=1: every yuno)"),
SDATACM2 (DTP_SCHEMA,   "deactivate-snap",  0,                  0,                  pm_deactivate_snap,cmd_deactivate_snap,"De-Activate snap; restarts the yunos whose launch changes (all=1: every yuno)"),
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
SDATACM2 (DTP_SCHEMA,   "watch-yuno-stats", 0,                  0,                  pm_watch_yuno_stats, cmd_watch_yuno_stats, "Send the stats of yunos to the requester every period (EV_YUNO_STATS), until stop=1 or its channel closes"),
SDATACM2 (DTP_SCHEMA,   "authzs-yuno",      SDF_WILD_CMD,       0,                  pm_authzs_yuno,  cmd_authzs_yuno, "Get permissions of yuno"),
SDATACM2 (DTP_SCHEMA,   "ping",             0,                  0,                  0,   cmd_ping,       "Ping command"),
SDATACM2 (DTP_SCHEMA,   "node-uuid",        0,                  a_uuid,             0,   cmd_uuid,       "Get uuid of node"),

SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_STRING,      "__username__",     SDF_RD,             "",             "Username 'yuneta', permission for all"),
SDATA (DTP_STRING,      "startup_command",  SDF_RD,             0,              "Command to execute at startup"),
SDATA (DTP_JSON,        "agent_environment",SDF_RD,             0,              "Agent environment. Override the yuno environment"),
SDATA (DTP_JSON,        "node_variables",   SDF_RD,             0,              "Global to Node json config variables"),
SDATA (DTP_INTEGER,     "timerStBoot",      SDF_RD,             "4000",         "Timer to run yunos on boot"),
SDATA (DTP_INTEGER,     "signal2kill",      SDF_RD,             "3",            "Signal to kill yunos:SIGQUIT"),

SDATA (DTP_JSON,        "range_ports",      SDF_RD,             "[[11100,11199]]", "Range Ports. List of ports to be assigned to public services of yunos."),
SDATA (DTP_INTEGER,     "last_port",        SDF_WR,             0,              "Last port assigned"),
SDATA (DTP_INTEGER,     "max_consoles",     SDF_WR,             "30",           "Maximum consoles opened"),
SDATA (DTP_INTEGER,     "max_watches",      SDF_WR,             "30",           "Maximum requesters with a watch-yuno-stats"),
SDATA (DTP_INTEGER,     "max_watch_ids",    SDF_WR,             "256",          "Maximum yuno ids (id or id:service) in one watch-yuno-stats"),
SDATA (DTP_INTEGER,     "watch_min_period", SDF_WR,             "1000",         "Minimum milliseconds between two readings of a watch-yuno-stats"),
SDATA (DTP_INTEGER,     "watch_ttl",        SDF_WR,             "60000",        "Milliseconds a watch-yuno-stats lives unless asked again (a requester behind a control center is never seen leaving). 0 = until stop or its channel closes"),
SDATA (DTP_INTEGER,     "timeout_expiration",SDF_WR,            "30000",        "Expiration timeout for commands"),

SDATA (DTP_BOOLEAN,     "use_audit_command_file",SDF_WR,        "1",            "Use audit file commands"),
SDATA (DTP_INTEGER,     "max_megas_audit_file",SDF_WR,          "500",          "max megas rotatory file size"),
SDATA (DTP_INTEGER,     "min_free_disk_percentage",SDF_WR,      "20",           "min free disk percentage using audit file"),
SDATA (DTP_INTEGER,     "audit_keep_days",  SDF_WR,             "7",            "Days of audit files kept. Older audit files are removed at start and when a new audit file begins. 0 = keep all"),

/*  Certificate auto-sync: self-healing path independent of the certbot
 *  deploy hook. On every tick the agent runs the copy-certs script (as
 *  root via sudo, which yuneta has NOPASSWD for) and, if the files under
 *  cert_sync_store_dir changed, broadcasts reload-certs to all running
 *  yunos — so expired certificates get replaced even when the hook fails.
 */
SDATA (DTP_BOOLEAN,     "cert_sync_enabled",     SDF_WR|SDF_PERSIST, "1",           "Enable periodic TLS certificate auto-sync + hot-reload"),
SDATA (DTP_INTEGER,     "cert_sync_interval_sec",SDF_WR|SDF_PERSIST, "900",         "Seconds between cert-sync checks (default 15 min)"),
SDATA (DTP_STRING,      "cert_sync_store_dir",   SDF_RD,             "/yuneta/store/certs", "Directory watched for cert file changes (owned by yuneta). Config only"),
SDATA (DTP_STRING,      "cert_sync_copy_cmd",    SDF_RD,             "/usr/bin/sudo -n /yuneta/store/certs/copy-certs.sh", "Command to copy fresh letsencrypt certs into cert_sync_store_dir (runs root, with sh -c). Config only: up to 7.25.21 write-attr could set it, and persist it"),
SDATA (DTP_INTEGER,     "cert_sync_last_check",  SDF_RSTATS,         "0",           "Unix ts of last cert-sync check"),
SDATA (DTP_INTEGER,     "cert_sync_last_action", SDF_RSTATS,         "0",           "Unix ts of last time a cert change was applied"),
SDATA (DTP_STRING,      "cert_sync_last_result", SDF_RSTATS,         "",            "Last cert-sync outcome (ok / skipped / error)"),
SDATA (DTP_INTEGER,     "cert_sync_failures",    SDF_RSTATS,         "0",           "Cumulative cert-sync failures since startup"),

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
    BOOL util_yunos_running;

    hgobj gobj_authz;
    hgobj gobj_input_side;

    hgobj gobj_treedbs;             // manager of multiple treedbs
    hgobj gobj_treedb_agentdb;      // service of treedb_agentdb (create in gobj_treedbs)
    // json_t *tranger_treedb_agentdb;
    char treedb_agentdb_name[80];

    json_t *list_consoles; // Dictionary of console names

    json_t *watches;        // watch-yuno-stats, by route: {route, yunos: {id: service}, period}
    json_t *publishing;     // publish-stats asked to the watched yunos: {id: {pid, period, renew, services: {service: state}}}
    hgobj watch_timer;
    json_int_t watch_period; // period the watch timer runs at, 0 = stopped

    hgobj restart_wait_timer;   // restart_nodes(): the yunos killed must be gone before the relaunch
    json_t *restart_wait;       // restart_nodes(): {yuno_id: [[pid, start_time] killed, not seen gone yet]}
    uint64_t restart_wait_until; // msectimer: the relaunch goes on anyway
    BOOL restart_sparing;       // after the wait: the yunos left alive are watched, each launched when gone
    json_t *restart_held;       // restart_nodes(): ids the operator stopped or disabled in its first wait

    json_t *no_play_launches; // set of launch_id (string key) launched with run-yuno play=0

    /*
     *  Set of yuno id (string key) already launched by run_yuno() and not yet
     *  registered by its EV_ON_OPEN. The boot sweeps guard on `yuno_running`,
     *  which only turns TRUE in ac_on_open(): without this, a yuno slower than
     *  timerStBoot to open (one loading a treedb) is launched a second time by
     *  run_enabled_yunos() while its first instance is still coming up.
     */
    json_t *launching_yunos;

    hgobj resource;
    hgobj timer;
    hrotatory_h audit_file;
    json_t *audit_tty_bursts;   // console writes being counted, see audit_tty_command()

    json_int_t timeout_expiration;

    /* cert auto-sync (see attrs cert_sync_*) */
    hgobj cert_sync_timer;
    BOOL cert_sync_enabled;
    int32_t cert_sync_interval_sec;
    json_t *cert_sync_state; // dict: filename -> "size:mtime", snapshot of store_dir
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
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "yuneta_agent: node_owner EMPTY, setting none",
            NULL
        );
        print_error(0, "yuneta_agent: node_owner EMPTY, setting none");
    }

    /*----------------------------------------*
     *      Check user yuneta
     *----------------------------------------*/
    gobj_write_str_attr(
        gobj,
        "__username__",
        gobj_read_str_attr(gobj_yuno(), "__username__")
    );

    /*---------------------------------------*
     *      Create timer to start yunos
     *---------------------------------------*/
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*---------------------------------------*
     *      Cert auto-sync timer (child). It
     *      fires EV_TIMEOUT on this gobj; we
     *      tell it apart by checking `src`.
     *---------------------------------------*/
    priv->cert_sync_timer = gobj_create_pure_child("cert_sync", C_TIMER, 0, gobj);
    priv->cert_sync_state = json_object();

    /*---------------------------------------*
     *      watch-yuno-stats timer (child),
     *      told apart by `src` too.
     *---------------------------------------*/
    priv->watch_timer = gobj_create_pure_child("watch_stats", C_TIMER, 0, gobj);
    priv->watches = json_object();
    priv->publishing = json_object();

    /*---------------------------------------*
     *      restart_nodes() waits for the
     *      yunos it killed (child, by src)
     *---------------------------------------*/
    priv->restart_wait_timer = gobj_create_pure_child("restart_wait", C_TIMER, 0, gobj);

    /*---------------------------------------*
     *      Check if already running
     *---------------------------------------*/
    {
        int pid = 0;

        FILE *file = fopen(pidfile, "r");
        if(file) {
            int x = fscanf(file, "%d", &pid);
            fclose(file);

            if(x != 1 || pid <= 0) {
                // pid 0 would be read as this very process (/proc/self)
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_OPERATIONAL,
                    "msg",          "%s", "pid file without a pid, removed",
                    "pid_file",     "%s", pidfile,
                    NULL
                );
                unlink(pidfile);
            } else if(pid != getpid()) {
                int ret = is_yuneta_agent(pid);
                if(ret == 0) {
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INFO,
                        "msg",          "%s", "yuneta_agent already running, exiting",
                        "pid_in_file",  "%d", pid,
                        NULL
                    );
                    exit(0);
                } else if(ret == 1) {
                    unlink(pidfile);
                } else if(ret == 2) {
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_OPERATIONAL,
                        "msg",          "%s", "pid file left by an agent that did not end in order: its pid is not an agent now (killed and not yet reaped, or taken by another program), removed",
                        "pid_in_file",  "%d", pid,
                        NULL
                    );
                    unlink(pidfile);
                } else {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM,
                        "msg",          "%s", "cannot check pid",
                        "pid_in_file",  "%d", pid,
                        "errno",        "%d", errno,
                        "serrno",       "%s", strerror(errno),
                        NULL
                    );
                    unlink(pidfile);
                }
            }
        }
        file = fopen(pidfile, "w");
        if(file) {
            fprintf(file, "%d\n", getpid());
            fclose(file);
        }

        if (!atexit_registered) {
            atexit(remove_pid_file);
            atexit_registered = 1;
        }
    }

    /*-----------------------------*
     *      Audit
     *-----------------------------*/
    if(gobj_read_bool_attr(gobj, "use_audit_command_file")) {
        /*
         *  ONE policy: an agent whose commands cannot be audited does not
         *  run. The path is only BUILT here (create FALSE): the audit
         *  directory and the file are both made by rotatory_open() with
         *  exit_on_fail, as the file log of every yuno, so a directory or
         *  a file that cannot be made stops the agent at start (stdout and
         *  syslog say why, and its watcher relaunches it until the cause
         *  is fixed: 2 s after the first end, then with a wait that doubles
         *  up to 60 s, see ydaemon.c). Up to 7.29.3 a directory that could
         *  not be made left an empty path, and the agent ran with no audit.
         */
        char audit_path[PATH_MAX];
        if(yuneta_realm_file(audit_path, sizeof(audit_path), "audit", "ZZZ-DD_MM_CCYY.log", FALSE)) {
            priv->audit_file = rotatory_open(
                audit_path,
                0,                      // 0 = default 64K
                gobj_read_integer_attr(gobj, "max_megas_audit_file"), // 0 = default 8
                gobj_read_integer_attr(gobj, "min_free_disk_percentage"), // 0 = default 10
                yuneta_xpermission(),   // permission for directories and executable files. 0 = default 02775
                0660,                   // permission for regular files. 0 = default 0664
                TRUE
            );
        }
        if(!priv->audit_file) {
            /*
             *  No path (no root or domain directory: both are in the fixed
             *  config of main.c; or longer than PATH_MAX, logged by
             *  build_path()), or rotatory not started up (entry_point does
             *  it). Nothing of the filesystem failed here, so no errno: the
             *  parts of the path are what tells the cause.
             */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Command audit: the path of the audit file cannot be built, the agent does not run",
                "root_dir",     "%s", yuneta_root_dir(),
                "domain_dir",   "%s", yuneta_domain_dir(),
                "subdir",       "%s", "audit",
                "filename",     "%s", "ZZZ-DD_MM_CCYY.log",
                NULL
            );
            print_error(
                PEF_EXIT,
                "Command audit: the path of the audit file cannot be built (root_dir '%s', "
                "domain_dir '%s', subdir 'audit'), the agent does not run",
                yuneta_root_dir(),
                yuneta_domain_dir()
            );
        }
        if(priv->audit_file) {
            priv->audit_tty_bursts = json_object();
            rotatory_keep_all_old_files(priv->audit_file, TRUE);   // a size rotation deletes no audit
            rotatory_subscribe2newfile(priv->audit_file, audit_newfile_cb, gobj);
            remove_old_audit_files(gobj);
            gobj_audit_commands(audit_command_cb, gobj);
        }
    }

    /*-----------------------------*
     *      Consoles
     *-----------------------------*/
    priv->list_consoles = json_object();

    /*-----------------------------*
     *      run-yuno play=0
     *-----------------------------*/
    priv->no_play_launches = json_object();

    /*-----------------------------*
     *      Launched, not open yet
     *-----------------------------*/
    priv->launching_yunos = json_object();

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
    SET_PRIV(timerStBoot,             gobj_read_integer_attr)
    SET_PRIV(timeout_expiration,      gobj_read_integer_attr)
    SET_PRIV(cert_sync_enabled,       gobj_read_bool_attr)
    SET_PRIV(cert_sync_interval_sec,  (int32_t)gobj_read_integer_attr)
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
    JSON_DECREF(priv->watches);
    JSON_DECREF(priv->publishing);
    JSON_DECREF(priv->restart_wait);
    JSON_DECREF(priv->restart_held);
    JSON_DECREF(priv->no_play_launches);
    JSON_DECREF(priv->launching_yunos);
    JSON_DECREF(priv->cert_sync_state);

    if(priv->audit_file) {
        /*
         *  The console writes still being counted are written before the end
         */
        json_t *jn_records = json_array();
        audit_tty_close_bursts(priv->audit_tty_bursts, jn_records);
        write_audit_records(gobj, jn_records);
        JSON_DECREF(jn_records)
        rotatory_close(priv->audit_file);
        priv->audit_file = 0;
    }
    JSON_DECREF(priv->audit_tty_bursts);

    remove_pid_file();
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

    /*----------------------------------------*
     *  Start controlcenter if it has owner
     *----------------------------------------*/
    const char *node_owner = gobj_yuno_node_owner();
    if(strcasecmp(node_owner, "none")!=0) {
        hgobj gobj_controlcenter = gobj_find_service("controlcenter", TRUE);
        gobj_start_tree(gobj_controlcenter);
    }

    /*----------------------------------------*
     *  Cert auto-sync timer (self-healing for
     *  the Let's Encrypt deploy-hook path)
     *----------------------------------------*/
    cert_sync_arm_timer(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Close the live consoles (volatil C_PTY services): left running
     *  they reach gobj_end still started ("Destroying a RUNNING gobj").
     *  HACK copy the key: delete_console KW_EXTRACTs it from
     *  list_consoles and the original string dies mid-call.
     */
    const char *name; json_t *jn_console; void *n;
    json_object_foreach_safe(priv->list_consoles, n, name, jn_console) {
        char *name_ = gbmem_strdup(name);   // any length: a gobj name can exceed NAME_MAX - 1
        delete_console(gobj, name_);
        GBMEM_FREE(name_);
    }

    /*
     *  And the C_COUNTERs of run/kill/play/pause-yuno still waiting: the
     *  yuno stops only its direct children, so they reach gobj_end still
     *  running. Not answered: their channels are closing.
     */
    json_t *dl_counters = gobj_match_children(gobj, json_pack("{s:s}",
        "__gclass_name__", C_COUNTER
    ));
    /*
     *  The iter holds a reference of each gobj: it is freed BEFORE they are
     *  destroyed. Up to 7.28.2 they were destroyed under it ("gobj
     *  DESTROYING with references") and its free read the gobjs already
     *  gone ("iter gobj with refs <= 0"), at a stop that came while a
     *  run/play-yuno still waited, as a restart right after a deploy.
     */
    size_t n_counters = json_array_size(dl_counters);
    hgobj *counters = NULL;
    if(n_counters > 0) {
        counters = gbmem_malloc(n_counters * sizeof(hgobj));
        if(!counters) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "no memory for the C_COUNTERs to stop: left running",
                "counters",     "%d", (int)n_counters,
                NULL
            );
            n_counters = 0;
        }
    }
    for(size_t i = 0; i < n_counters; i++) {
        counters[i] = (hgobj)(size_t)json_integer_value(json_array_get(dl_counters, i));
    }
    gobj_free_iter(dl_counters);
    for(size_t i = 0; i < n_counters; i++) {
        if(gobj_is_running(counters[i])) {
            gobj_stop(counters[i]);
        }
        gobj_destroy(counters[i]);
    }
    GBMEM_FREE(counters);

    clear_timeout(priv->timer);
    if(priv->cert_sync_timer) {
        clear_timeout(priv->cert_sync_timer);
    }
    clear_timeout(priv->restart_wait_timer);
    json_object_clear(priv->watches);
    watch_rearm_timer(gobj);
    gobj_unsubscribe_event(priv->gobj_authz, 0, 0, gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 *  Yuneta rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-------------------------------------------*
     *          Create Treedb System
     *-------------------------------------------*/
    char path[PATH_MAX];
    if(!yuneta_realm_store_dir(
        path,
        sizeof(path),
        "agent",
        gobj_yuno_realm_owner(),
        gobj_yuno_realm_id(),
        "",  // tenant
        "",  // gclass-treedb controls the directories
        TRUE
    )) {
        gobj_log_critical(gobj, LOG_OPT_EXIT_ZERO,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot build or create the store directory",
            NULL
        );
        return -1;
    }
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
    gobj_start_tree(priv->gobj_treedbs);

    /*-------------------------------------------*
     *      Load schema
     *      Open treedb agentdb service
     *      (does not return if it does not open)
     *-------------------------------------------*/
    open_agent_treedb(
        gobj,
        priv->gobj_treedbs,
        priv->treedb_agentdb_name,
        sizeof(priv->treedb_agentdb_name)
    );

    /*-------------------------*
     *      Get services
     *-------------------------*/
    priv->resource = gobj_find_service(priv->treedb_agentdb_name, TRUE);

    remove_temp_config_files_of_yunos(gobj);

    // Get timeranger of treedb_agentdb, will be used for alarms too
    // priv->tranger_treedb_agentdb = gobj_read_pointer_attr(priv->gobj_treedb_agentdb, "tranger");

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);

    /*--------------------------------*
     *      Start
     *--------------------------------*/
    get_last_public_port(gobj);
    gobj_start_tree(priv->gobj_input_side);

    set_timeout(priv->timer, 3000); // for util_yunos_running

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------*
     *      Stop services
     *-----------------------------*/
    gobj_stop_tree(priv->gobj_input_side);

    /*---------------------------------------*
     *      Close treedb airedb
     *---------------------------------------*/
    json_decref(gobj_command(priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s}",
            "treedb_name", priv->treedb_agentdb_name
        ),
        gobj
    ));

    /*-------------------------*
     *      Stop treedbs
     *-------------------------*/
    if(priv->gobj_treedbs) {
        gobj_stop_tree(priv->gobj_treedbs);
        EXEC_AND_RESET(gobj_destroy, priv->gobj_treedbs)
    }

    clear_timeout(priv->timer);

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
        json_sprintf("%s: pong", gobj_yuno_role_plus_name()),
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
    /*
     *  The uuid travels in `comment` and in `data` as an object. It used to
     *  be a bare string in `data`: legal, but then every client has to
     *  special-case a scalar, and the ones that did not simply dropped it.
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), node_uuid()),
        0,
        json_pack("{s:s}", "uuid", node_uuid()),
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
        0,
        0,
        jn_resp,
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
    if(!build_path(directory, sizeof(directory), "/yuneta", subdirectory, NULL)) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("%s: subdirectory too long", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    return build_dir_listing_response(gobj, directory, match, kw);
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
    if(!build_path(directory, sizeof(directory), "/yuneta/realms", subdirectory, NULL)) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("%s: subdirectory too long", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    return build_dir_listing_response(gobj, directory, match, kw);
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
    if(!build_path(directory, sizeof(directory), "/yuneta/repos", subdirectory, NULL)) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("%s: subdirectory too long", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    return build_dir_listing_response(gobj, directory, match, kw);
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
    if(!build_path(directory, sizeof(directory), "/yuneta/store", subdirectory, NULL)) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("%s: subdirectory too long", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    return build_dir_listing_response(gobj, directory, match, kw);
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
            json_sprintf("%s: 'id' required", gobj_yuno_role_plus_name()),
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
            json_sprintf("%s: Yuno not found: %s", gobj_yuno_role_plus_name(), id),
            0,
            0,
            kw  // owned
        );
    }

    char yuno_log_path[PATH_MAX];
    if(!build_yuno_log_path(gobj, node, yuno_log_path, sizeof(yuno_log_path), FALSE)) {
        json_decref(node);
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("%s: cannot build the logs path of yuno %s",
                gobj_yuno_role_plus_name(), id),
            0,
            0,
            kw  // owned
        );
    }

    json_decref(node);

    return build_dir_listing_response(gobj, yuno_log_path, ".*", kw);
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
            json_sprintf("%s: 'id' required", gobj_yuno_role_plus_name()),
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
            json_sprintf("%s: Yuno not found: %s", gobj_yuno_role_plus_name(), id),
            0,
            0,
            kw  // owned
        );
    }

    char private_domain[PATH_MAX];
    char yuno_data_path[PATH_MAX];
    const char *work_dir = yuneta_root_dir();
    if(!build_yuno_private_domain(gobj, node, private_domain, sizeof(private_domain)) ||
            !build_path(yuno_data_path, sizeof(yuno_data_path), work_dir, private_domain, NULL)) {
        json_decref(node);
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("%s: cannot build the data path of yuno %s",
                gobj_yuno_role_plus_name(), id),
            0,
            0,
            kw  // owned
        );
    }

    json_decref(node);

    return build_dir_listing_response(gobj, yuno_data_path, ".*", kw);
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
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), cmd),
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
            json_sprintf("%s: Select some public service please",
                gobj_yuno_role_plus_name()),
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

    gobj_log_set_last_message("%s", "");    // the cause answered is of these updates
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
            json_sprintf("%s: %s", gobj_yuno_role_plus_name(), last_failure_cause()):
            json_sprintf("%s: Updated", gobj_yuno_role_plus_name()),
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
     *  The id is required. Without it the filter is empty and selects
     *  every public service: up to 7.29.3 `delete-public-service` with no
     *  id deleted them all. Checked here, not with SDF_REQUIRED: the
     *  parser takes a required parameter by position, and refuses the
     *  `id=X` every caller writes.
     */
    if(empty_string(kw_get_str(gobj, kw, "id", "", 0))) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: delete-public-service needs an id: without one it selects every public service",
                gobj_yuno_role_plus_name()),
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
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: Select some public service please",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Delete
     */
    int result = 0;
    json_t *jn_data = json_array();     // the ones deleted
    json_t *jn_comment = 0;

    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        const char *id = kw_get_str(gobj, node, "id", "", 0);
        gobj_log_set_last_message("%s", "");    // the cause answered is this delete's
        if(gobj_delete_node(
                priv->resource, resource, kw_incref(node), json_pack("{s:b, s:b}", "force", force, "ignore_snaps", force), src)<0) {
            /*
             *  Said with the ones deleted before it (in `data`): up to
             *  7.29.3 the answer was the cause alone, and `data` named the
             *  one that failed among them
             */
            char after[64] = "";
            int not_tried = (int)json_array_size(iter) - idx - 1;
            if(not_tried > 0) {
                snprintf(after, sizeof(after), "; %d not tried, after it", not_tried);
            }
            jn_comment = json_sprintf(
                "%s: public service '%s' NOT deleted: %s; before it, %d public services deleted%s",
                gobj_yuno_role_plus_name(), id, last_failure_cause(),
                (int)json_array_size(jn_data), after
            );
            result = -1;
            break;
        }
        json_array_append_new(jn_data, json_string(id));
    }

    JSON_DECREF(iter)

    if(!jn_comment) {
        jn_comment = json_sprintf("%s: %d public services deleted",
            gobj_yuno_role_plus_name(), (int)json_array_size(jn_data));
    }
    return msg_iev_build_response(
        gobj,
        result,
        jn_comment,
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
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), cmd),
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
            json_sprintf("%s: What realm id?", gobj_yuno_role_plus_name()),
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
        ret<0?
            json_sprintf("%s: Not found!", gobj_yuno_role_plus_name()):
            json_sprintf("%s: Exists!", gobj_yuno_role_plus_name()),
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
            json_sprintf("%s: What realm owner?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(realm_role)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: What realm role?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(realm_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: What realm name?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(realm_env)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: What realm environment?", gobj_yuno_role_plus_name()),
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
            json_sprintf("%s: Realm already exists", gobj_yuno_role_plus_name()),
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
    gobj_log_set_last_message("%s", "");    // the cause answered is this create's
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
            json_sprintf("%s: Cannot create realm: %s",
                gobj_yuno_role_plus_name(), last_failure_cause()),
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
            json_sprintf("%s: Select some realm please", gobj_yuno_role_plus_name()),
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

    gobj_log_set_last_message("%s", "");    // the cause answered is of these updates
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
            json_sprintf("%s: %s", gobj_yuno_role_plus_name(), last_failure_cause()):
            json_sprintf("%s: Updated", gobj_yuno_role_plus_name()),
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
     *  The id is required. Without it the filter is empty and selects
     *  every realm: up to 7.29.3 `delete-realm` with no id deleted every
     *  realm with no yunos. Checked here, not with SDF_REQUIRED: the
     *  parser takes a required parameter by position, and refuses the
     *  `id=X` every caller writes.
     */
    if(empty_string(kw_get_str(gobj, kw, "id", "", 0))) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: delete-realm needs an id: without one it selects every realm",
                gobj_yuno_role_plus_name()),
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
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: Select some realm please", gobj_yuno_role_plus_name()),
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
                "%s: Cannot delete realm '%s'. Using in %d yunos",
                gobj_yuno_role_plus_name(),
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
    json_t *jn_data = json_array();     // the ones deleted
    json_t *jn_comment = 0;
    json_array_foreach(iter, idx, node) {
        const char *id = kw_get_str(gobj, node, "id", "", 0);
        gobj_log_set_last_message("%s", "");    // the cause answered is this delete's
        if(gobj_delete_node(
                priv->resource, resource, kw_incref(node), json_pack("{s:b, s:b}", "force", force, "ignore_snaps", force), src)<0) {
            /*
             *  Said with the ones deleted before it (in `data`): up to
             *  7.29.3 the answer was the cause alone, and `data` named the
             *  one that failed among them
             */
            char after[64] = "";
            int not_tried = (int)json_array_size(iter) - idx - 1;
            if(not_tried > 0) {
                snprintf(after, sizeof(after), "; %d not tried, after it", not_tried);
            }
            jn_comment = json_sprintf(
                "%s: realm '%s' NOT deleted: %s; before it, %d realms deleted%s",
                gobj_yuno_role_plus_name(), id, last_failure_cause(),
                (int)json_array_size(jn_data), after
            );
            result = -1;
            break;
        }
        json_array_append_new(jn_data, json_string(id));
    }

    JSON_DECREF(iter)

    if(!jn_comment) {
        jn_comment = json_sprintf("%s: %d realms deleted",
            gobj_yuno_role_plus_name(), (int)json_array_size(jn_data));
    }
    return msg_iev_build_response(
        gobj,
        result,
        jn_comment,
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *  Add the on-disk file time of each record's `binary` as the sibling of
 *  `size`: `time` (epoch seconds) and `time_str` (local timestamp). This lets
 *  a consumer (sync_binaries) detect a rebuild that kept the byte count
 *  identical — a relink against a changed static lib, or a one-char edit — by
 *  comparing the local file's mtime against the installed slot's, since `size`
 *  alone would call it up-to-date.
 *
 *  Computed live by stat()'ing the stored path: it covers records installed
 *  before this field existed and never mutates the in-memory treedb node (the
 *  array elements are fresh node_collapsed_view dicts, not store references).
 ***************************************************************************/
PRIVATE void add_binary_file_time(json_t *jn_data)
{
    if(!json_is_array(jn_data)) {
        return;
    }
    size_t idx; json_t *node;
    json_array_foreach(jn_data, idx, node) {
        const char *binary = json_string_value(json_object_get(node, "binary"));
        if(empty_string(binary)) {
            continue;
        }
        struct stat st;
        if(stat(binary, &st) != 0) {
            // No log: a slot whose file was pruned is not an error here.
            continue;
        }
        char timestamp[80];
        t2timestamp(timestamp, sizeof(timestamp), st.st_mtime, TRUE);
        json_object_set_new(node, "time", json_integer((json_int_t)st.st_mtime));
        json_object_set_new(node, "time_str", json_string(timestamp));
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_binaries(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "binaries";

    /*
     *  list-binaries shows the binaries IN USE: ONE node per id (role), the
     *  in-memory primary. Every installed (role, version) is surfaced by the
     *  sibling `list-binaries-instances` (gobj_list_instances) instead. The
     *  old "primary is stale after install-binary until deactivate-snap"
     *  reason for walking instances here was the pkey2 staleness bug, fixed
     *  in tr_treedb's runtime save_node pkey2 refresh.
     */
    json_t *jn_data = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    add_binary_file_time(jn_data);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), cmd),
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
    /*
     *  run_command() goes through sh -c: the path goes in single quotes,
     *  each ' of it as '\'' -- the shell reads it as one word, whatever it
     *  holds. Up to 7.29.3 it went in bare.
     */
    char command[2*PATH_MAX + 32];
    size_t n = 0;
    BOOL fits = TRUE;
    command[n++] = '\'';
    for(const char *c = cmd; *c && fits; c++) {
        if(*c == '\'') {
            if(n + 4 >= sizeof(command)) {
                fits = FALSE;
                break;
            }
            memcpy(command + n, "'\\''", 4);
            n += 4;
        } else {
            if(n + 1 >= sizeof(command)) {
                fits = FALSE;
                break;
            }
            command[n++] = *c;
        }
    }
    const char *tail = "' --print-role";
    if(!fits || n + strlen(tail) >= sizeof(command)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Path of the binary too long",
            "path",         "%s", cmd,
            NULL
        );
        return NULL;
    }
    memcpy(command + n, tail, strlen(tail) + 1);
    gbuffer_t *gbuf = run_command(command);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Incorrect binary",
            "command",      "%s", command,
            NULL
        );
        return NULL;
    }
    char *cmd_output = gbuffer_cur_rd_pointer(gbuf);

    if(empty_string(cmd_output)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Incorrect binary",
            "command",      "%s", command,
            NULL
        );
        gbuffer_decref(gbuf);
        return NULL;
    }

    /*
     *  Parse output
     */
    char *p = strchr(cmd_output, '{');
    if(!p) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Dict not found",
            "command",      "%s", command,
            "output",       "%s", cmd_output,
            NULL
        );
        gbuffer_decref(gbuf);
        return NULL;
    }
    json_t *jn_basic_info = legalstring2json(p, TRUE);
    gbuffer_decref(gbuf);
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

    /*
     *  `id` becomes a filename under the realm "temp" dir, run by
     *  yuno_basic_information() (which also quotes it for the shell).
     *  build_path() does not strip shell metacharacters.
     */
    if(!is_safe_binary_name(gobj, "id", id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: invalid 'id': only [A-Za-z0-9._-] allowed, "
                "not starting with '.', no '..'",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    if(empty_string(content64)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: No data in content64", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    gbuffer_t *gbuf_content = gbuffer_base64_to_binary(content64, strlen(content64));
    if(!gbuf_content) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: Bad data in content64", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    char path[PATH_MAX];
    if(!yuneta_realm_file(path, sizeof(path), "temp", id, TRUE)) {
        GBUFFER_DECREF(gbuf_content);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot build the temporary path of '%s'",
                gobj_yuno_role_plus_name(), id),
            0,
            0,
            kw  // owned
        );
    }
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
            json_sprintf("%s: It seems a wrong yuno binary", gobj_yuno_role_plus_name()),
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

    /*
     *  They name /yuneta/repos/<tags>/<role>/<version>: an empty version
     *  would make the role's own directory the one of this version.
     */
    if(!is_safe_binary_name(gobj, "role", binary_role) ||
            !is_safe_binary_name(gobj, "version", binary_version)) {
        json_t *comment = json_sprintf(
            "%s: the binary says role '%s' version '%s': only [A-Za-z0-9._-] allowed, "
            "not starting with '.', no '..'",
            gobj_yuno_role_plus_name(), binary_role? binary_role: "", binary_version
        );
        JSON_DECREF(jn_basic_info);
        return msg_iev_build_response(
            gobj,
            -1,
            comment,
            0,
            0,
            kw  // owned
        );
    }

    if(!yuno_version_has_a_comparable_shape(binary_version)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Binary version neither x.y.z nor x.y.z-a.b.c "
                                  "(nor the old x.y.z.0): its releases may not compare in order",
            "role",         "%s", binary_role? binary_role : "",
            "version",      "%s", binary_version,
            NULL
        );
    }

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
            "%s: Binary already exists: role %s, version %s",
            gobj_yuno_role_plus_name(),
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
    char destination[PATH_MAX];
    if(!yuneta_repos_yuno_dir(
            destination,
            sizeof(destination),
            jn_tags,
            binary_role,
            binary_version,
            TRUE
        ) || access(destination,0)!=0 ||
        !yuneta_repos_yuno_file(
            destination,
            sizeof(destination),
            jn_tags,
            binary_role,
            binary_version,
            binary_role,
            TRUE
        )) {
        JSON_DECREF(jn_basic_info);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot build or create the repos directory of '%s' '%s'",
                gobj_yuno_role_plus_name(), binary_role, binary_version
            ),
            0,
            0,
            kw  // owned
        );
    }
    /*
     *  Overwrite, the overwrite filter was above.
     */
    if(copyfile(path, destination, yuneta_xpermission(), TRUE)<0) {
         gobj_log_error(gobj, 0,
             "function",     "%s", __FUNCTION__,
             "msgset",       "%s", MSGSET_SYSTEM,
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
                 "%s: Cannot copy '%s' to '%s'",
                 gobj_yuno_role_plus_name(),
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
    gobj_log_set_last_message("%s", "");    // the cause answered is this create's
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
            json_sprintf("%s: Cannot create binary: %s",
                gobj_yuno_role_plus_name(), last_failure_cause()),
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
        json_sprintf("%s: version: %s",
            gobj_yuno_role_plus_name(), kw_get_str(gobj, node, "version", "", KW_REQUIRED)),
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

    const char *id = kw_get_str(gobj, kw, "id", "", 0);
    if(!id) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: 'id' required", gobj_yuno_role_plus_name()),
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
            json_sprintf("%s: Binary not found", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    const char *role = kw_get_str(gobj, node, "id", "", KW_REQUIRED);
    if(!is_safe_binary_name(gobj, "id", role)) {
        json_t *comment = json_sprintf(
            "%s: invalid binary id '%s': only [A-Za-z0-9._-] allowed, "
            "not starting with '.', no '..'",
            gobj_yuno_role_plus_name(), role
        );
        json_decref(node);  // after the comment: `role` is in it
        return msg_iev_build_response(
            gobj,
            -1,
            comment,
            0,
            0,
            kw  // owned
        );
    }

    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    if(empty_string(content64)) {
        json_decref(node);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: No data in content64", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    gbuffer_t *gbuf_content = gbuffer_base64_to_binary(content64, strlen(content64));
    if(!gbuf_content) {
        json_decref(node);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: Bad data in content64", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    char path[PATH_MAX];
    if(!yuneta_realm_file(path, sizeof(path), "temp", role, TRUE)) {
        GBUFFER_DECREF(gbuf_content);
        json_decref(node);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot build the temporary path of '%s'",
                gobj_yuno_role_plus_name(), role),
            0,
            0,
            kw  // owned
        );
    }
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
                "%s: It seems a wrong yuno binary: %s",
                gobj_yuno_role_plus_name(), path
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Binary received.
     *------------------------------------------------*/
    const char *binary_role = kw_get_str(gobj, jn_basic_info, "role", "", 0);
    const char *binary_version = kw_get_str(gobj, jn_basic_info, "version", "", 0);
    json_t *jn_tags = kw_get_dict_value(gobj, jn_basic_info, "tags", 0, 0);

    /*
     *  The row of THAT version, not the primary one: the primary is the
     *  version installed last, and up to 7.28.9 a rebuild of an older
     *  version was merged into it. A version that is not installed, or a
     *  binary of another role, is refused: install-binary adds a version.
     */
    json_t *installed = NULL;
    if(strcmp(binary_role, role)==0 && is_safe_binary_name(gobj, "version", binary_version)) {
        installed = find_binary_version(gobj, priv->resource, role, binary_version);
    }
    if(!installed) {
        json_t *comment = json_sprintf(
            "%s: binary '%s' version '%s' is not installed here (it is a binary of '%s'): "
            "update-binary replaces an installed version, install-binary adds one",
            gobj_yuno_role_plus_name(),
            role,
            binary_version,
            binary_role
        );
        JSON_DECREF(jn_basic_info);
        json_decref(node);  // after the comment: `role` is in it
        return msg_iev_build_response(
            gobj,
            -1,
            comment,
            0,
            0,
            kw  // owned
        );
    }
    json_decref(node);
    node = installed;
    role = kw_get_str(gobj, node, "id", "", KW_REQUIRED);

    /*------------------------------------------------*
     *      Store in filesystem
     *------------------------------------------------*/
    /*
     *  Destination: inside of /yuneta/repos
     *      {{tags}}/{{role}/{{version}}/binary.exe
     */
    char destination[PATH_MAX];
    if(!yuneta_repos_yuno_dir(
            destination,
            sizeof(destination),
            jn_tags,
            binary_role,
            binary_version,
            TRUE
        ) || access(destination,0)!=0 ||
        !yuneta_repos_yuno_file(
            destination,
            sizeof(destination),
            jn_tags,
            binary_role,
            binary_version,
            binary_role,
            TRUE
        )) {
        JSON_DECREF(jn_basic_info);
        json_decref(node);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot build or create the repos directory of '%s' '%s'",
                gobj_yuno_role_plus_name(), binary_role, binary_version
            ),
            0,
            0,
            kw  // owned
        );
    }
    /*
     *  Overwrite, the overwrite filter was above.
     */
    if(copyfile(path, destination, yuneta_xpermission(), TRUE)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
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
                "%s: Cannot copy '%s' to '%s'",
                gobj_yuno_role_plus_name(),
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

    gobj_log_set_last_message("%s", "");    // the cause answered is this update's
    node = gobj_update_node(
        priv->resource,
        resource,
        node,
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    if(!node) {
        /*
         *  The file is in place, the record is not: sync-binaries read the
         *  old result 0 as "installed" (up to 7.24.1).
         */
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: binary copied but its record was NOT updated: %s",
                gobj_yuno_role_plus_name(), last_failure_cause()),
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
    json_t *webix = msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: version: %s",
            gobj_yuno_role_plus_name(), kw_get_str(gobj, node, "version", "", KW_REQUIRED)),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );

    return webix;
}

/***************************************************************************
 *  Select the node(s) a delete-* command will act on.
 *
 *  When the caller supplies the topic's pkey2 (yuno_release / version) the
 *  target is that exact instance, listed with gobj_list_instances; otherwise
 *  fall back to the in-memory primary node(s) with gobj_list_nodes — the
 *  historic behaviour. This lets an operator prune a single non-primary
 *  release/version durably (treedb_delete_instance tombstones its md2 rows).
 ***************************************************************************/
PRIVATE json_t *list_delete_targets(
    hgobj gobj,
    const char *topic_name,
    const char *pkey2_field,    // "yuno_release" or "version"
    json_t *kw,                 // NOT owned, used as filter
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *pkey2_value = kw_get_str(gobj, kw, pkey2_field, "", 0);
    if(!empty_string(pkey2_value)) {
        return gobj_list_instances(
            priv->resource,
            topic_name,
            pkey2_field,
            kw_incref(kw),  // filter
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
    }

    return gobj_list_nodes(
        priv->resource,
        topic_name,
        kw_incref(kw),  // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
}

/***************************************************************************
 *  Count yunos that actually USE the config/binary instances a delete
 *  takes, robustly.
 *
 *  Each instance (version) has its OWN `yunos` list (an fkey column): a
 *  yuno is linked to the version it was created on (create-yuno
 *  role_version=/name_version=). The lists are not disjoint: a version
 *  installed later starts with a copy of the primary's list (the treedb's
 *  new instance inherits the fkeys of the primary), so it can name yunos
 *  that use another version. So a delete that takes several versions reads
 *  the list of every one of them, a yuno that two lists name counts once,
 *  and the delete of one version counts only the yunos on THAT version
 *  (target_version below). Up to 7.29.3 only the list of the node given
 *  was read: a yuno pinned to another version of a key the delete took
 *  whole was not looked at.
 *
 *  A list can carry STALE refs (a yuno whose record was deleted but whose
 *  ref lingers in memory until the next reload — the list is not always
 *  cleaned on a multi-version key). So validate every listed
 *  yuno id with gobj_get_node (a deleted yuno returns NULL → skip). When
 *  target_version is non-empty, count only yunos pinned to THAT version (a
 *  yuno uses config version = its name_version, binary version = its
 *  role_version), so an unused/superseded version can be pruned while
 *  another is in use.
 ***************************************************************************/
PRIVATE int count_yunos_using(
    hgobj gobj,
    json_t *instances,          // NOT owned, config/binary nodes with their "yunos" list
    const char *version_field,  // "name_version" (config) or "role_version" (binary)
    const char *target_version, // node's version; "" => count any version
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int use = 0;
    json_t *seen = json_object();
    int idx; json_t *instance;
    json_array_foreach(instances, idx, instance) {
        json_t *yunos = kw_get_list(gobj, instance, "yunos", 0, 0);
        int i; json_t *jn_yuno_id;
        json_array_foreach(yunos, i, jn_yuno_id) {
            const char *yuno_id = json_string_value(jn_yuno_id);
            if(empty_string(yuno_id)) {
                continue;
            }
            if(json_object_get(seen, yuno_id)) {
                continue;   // named by the list of another version too
            }
            json_object_set_new(seen, yuno_id, json_true());
            json_t *y = gobj_get_node(
                priv->resource,
                "yunos",
                json_pack("{s:s}", "id", yuno_id),
                json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                src
            );
            if(!y) {
                continue;   // stale hook ref: the yuno is gone, not a real user
            }
            if(empty_string(target_version) ||
                    strcmp(kw_get_str(gobj, y, version_field, "", 0), target_version)==0) {
                use++;
            }
            JSON_DECREF(y);
        }
    }
    JSON_DECREF(seen)
    return use;
}

/***************************************************************************
 *  The instances a delete of `node` takes, and whether it takes the whole
 *  key. A delete of the PRIMARY takes every version of the id: c_node's
 *  delete of the primary's own version reaches treedb_delete_node(), which
 *  deletes the whole key. So it is the whole key with no pkey2 given, and
 *  with the pkey2 of the primary too. Otherwise only `node`. Return YOURS.
 ***************************************************************************/
PRIVATE json_t *instances_taken_by_delete(
    hgobj gobj,
    const char *topic_name,
    const char *pkey2_field,    // "version"
    json_t *node,               // NOT owned, a node of list_delete_targets()
    BOOL by_instance,           // a pkey2 was given
    BOOL *whole_key,            // out
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *id = kw_get_str(gobj, node, "id", "", KW_REQUIRED);
    const char *pkey2 = kw_get_str(gobj, node, pkey2_field, "", KW_REQUIRED);

    *whole_key = !by_instance;
    if(by_instance) {
        json_t *primary = gobj_get_node(
            priv->resource,
            topic_name,
            json_pack("{s:s}", "id", id),
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
        if(primary && strcmp(kw_get_str(gobj, primary, pkey2_field, "", 0), pkey2)==0) {
            *whole_key = TRUE;
        }
        JSON_DECREF(primary)
    }

    if(*whole_key) {
        json_t *instances = gobj_list_instances(
            priv->resource,
            topic_name,
            pkey2_field,
            json_pack("{s:s}", "id", id),
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
        if(!instances) {
            instances = json_array();   // Error already logged
        }
        /*
         *  A record with an empty pkey2 (installed by 7.29.3 or older) has
         *  no row in the pkey2 index, so it is not listed: when it is the
         *  node itself, it is taken all the same.
         */
        BOOL listed = FALSE;
        int idx; json_t *instance;
        json_array_foreach(instances, idx, instance) {
            if(strcmp(kw_get_str(gobj, instance, pkey2_field, "", 0), pkey2)==0) {
                listed = TRUE;
                break;
            }
        }
        if(!listed) {
            json_array_append(instances, node);
        }
        return instances;
    }
    return json_pack("[O]", node);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_binary(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "binaries";

    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);
    BOOL record_only = kw_get_bool(gobj, kw, "record_only", 0, KW_WILD_NUMBER);

    /*
     *  The id is required. Without it the filter is empty and selects
     *  every binary: up to 7.29.3 `delete-binary force=1` with no id
     *  deleted them all, those in use too. Checked here, not with
     *  SDF_REQUIRED: the parser takes a required parameter by position,
     *  and refuses the `id=X` every caller writes.
     */
    if(empty_string(kw_get_str(gobj, kw, "id", "", 0))) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: delete-binary needs an id: without one it selects every binary",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = list_delete_targets(gobj, resource, "version", kw, src);

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: binary not found, nothing deleted",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  What each delete takes: a delete of the primary (no version=, or
     *  the primary's own version) takes EVERY version of the id, so its
     *  guards, its directories and its count are those of every version.
     *  Up to 7.29.3 they were the primary's alone: the record of every
     *  version went, the directory of the primary only (the others were
     *  left in /yuneta/repos, named by no record), the answer said "1
     *  binaries deleted", and a version used by a yuno or held by a snap
     *  was not looked at when the primary's was given.
     */
    BOOL by_instance = !empty_string(kw_get_str(gobj, kw, "version", "", 0));
    json_t *jn_units = json_array();    // [{"node": what is deleted, "whole": BOOL, "taken": [instances]}]
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        BOOL whole = FALSE;
        json_t *taken = instances_taken_by_delete(
            gobj, resource, "version", node, by_instance, &whole, src
        );
        json_array_append_new(jn_units,
            json_pack("{s:O, s:b, s:o}", "node", node, "whole", whole, "taken", taken)
        );
    }
    JSON_DECREF(iter)

    /*
     *  Check conditions to delete, of every version taken
     */
    json_t *unit;
    json_t *jn_comment = 0;
    json_array_foreach(jn_units, idx, unit) {
        node = kw_get_dict(gobj, unit, "node", 0, KW_REQUIRED);
        BOOL whole = kw_get_bool(gobj, unit, "whole", 0, KW_REQUIRED);
        json_t *taken = kw_get_list(gobj, unit, "taken", 0, KW_REQUIRED);
        const char *role = kw_get_str(gobj, node, "id", "", KW_REQUIRED);
        const char *node_version = kw_get_str(gobj, node, "version", "", KW_REQUIRED);

        int use = count_yunos_using(gobj, taken, "role_version",
            whole? "": node_version, src);
        if(use > 0 && !force) {
            if(whole) {
                jn_comment = json_sprintf(
                    "%s: cannot delete binary '%s': the delete of its primary takes its %d versions, "
                    "and %d yunos use it (use force=1 to override)",
                    gobj_yuno_role_plus_name(),
                    role,
                    (int)json_array_size(taken),
                    use
                );
            } else {
                jn_comment = json_sprintf(
                    "%s: cannot delete binary '%s' version '%s'. Using in %d yunos "
                    "(use force=1 to override)",
                    gobj_yuno_role_plus_name(),
                    role,
                    node_version,
                    use
                );
            }
            break;
        }

        /*
         *  A snap shot while a version was the in-use one stamps its id on
         *  the record (surfaced as __md_treedb__.tag). Deleting it would
         *  break that snap's rollback ("primary binary not found" on
         *  activate-snap + run-yuno). The kernel (treedb_delete_node)
         *  already refuses a tagged node, but check here too for a clear
         *  reason and so no binary FILE is ever rmrdir'd. force=1 overrides
         *  (operator accepts breaking the snap), matching the kernel's
         *  force semantics.
         */
        int i; json_t *instance;
        json_array_foreach(taken, i, instance) {
            json_int_t snap_tag = kw_get_int(gobj, instance, "__md_treedb__`tag", 0, 0);
            if(snap_tag != 0 && !force) {
                jn_comment = json_sprintf(
                    "%s: cannot delete binary '%s' %s: referenced by snap %d (rollback)%s. "
                    "Use force=1 to delete anyway (breaks that snap's rollback)",
                    gobj_yuno_role_plus_name(),
                    role,
                    kw_get_str(gobj, instance, "version", "", 0),
                    (int)snap_tag,
                    whole? ", and the delete of the primary takes every version": ""
                );
                break;
            }

            /*
             *  Neither the record nor the files: an rmrdir of what a cut
             *  path (or an empty or "." version) names removes an ancestor,
             *  every version of the role. Not force=1: force passes the
             *  guards above (in use, a snap), and up to 7.29.3 cleaning
             *  such a record with it deleted it while a yuno used it.
             */
            const char *version = kw_get_str(gobj, instance, "version", "", 0);
            char destination[PATH_MAX];
            if(!record_only && !binary_version_dir(
                    gobj, destination, sizeof(destination),
                    kw_get_dict_value(gobj, instance, "tags", 0, 0), role, version)) {
                jn_comment = json_sprintf(
                    "%s: binary '%s' '%s': name refused or repos path too long, not deleted%s "
                    "(record_only=1 deletes the record only, its files stay)",
                    gobj_yuno_role_plus_name(), role, version,
                    whole? ", nor any version of it (the delete of the primary takes them all)": ""
                );
                break;
            }
        }
        if(jn_comment) {
            break;
        }
    }
    if(jn_comment) {
        JSON_DECREF(jn_units)
        return msg_iev_build_response(
            gobj,
            -1,
            jn_comment,
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Delete. The data says each version taken: its id, its version, its
     *  files (its version's directory, or for a name refused the binary
     *  the record says it installed, "" when it names none), and whether
     *  the agent removed them.
     */
    int result = 0;
    int deleted = 0;
    int records_only = 0;
    json_t *jn_data = json_array();
    json_t *jn_kept = json_array();   // the files of the records deleted alone
    json_t *jn_left = json_array();   // the directories rmrdir() could not remove
    json_t *failed_node = 0;          // the delete that failed: the ones after it are not tried
    int not_tried = 0;
    char failed_cause[256] = "";
    json_array_foreach(jn_units, idx, unit) {
        node = kw_get_dict(gobj, unit, "node", 0, KW_REQUIRED);
        json_t *taken = kw_get_list(gobj, unit, "taken", 0, KW_REQUIRED);

        gobj_log_set_last_message("%s", "");    // the cause answered is this delete's
        if(gobj_delete_node(
                priv->resource, resource, kw_incref(node), json_pack("{s:b, s:b}", "force", force, "ignore_snaps", force), src)<0) {
            snprintf(failed_cause, sizeof(failed_cause), "%s", last_failure_cause());
            failed_node = node;
            not_tried = (int)json_array_size(jn_units) - idx - 1;
            result = -1;
            break;
        }

        int i; json_t *instance;
        json_array_foreach(taken, i, instance) {
            const char *role = kw_get_str(gobj, instance, "id", "", KW_REQUIRED);
            const char *version = kw_get_str(gobj, instance, "version", "", KW_REQUIRED);
            char destination[PATH_MAX];
            BOOL with_files = binary_version_dir(
                gobj, destination, sizeof(destination),
                kw_get_dict_value(gobj, instance, "tags", 0, KW_REQUIRED), role, version
            )? TRUE: FALSE;
            deleted++;

            if(record_only || !with_files) {
                /*
                 *  Where the files of a record deleted alone are: its
                 *  version's directory, or, for a name refused (a record
                 *  installed by 7.29.3 or older can hold a ^, @, a space,
                 *  an empty version; no directory is exactly its
                 *  version's), the binary the record says it installed.
                 *  No directory is removed.
                 */
                const char *kept_files = with_files?
                    destination: kw_get_str(gobj, instance, "binary", "", 0);
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "Binary record deleted with record_only=1, its files stay",
                    "id",           "%s", role,
                    "version",      "%s", version,
                    "files",        "%s", kept_files,
                    NULL
                );
                if(!empty_string(kept_files)) {
                    json_array_append_new(jn_kept, json_string(kept_files));
                }
                json_array_append_new(jn_data, json_pack("{s:s, s:s, s:s, s:b}",
                    "id", role,
                    "version", version,
                    "files", kept_files,
                    "removed", 0
                ));
                records_only++;
                continue;
            }

            /*
             *  Remove from store in filesystem. A directory that is not
             *  there is nothing left behind: ENOENT, or ENOTDIR (a part of
             *  its path is not a directory, so no directory can be at that
             *  path). One rmrdir() could not remove (logged by it) is left,
             *  and so is one the agent cannot look at (access() fails
             *  otherwise: no permission on a parent, say): it may be there.
             *  The answer says so. Up to 7.29.3 the last was "removed".
             */
            BOOL removed = TRUE;
            if(access(destination, F_OK)==0) {
                if(rmrdir(destination)<0) {
                    removed = FALSE;    // Error already logged
                }
            } else if(errno != ENOENT && errno != ENOTDIR) {
                int err = errno;
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot tell whether the directory of a deleted binary exists: not removed",
                    "id",           "%s", role,
                    "version",      "%s", version,
                    "path",         "%s", destination,
                    "errno",        "%d", err,
                    "serrno",       "%s", strerror(err),
                    NULL
                );
                removed = FALSE;
            }
            if(!removed) {
                json_array_append_new(jn_left, json_string(destination));
            }
            json_array_append_new(jn_data, json_pack("{s:s, s:s, s:s, s:b}",
                "id", role,
                "version", version,
                "files", destination,
                "removed", removed
            ));
        }
    }

    /*
     *  What was done, said whole even when a later delete failed: the
     *  records of the units before it are gone, and so are their
     *  directories, or not (jn_left), or they stay (jn_kept). Up to 7.29.3
     *  a failure said only its own cause, and lost all of that.
     *
     *  Directories left and records deleted alone never meet in one call:
     *  record_only=1 removes no directory, and without it every version
     *  taken passed the name guard above, so each has its directory.
     */
    json_t *jn_done;
    if(json_array_size(jn_left) > 0) {
        /*
         *  The records are gone and these directories are not: named by no
         *  record, nothing removes them but a hand.
         */
        char *left = json2uglystr(jn_left);
        jn_done = json_sprintf(
            "%d binaries deleted, %d of them with files the agent could not remove "
            "(see the log): remove them by hand: %s",
            deleted, (int)json_array_size(jn_left),
            left? left: ""
        );
        GBMEM_FREE(left);
        result = -1;
    } else if(records_only > 0) {
        char *kept = json_array_size(jn_kept)? json2uglystr(jn_kept): 0;
        jn_done = json_sprintf(
            "%d binaries deleted, %d of them the record only (record_only=1): "
            "remove its files by hand%s%s",
            deleted, records_only,
            kept? ": ": " (the record names none)",
            kept? kept: ""
        );
        GBMEM_FREE(kept);
    } else {
        jn_done = json_sprintf("%d binaries deleted", deleted);
    }
    if(failed_node) {
        char after[64] = "";
        if(not_tried > 0) {
            snprintf(after, sizeof(after), "; %d not tried, after it", not_tried);
        }
        jn_comment = json_sprintf(
            "%s: binary '%s' version '%s' NOT deleted: %s; before it, %s%s",
            gobj_yuno_role_plus_name(),
            kw_get_str(gobj, failed_node, "id", "", 0),
            kw_get_str(gobj, failed_node, "version", "", 0),
            failed_cause,
            json_string_value(jn_done),
            after
        );
    } else {
        jn_comment = json_sprintf("%s: %s", gobj_yuno_role_plus_name(), json_string_value(jn_done));
    }
    JSON_DECREF(jn_done)
    JSON_DECREF(jn_units)
    JSON_DECREF(jn_kept)
    JSON_DECREF(jn_left)
    return msg_iev_build_response(
        gobj,
        result,
        jn_comment,
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
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), cmd),
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
            json_sprintf("%s: Configuration Id is required", gobj_yuno_role_plus_name()),
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
        gbuffer_t *gbuf_content = gbuffer_base64_to_binary(content64, strlen(content64));
        if(!gbuf_content) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: Invalid base64 in content64", gobj_yuno_role_plus_name()),
                0,
                0,
                kw  // owned
            );
        }
        jn_config = config_gbuffer2json(
            gbuf_content,  // owned
            2
        );
        if(!jn_config) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: Bad json in content64", gobj_yuno_role_plus_name()),
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
            json_sprintf(
                "%s: Configuration __version__ is required, as a string",
                gobj_yuno_role_plus_name()
            ),
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
            json_sprintf("%s: Configuration already exists", gobj_yuno_role_plus_name()),
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
    gobj_log_set_last_message("%s", "");    // the cause answered is this create's
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
            json_sprintf("%s: Cannot create config: %s",
                gobj_yuno_role_plus_name(), last_failure_cause()),
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
        json_sprintf("%s: version: %s",
            gobj_yuno_role_plus_name(), kw_get_str(gobj, node, "version", "", KW_REQUIRED)),
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
            json_sprintf("%s: 'id' required", gobj_yuno_role_plus_name()),
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
        gbuffer_t *gbuf_content = gbuffer_base64_to_binary(content64, strlen(content64));
        if(!gbuf_content) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: Invalid base64 in content64", gobj_yuno_role_plus_name()),
                0,
                0,
                kw  // owned
            );
        }
        jn_config = config_gbuffer2json(
            gbuf_content,  // owned
            2
        );
    }
    if(!jn_config) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: Bad json in content64", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Read both before the content is handed over below: the `zcontent`
     *  assignment takes ownership of jn_config, and reading through a pointer
     *  whose ownership has moved is a trap waiting for the next edit.
     */
    const char *version = kw_get_str(gobj, jn_config, "__version__", "", 0);
    const char *description = kw_get_str(gobj, jn_config, "__description__", "", 0);

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
            json_sprintf("%s: Configuration not found", gobj_yuno_role_plus_name()),
            tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
            iter,
            kw  // owned
        );
    }

    json_t *node = json_array_get(iter, 0);
    json_object_set_new(node, "zcontent", jn_config);

    /*
     *  The description travels with the content, so it has to be rewritten
     *  with it. Only create-config used to write this column, so a row kept
     *  the description of its FIRST version for the rest of its life while
     *  its content changed underneath -- and `list-configs` shows that column,
     *  which is where an operator reads what a config row is for. Found on
     *  a staging node, where the webstats row still announced "1: initial load" after
     *  its content had stopped reading the openresty tree.
     *
     *  Set unconditionally, exactly as create-config does: a content with no
     *  `__description__` leaves the column empty. Keeping the old text there
     *  would describe content that no longer exists, which is the defect.
     */
    json_object_set_new(node, "description", json_string(description));

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
    gobj_log_set_last_message("%s", "");    // the cause answered is this update's
    node = gobj_update_node(
        priv->resource,
        resource,
        json_incref(node),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );
    JSON_DECREF(iter)
    if(!node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: configuration NOT updated: %s",
                gobj_yuno_role_plus_name(), last_failure_cause()),
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

    json_t *webix = msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: version: %s",
            gobj_yuno_role_plus_name(), kw_get_str(gobj, node, "version", "", KW_REQUIRED)),
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
     *  The id is required. Without it the filter is empty and selects
     *  every configuration: up to 7.29.3 `delete-config force=1` with no
     *  id deleted them all, those in use too. Checked here, not with
     *  SDF_REQUIRED: the parser takes a required parameter by position,
     *  and refuses the `id=X` every caller writes.
     */
    if(empty_string(kw_get_str(gobj, kw, "id", "", 0))) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: delete-config needs an id: without one it selects every configuration",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = list_delete_targets(gobj, resource, "version", kw, src);

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: configuration not found, nothing deleted",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Check conditions to delete.
     *  In instance mode (version given) only a yuno pinned to THAT version
     *  blocks; otherwise any using yuno does. Stale hook refs are ignored
     *  (count_yunos_using validates each via gobj_get_node). force overrides.
     */
    BOOL by_instance = !empty_string(kw_get_str(gobj, kw, "version", "", 0));
    int deleted = 0;
    json_t *jn_taken_counts = json_array();    // the versions each delete takes
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        const char *node_version = kw_get_str(gobj, node, "version", "", KW_REQUIRED);
        /*
         *  A delete of the primary (no version=, or its own version) takes
         *  every version of the id: any yuno that uses one blocks it, and
         *  the answer counts them all. Up to 7.29.3 the primary's version
         *  given checked only the yunos of that version.
         */
        BOOL whole = FALSE;
        json_t *taken = instances_taken_by_delete(
            gobj, resource, "version", node, by_instance, &whole, src
        );
        deleted += (int)json_array_size(taken);
        json_array_append_new(jn_taken_counts, json_integer((json_int_t)json_array_size(taken)));
        int use = count_yunos_using(gobj, taken, "name_version",
            whole? "": node_version, src);
        JSON_DECREF(taken)
        if(use > 0 && !force) {
            json_t *comment = json_sprintf(
                "%s: cannot delete configuration '%s' version '%s'%s. Using in %d yunos "
                "(use force=1 to override)",
                gobj_yuno_role_plus_name(),
                kw_get_str(gobj, node, "id", "", KW_REQUIRED),
                node_version,
                whole? " (its primary: the delete takes every version)": "",
                use
            );
            JSON_DECREF(jn_taken_counts)
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
     *  Delete. A failure stops there, and the answer says it with what
     *  was deleted before it, which is in `data`. Up to 7.29.3 it said
     *  only the one that failed (and its data leaked).
     */
    json_t *jn_data = json_array();
    int done = 0;
    json_array_foreach(iter, idx, node) {
        const char *id = kw_get_str(gobj, node, "id", "", KW_REQUIRED);
        const char *version = kw_get_str(gobj, node, "version", "", KW_REQUIRED);

        gobj_log_set_last_message("%s", "");    // the cause answered is this delete's
        if(gobj_delete_node(
                priv->resource, resource, kw_incref(node), json_pack("{s:b, s:b}", "force", force, "ignore_snaps", force), src)<0) {
            char after[64] = "";
            int not_tried = (int)json_array_size(iter) - idx - 1;
            if(not_tried > 0) {
                snprintf(after, sizeof(after), "; %d not tried, after it", not_tried);
            }
            json_t *comment = json_sprintf(
                "%s: configuration '%s' version '%s' NOT deleted: %s; "
                "before it, %d configurations deleted%s",
                gobj_yuno_role_plus_name(), id, version, last_failure_cause(),
                done, after
            );
            JSON_DECREF(jn_taken_counts)
            JSON_DECREF(iter)
            return msg_iev_build_response(
                gobj,
                -1,
                comment,
                0,
                jn_data,
                kw  // owned
            );
        }
        done += (int)json_integer_value(json_array_get(jn_taken_counts, (size_t)idx));
        json_array_append_new(jn_data, json_string(id));
    }

    JSON_DECREF(jn_taken_counts)
    JSON_DECREF(iter)

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: %d configurations deleted",
            gobj_yuno_role_plus_name(), deleted),
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
            json_sprintf("%s: What tag?", gobj_yuno_role_plus_name()),
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
            json_sprintf("%s: Select some yuno please", gobj_yuno_role_plus_name()),
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
            json_sprintf("%s: What multiple?", gobj_yuno_role_plus_name()),
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
            json_sprintf("%s: Select some yuno please", gobj_yuno_role_plus_name()),
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
    hgobj gobj,
    json_t *node // not owned
)
{
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
            json_array_append(jn_data, webix?yuno2multiselect(gobj, node):node);
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
    hgobj gobj,
    json_t *iter  // owned
)
{
    json_t *jn_data = json_array();
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_array_append(jn_data, yuno2multiselect(gobj, node));
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
    json_t *jn_data = webix?yunos2multilselect(gobj, iter):iter;

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
 *  The create-yuno rows of the yunos with a newer binary or configuration
 *  (find_new_yunos.c). A row whose instance at the new release already
 *  exists -- a create=1 of an upgrade that was never promoted -- is not a
 *  row to create: create-yuno would answer "Yuno already exists". The
 *  preview still lists it, marked, because there IS something left to do
 *  (deactivate-snap promotes it), and `yunetas upgrade-yunos` stops at an
 *  empty preview; create=1 skips it.
 ***************************************************************************/
PRIVATE json_t *cmd_find_new_yunos(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL create = kw_get_bool(gobj, kw, "create", 0, KW_WILD_NUMBER);
    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    json_t *jn_rows = find_new_yunos(
        gobj,
        priv->resource,
        kw_incref(kw),  // filter
        force,
        src
    );

    json_t *jn_commands = json_array(); // what create=1 runs
    int registered = 0;
    json_t *jn_data = find_new_yunos_preview( // the preview
        gobj,
        jn_rows,
        jn_commands,
        &registered
    );
    json_decref(jn_rows);

    json_t *jn_comment = 0;
    if(registered > 0) {
        jn_comment = json_sprintf(
            "%s: %d yuno(s) already registered at the new release, pending promotion: run deactivate-snap",
            gobj_yuno_role_plus_name(),
            registered
        );
    }

    int ret = 0;
    json_t *schema = 0;
    if(create) {
        json_t *new_jn_data = json_array();
        int idx; json_t *jn_row;
        json_array_foreach(jn_commands, idx, jn_row) {
            const char *command = json_string_value(jn_row);
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
        if(ret == 0 && json_array_size(jn_commands)) {
            schema = tranger2_list_topic_desc_cols(
                gobj_read_pointer_attr(priv->resource, "tranger"),
                "yunos"
            );
        }
    }
    json_decref(jn_commands);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        ret,
        jn_comment,
        schema,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_create_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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
            json_sprintf("%s: Realm not found: '%s' ", gobj_yuno_role_plus_name(), realm_id),
            0,
            0,
            kw  // owned
        );
    }

    /*---------------------------------------------*
     *      Role
     *---------------------------------------------*/
    json_t *hs_binary = find_binary_version(gobj, priv->resource, yuno_role, role_version);
    if(!hs_binary) {
        json_t *comment = json_sprintf(
            "%s: Binary '%s%s%s' not found",
            gobj_yuno_role_plus_name(),
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
        priv->resource,
        SDATA_GET_STR(hs_binary, "id"),
        yuno_name,
        name_version
    );
    if(!hs_configuration) {
        json_t *comment = json_sprintf(
            "%s: Yuno '%s.%s': configuration not found '%s.%s%s%s.json' ",
            gobj_yuno_role_plus_name(),
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
    char yuno_release[NAME_MAX];
    if(build_release_name(gobj, yuno_release, sizeof(yuno_release), hs_binary, hs_configuration)<0) {
        /*
         *  Error already logged. A release cut short was stored before: a
         *  pkey2 that find-new-yunos never finds, so it offered the yuno
         *  again at every call, and create=1 failed on it for ever.
         */
        json_t *comment = json_sprintf(
            "%s: Yuno '%s.%s': release name too long ('%s' + '%s')",
            gobj_yuno_role_plus_name(),
            yuno_role, yuno_name,
            SDATA_GET_STR(hs_binary, "version"),
            SDATA_GET_STR(hs_configuration, "version")
        );
        json_decref(hs_realm);
        json_decref(hs_binary);
        json_decref(hs_configuration);
        return msg_iev_build_response(gobj,
            -1,
            comment,
            0,
            0,
            kw  // owned
        );
    }
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
                json_sprintf("%s: Yuno already exists", gobj_yuno_role_plus_name()),
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

    /*
     *  Seed the launch tier for framework utilities: a 'util'-tagged yuno
     *  (the same set run_util_yunos starts first) is born at start_priority 1
     *  unless the caller passed one. App tiers are assigned per node with
     *  `yunetas set-start-priorities`; no app role names belong here.
     */
    if(!kw_has_key(kw, "start_priority")) {
        const char *yuno_tag = kw_get_str(gobj, kw, "yuno_tag", "", 0);
        if(!empty_string(yuno_tag) && strstr(yuno_tag, "util")) {
            json_object_set_new(kw, "start_priority", json_integer(1));
        }
    }

    gobj_log_set_last_message("%s", "");    // the cause answered is of this create and its links
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
            json_sprintf("%s: Cannot create yuno: %s",
                gobj_yuno_role_plus_name(), last_failure_cause()),
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
     *  A known limit: a release that is not promoted yet (find-new-yunos
     *  create=1, before deactivate-snap) already writes ITS binary's
     *  description, schema and connector into the public services of the
     *  yuno id, while the old release still serves them (the port is kept)
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
            json_sprintf("%s: %s", gobj_yuno_role_plus_name(), last_failure_cause()):
            json_sprintf("%s: Created", gobj_yuno_role_plus_name()),
        tranger2_list_topic_desc_cols(gobj_read_pointer_attr(priv->resource, "tranger"), resource),
        jn_data, // owned
        kw  // owned
    );

    return webix;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_yuno(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    /*
     *  The id is required. Without it the filter is empty and selects
     *  every yuno: up to 7.29.3 `delete-yuno whole=1` with no id deleted
     *  every yuno that was not running, and `yuno_release=` alone that
     *  release of every yuno (a release is two versions, shared). Checked
     *  here, not with SDF_REQUIRED: the parser takes a required parameter
     *  by position, and refuses the `id=X` every caller writes.
     */
    if(empty_string(kw_get_str(gobj, kw, "id", "", 0))) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: delete-yuno needs an id: without one it selects every yuno",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  A yuno_release (the pkey2) targets that exact instance; otherwise the
     *  in-memory primary. See list_delete_targets().
     */
    BOOL by_instance = !empty_string(kw_get_str(gobj, kw, "yuno_release", "", 0));

    /*
     *  Deleting the WHOLE yuno must be asked for, never inherited from an
     *  omission.  Without `yuno_release` the target is the in-memory primary,
     *  i.e. the yuno itself and every release behind it -- the destructive
     *  reading was the one you got by typing LESS, and it has already cost a
     *  realm its auth_bff.  `force` does not stand in for this: it deletes a
     *  release a snap froze, a different question.
     */
    BOOL whole = kw_get_bool(gobj, kw, "whole", 0, KW_WILD_NUMBER);

    /*
     *  One or the other: a release and the whole yuno are two deletes, and
     *  the command does not guess which was meant. Up to 7.29.3 whole=1
     *  with a release other than the primary's deleted that release only,
     *  and with the primary's release every release.
     */
    if(by_instance && whole) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: delete-yuno takes yuno_release=<release> (one release) "
                "or whole=1 (the yuno and all of its releases), not both",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    if(!by_instance && !whole) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: refusing to delete the WHOLE yuno by omission. "
                "Add yuno_release=<release> to delete one release, "
                "or whole=1 to delete the yuno and all of its releases.",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get a iter of matched resources.
     */
    json_t *iter = list_delete_targets(gobj, resource, "yuno_release", kw, src);

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: Select some yuno please", gobj_yuno_role_plus_name()),
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
        /*
         *  The release of the PRIMARY is not one release: c_node's delete
         *  of the primary's own pkey2 reaches treedb_delete_node(), which
         *  takes every release of the yuno. So, with other releases behind
         *  it, it is the whole yuno, and that is asked with whole=1. Up to
         *  7.29.3 it passed as one release and deleted them all.
         */
        if(by_instance) {
            BOOL whole_key = FALSE;
            json_t *taken = instances_taken_by_delete(
                gobj, resource, "yuno_release", node, by_instance, &whole_key, src
            );
            int releases = (int)json_array_size(taken);
            JSON_DECREF(taken)
            if(whole_key && releases > 1) {
                json_t *comment = json_sprintf(
                    "%s: cannot delete release '%s' of yuno '%s' alone: it is the primary, "
                    "and its delete takes every release of the yuno (%d). "
                    "Use whole=1 without yuno_release to delete the yuno and all of its releases.",
                    gobj_yuno_role_plus_name(),
                    SDATA_GET_STR(node, "yuno_release"),
                    SDATA_GET_ID(node),
                    releases
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
         *  Instance records carry a stale yuno_running (the live runtime flag
         *  lives on the in-memory primary, not on the per-release record). When
         *  targeting an instance, a release is "running" only if it IS the
         *  primary's current running release, so consult the primary.
         */
        BOOL yuno_running;
        if(by_instance) {
            json_t *primary = gobj_get_node(
                priv->resource,
                resource,
                json_pack("{s:s}", "id", SDATA_GET_ID(node)),
                json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                src
            );
            yuno_running = kw_get_bool(gobj, primary, "yuno_running", 0, 0) &&
                strcmp(
                    kw_get_str(gobj, primary, "yuno_release", "", 0),
                    SDATA_GET_STR(node, "yuno_release")
                )==0;
            JSON_DECREF(primary);
        } else {
            yuno_running = kw_get_bool(gobj, node, "yuno_running", 0, KW_REQUIRED);
        }
        if(yuno_running > 0) {
            json_t *comment = json_sprintf(
                "%s: Cannot delete yuno '%s', it's running",
                gobj_yuno_role_plus_name(),
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
    }

    /*
     *  Delete. The links go with the yuno (`force`, always); a release a
     *  snap froze goes only when asked (`ignore_snaps` = this command's
     *  `force`). The snapshot guard is the treedb's, and it reads the
     *  RECORDS of the key: the tag in memory it used to read here is 0 for
     *  anything saved after the shot, and the `force = 1` that followed
     *  switched the treedb's own guard off too (up to 7.24.1), so a frozen
     *  release was deleted without force.
     */
    int result = 0;
    int deleted = 0;
    gobj_log_set_last_message("%s", "");    // the cause answered is of these deletes
    json_array_foreach(iter, idx, node) {
        /*
         *  In instance mode pass the pkey2 (yuno_release) too, so mt_delete_node
         *  routes to the durable per-instance delete instead of the whole-key path.
         */
        json_t *kw_delete = json_pack("{s:s}", "id", SDATA_GET_ID(node));
        if(by_instance) {
            json_object_set_new(kw_delete, "yuno_release",
                json_string(SDATA_GET_STR(node, "yuno_release")));
        }
        if(gobj_delete_node(
            priv->resource,
            resource,
            kw_delete,
            json_pack("{s:b, s:b}", "force", 1, "ignore_snaps", force),
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
            json_sprintf("%s: %s", gobj_yuno_role_plus_name(), last_failure_cause()):
            json_sprintf("%s: %d yunos deleted", gobj_yuno_role_plus_name(), deleted),
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
     *  HACK play=1 (default): auto-play on connect (ac_on_open reconciles must_play).
     *  play=0: only launch the process(es); the caller must issue play-yuno later.
     *  Splitting run (launch) from play lets scripts get exactly ONE response per
     *  command: run-yuno aggregates the launch ACKs into a single counter, and the
     *  later play-yuno aggregates over already-running yunos.
     */
    BOOL do_play = kw_get_bool(gobj, kw, "play", 1, KW_WILD_NUMBER);

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
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPERATIONAL,
            "msg",              "%s", "Yuno not found or already running",
            "kw",               "%j", kw,
            NULL
        );
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "%s: Yuno not found or already running",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*---------------------------------------*
     *      Get the requester
     *---------------------------------------*/
    // Who asked: ac_final_count() answers it, all ACKs in or the counter timed out
    const char *requester = kw_get_str(
        gobj, kw, "__md_iev__`ievent_gate_stack`0`input_channel", 0, KW_REQUIRED
    );

    /*
     *  Launch in ascending start_priority order (utilities before gates/dba).
     */
    sort_yunos_by_start_priority(gobj, iter, TRUE);

    /*------------------------------------------------*
     *  Walk over yunos iter:
     *      run
     *      add filter for future counter.
     *------------------------------------------------*/
    json_t *filterlist = json_array();
    int total_run = 0;
    int total_living = 0;

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
        const char *id = SDATA_GET_ID(yuno);
        /*
         *  Skip a yuno already launched and not open yet, like the boot sweeps
         *  do: yuno_running only turns TRUE in ac_on_open(), so a run-yuno
         *  landing in that window would launch a second instance.
         */
        if(!disabled && !yuno_running && !is_launching(gobj, id)) {
            if(yuno_lives_unregistered(gobj, yuno)) {
                total_living++;
                continue;   // Warning already logged
            }
            int r = run_yuno(gobj, yuno, src);
            if(r==0) {
                json_t *jn_EvChkItem = json_pack("{s:s, s:{s:s, s:s, s:I}}",
                    "event", EV_ON_OPEN,
                    "filters",
                        "identity_card`realm_id", kw_get_str(gobj, yuno, "realm_id`0", "", KW_REQUIRED),
                        "identity_card`yuno_id", id,
                        "identity_card`launch_id", kw_get_int(gobj, yuno, "launch_id", 0, KW_REQUIRED)
                );
                json_array_append_new(filterlist, jn_EvChkItem);

                if(do_play) {
                    /*
                     *  Route the implicit play answer back to this requester:
                     *  ac_on_open's must_play reconcile will call play-yuno,
                     *  and gives the ACK to the `_requester` kept here.
                     */
                    json_object_set_new(yuno, "_requester", json_string(requester));
                    json_t *kw_requester = kw_get_dict(
                        gobj, kw, "__md_iev__", 0, KW_REQUIRED
                    );
                    json_object_set_new(yuno, "_requester_md_iev", kw_incref(kw_requester));
                } else {
                    /*
                     *  Launch-only: remember this launch_id so ac_on_open skips its
                     *  must_play auto-play once. Kept in-memory (transient), consumed
                     *  on the first connect. The watcher reuses the same launch_id on
                     *  a crash relaunch, but by then the marker is gone, so autonomous
                     *  must_play recovery is intact.
                     */
                    json_object_set_new(yuno, "_requester", json_string(""));
                    char s_launch_id[32];
                    snprintf(s_launch_id, sizeof(s_launch_id), "%lld",
                        (long long)kw_get_int(gobj, yuno, "launch_id", 0, KW_REQUIRED)
                    );
                    json_object_set_new(priv->no_play_launches, s_launch_id, json_true());
                }

                // volatil: memory only -- saved, a lifecycle write would put this release over a newer one
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
                    "msgset",           "%s", MSGSET_INTERNAL,
                    "msg",              "%s", "run_yuno() FAILED",
                    "ret",              "%d", r,
                    NULL
                );
            }
        }
    }

    if(!total_run) {
        JSON_DECREF(iter)
        JSON_DECREF(filterlist)
        if(total_living) {
            return msg_iev_build_response(gobj,
                -1,
                json_sprintf(
                    "%s: %d yuno(s) alive but not connected to the agent: not launched again",
                    gobj_yuno_role_plus_name(), total_living
                ),
                0,
                0,
                kw  // owned
            );
        }
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("%s: Yuno not found to run", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*--------------------------------------*
     *  Crea con counter un futuro
     *  que nos indique cuando han arrancado
     *  all yunos arrancados.
     *  HACK one counter for the whole set (like kill/pause/play): a single
     *  command answer when all launched yunos have connected back.
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
            "iter", iter,          // HACK free en diferido, en ac_final_count()
            "kw_answer", kw_answer // HACK free en diferido, en ac_final_count()
    );

    hgobj gobj_counter = gobj_create_volatil( // run-yuno
        sequence_name(gobj),
        C_COUNTER,
        kw_counter,
        gobj
    );

    /*
     *  Subscribe al objeto counter a los eventos del router
     */
    json_t *kw_sub = json_pack("{s:{s:s}}",
        "__config__", "__rename_event_name__", EV_COUNT
    );
    gobj_subscribe_event(
        priv->gobj_input_side,
        EV_ON_OPEN,
        kw_sub,
        gobj_counter
    );

    /*
     *  Subscribe me to the counter's final event, with the requester:
     *  ac_final_count() answers it
     */
    json_t *kw_final_count = json_object();
    json_t *global = json_object();
    json_object_set_new(kw_final_count, "__global__", global);
    json_object_set_new(global, "requester", json_string(requester));

    gobj_subscribe_event(gobj_counter, EV_FINAL_COUNT, kw_final_count, gobj);

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
    json_t *kw_unconnected = json_deep_copy(kw);
    json_object_set_new(kw, "yuno_running", json_true());

    json_t *iter = gobj_list_nodes(
        priv->resource,
        resource,
        kw_incref(kw), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    /*
     *  And the ones alive that the agent does not know running (they lost
     *  their channel, or outlived a restart of the agent): nothing will say
     *  they closed, so they are signalled and said now, not counted. Up to
     *  7.25.21 kill-yuno could not reach them.
     */
    int signal_unconnected = (int)gobj_read_integer_attr(gobj, "signal2kill");
    if(force) {
        signal_unconnected = SIGKILL;
    } else if(!signal_unconnected) {
        signal_unconnected = SIGQUIT;
    }
    json_object_set_new(kw_unconnected, "yuno_running", json_false());
    json_t *jn_unconnected = signal_unconnected_yunos(
        gobj, kw_unconnected, app, signal_unconnected, src
    );

    if(json_array_size(iter)==0) {
        JSON_DECREF(iter)
        if(json_array_size(jn_unconnected) > 0) {
            return msg_iev_build_response(gobj,
                0,
                json_sprintf("%s: %d yuno(s) alive but not connected to the agent: signalled (%d), "
                    "not waited for: a run-yuno before they are gone does not launch them again",
                    gobj_yuno_role_plus_name(), (int)json_array_size(jn_unconnected), signal_unconnected
                ),
                0,
                jn_unconnected, // owned
                kw  // owned
            );
        }
        JSON_DECREF(jn_unconnected)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "%s: Yuno not found or already not running",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    int prev_signal2kill = 0;
    if(force) {
        prev_signal2kill = (int)gobj_read_integer_attr(gobj, "signal2kill");
        gobj_write_integer_attr(gobj, "signal2kill", SIGKILL);
    }

    /*---------------------------------------*
     *      Get the requester
     *---------------------------------------*/
    // Who asked: ac_final_count() answers it, all ACKs in or the counter timed out
    const char *requester = kw_get_str(
        gobj, kw, "__md_iev__`ievent_gate_stack`0`input_channel", 0, KW_REQUIRED
    );

    /*
     *  Kill in descending start_priority order so utilities die last
     *  (logcenter stays up to capture everyone's shutdown).
     */
    sort_yunos_by_start_priority(gobj, iter, FALSE);

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
        hold_restarted_yuno(gobj, id, FALSE, "kill");   // stopped by the operator: not for the restart to launch
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
                JSON_DECREF(jn_unconnected)
                return msg_iev_build_response(gobj,
                    -1,
                    json_sprintf(
                        "%s: Can't kill yuno: %s",
                        gobj_yuno_role_plus_name(), last_failure_cause()
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
        if(json_array_size(jn_unconnected) > 0) {
            return msg_iev_build_response(gobj,
                0,
                json_sprintf("%s: %d yuno(s) alive but not connected to the agent: signalled (%d), "
                    "not waited for: a run-yuno before they are gone does not launch them again",
                    gobj_yuno_role_plus_name(), (int)json_array_size(jn_unconnected), signal_unconnected
                ),
                0,
                jn_unconnected, // owned
                kw  // owned
            );
        }
        JSON_DECREF(jn_unconnected)
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("%s: Yuno not found to kill", gobj_yuno_role_plus_name()),
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

    char info[NAME_MAX];
    if(json_array_size(jn_unconnected) > 0) {
        snprintf(info, sizeof(info), "%d yunos found to kill, %d alive but not connected signalled (not waited for)",
            total_killed, (int)json_array_size(jn_unconnected)
        );
    } else {
        snprintf(info, sizeof(info), "%d yunos found to kill", total_killed);
    }
    JSON_DECREF(jn_unconnected)
    json_t *kw_counter = json_pack("{s:s, s:i, s:I, s:o, s:{s:o, s:o}}",
        "info", info,
        "max_count", total_killed,
        "expiration_timeout", priv->timeout_expiration,
        "input_schema", filterlist, // owned
        "__user_data__",
            "iter", iter,                   // HACK free en diferido, en ac_final_count()
            "kw_answer", kw_answer          // HACK free en diferido, en ac_final_count()
    );

    hgobj gobj_counter = gobj_create_volatil( // kill-yuno
        sequence_name(gobj),
        C_COUNTER,
        kw_counter,
        gobj
    );

    json_t *kw_sub = json_pack("{s:{s:s}}",
        "__config__", "__rename_event_name__", EV_COUNT
    );

    /*
     *  Subscribe al objeto counter a los eventos del router
     */
    gobj_subscribe_event(
        priv->gobj_input_side,
        EV_ON_CLOSE,
        kw_sub,
        gobj_counter
    );

    /*
     *  Subscribe me to the counter's final event, with the requester:
     *  ac_final_count() answers it
     */
    json_t *kw_final_count = json_object();
    json_t *global = json_object();
    json_object_set_new(kw_final_count, "__global__", global);
    json_object_set_new(global, "requester", json_string(requester));

    gobj_subscribe_event(gobj_counter, EV_FINAL_COUNT, kw_final_count, gobj);

    gobj_start(gobj_counter);

    KW_DECREF(kw);
    return 0;   // Asynchronous response
}

/***************************************************************************
 *  HACK this command could come from internal auto-play
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
                "%s: Yuno not found or already playing",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------------------------*
     *      Get the requester
     *  HACK it can come from internal auto-play
     *-----------------------------------------------*/
    // Who asked: ac_final_count() answers it, all ACKs in or the counter timed out (empty: an auto-play of ac_on_open())
    const char *requester = kw_get_str(
        gobj, kw, "__md_iev__`ievent_gate_stack`0`input_channel", "", 0
    );

    /*------------------------------------------------*
     *  Walk over yunos iter:
     *      play
     *      add filter for future counter.
     *------------------------------------------------*/
    json_t *filterlist = json_array();
    // int total_already_playing = 0;
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
                    "event", EV_PLAY_YUNO_ACK,
                    "filters",
                        "__md_iev__`__id__", (json_int_t)filter_ref
                );
                json_array_append_new(filterlist, jn_EvChkItem);
                total_to_played++;
            }
        } else {
            // total_already_playing++;
        }
    }

    if(!total_to_played && !total_to_preplayed) {
        JSON_DECREF(iter)
        JSON_DECREF(filterlist);
        return msg_iev_build_response(gobj,
            0,
            json_sprintf("%s: Yuno not found to play", gobj_yuno_role_plus_name()),
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
                "%s: %d yunos found to preplay",
                gobj_yuno_role_plus_name(),
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
    json_t *kw_answer = kw_incref(kw);

    char info[80];
    snprintf(info, sizeof(info), "%d to preplay, %d to play",
        total_to_preplayed,
        total_to_played
    );

    json_t *kw_counter = json_pack("{s:s, s:i, s:I, s:o, s:{s:o, s:o}}",
        "info", info,
        "max_count", total_to_played,
        "expiration_timeout", priv->timeout_expiration * 6,
        "input_schema", filterlist, // owned
        "__user_data__",
            "iter", iter,                   // HACK free en diferido, en ac_final_count()
            "kw_answer", kw_answer          // HACK free en diferido, en ac_final_count()
    );

    hgobj gobj_counter = gobj_create_volatil( // play-yuno
        sequence_name(gobj),
        C_COUNTER,
        kw_counter,
        gobj
    );

    json_t *kw_sub = json_pack("{s:{s:s}}",
        "__config__", "__rename_event_name__", EV_COUNT
    );

    /*
     *  Subscribe al objeto counter a los eventos del router
     */
    gobj_subscribe_event(
        gobj,
        EV_PLAY_YUNO_ACK,
        kw_sub,
        gobj_counter
    );

    /*
     *  Subscribe me to the counter's final event, with the requester:
     *  ac_final_count() answers it
     */
    json_t *kw_final_count = json_object();
    json_t *global = json_object();
    json_object_set_new(kw_final_count, "__global__", global);
    json_object_set_new(global, "requester", json_string(requester));

    gobj_subscribe_event(gobj_counter, EV_FINAL_COUNT, kw_final_count, gobj);

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
                "%s: Yuno not found or already in pause",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*---------------------------------------*
     *      Get the requester
     *---------------------------------------*/
    // Who asked: ac_final_count() answers it, all ACKs in or the counter timed out
    const char *requester = kw_get_str(
        gobj, kw, "__md_iev__`ievent_gate_stack`0`input_channel", 0, KW_REQUIRED
    );

    /*
     *  Pause in descending start_priority order (mirror of kill).
     */
    sort_yunos_by_start_priority(gobj, iter, FALSE);

    /*------------------------------------------------*
     *  Walk over yunos iter:
     *      pause
     *      add filter for future counter.
     *------------------------------------------------*/
    json_t *filterlist = json_array();
    // int total_already_pausing = 0;
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
                    "event", EV_PAUSE_YUNO_ACK,
                    "filters",
                        "__md_iev__`__id__", (json_int_t)filter_ref
                );
                json_array_append_new(filterlist, jn_EvChkItem);
                total_to_paused++;
            }
        } else {
            // total_already_pausing++;
        }
    }

    if(!total_to_paused && !total_to_prepaused) {
        JSON_DECREF(filterlist);
        JSON_DECREF(iter)
        return msg_iev_build_response(gobj,
            0,
            json_sprintf("%s: Yuno not found to pause", gobj_yuno_role_plus_name()),
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
                "%s: %d yunos found to prepause",
                gobj_yuno_role_plus_name(),
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
    json_t *kw_answer = kw_incref(kw);

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

    hgobj gobj_counter = gobj_create_volatil( // pause-yuno
        sequence_name(gobj),
        C_COUNTER,
        kw_counter,
        gobj
    );
    json_t *kw_sub = json_pack("{s:{s:s}}",
        "__config__", "__rename_event_name__", EV_COUNT
    );

    /*
     *  Subscribe al objeto counter a los eventos del router
     */
    gobj_subscribe_event(
        gobj,
        EV_PAUSE_YUNO_ACK,
        kw_sub,
        gobj_counter
    );

    /*
     *  Subscribe me to the counter's final event, with the requester:
     *  ac_final_count() answers it
     */
    json_t *kw_final_count = json_object();
    json_t *global = json_object();
    json_object_set_new(kw_final_count, "__global__", global);
    json_object_set_new(global, "requester", json_string(requester));

    gobj_subscribe_event(gobj_counter, EV_FINAL_COUNT, kw_final_count, gobj);

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
                "%s: Yuno not found or already enabled",
                gobj_yuno_role_plus_name()
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
        unhold_restarted_yuno(gobj, SDATA_GET_ID(node), "disable");

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
                "%s: Yuno not found or already disabled",
                gobj_yuno_role_plus_name()
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
        hold_restarted_yuno(gobj, SDATA_GET_ID(node), FALSE, "disable");
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
                "%s: Yuno not found or already tracing",
                gobj_yuno_role_plus_name()
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
                "%s: Yuno not found or already not tracing",
                gobj_yuno_role_plus_name()
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
            json_sprintf("%s: What command?", gobj_yuno_role_plus_name()),
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
            json_sprintf("%s: Yuno not found", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Send command
     *------------------------------------------------*/
    int sent = 0;
    json_t *jn_unreached = json_array();
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Command to yuno
         */
        json_t *kw_yuno = json_deep_copy(kw);
        if(command_to_yuno(gobj, yuno, json_string_value(jn_command), kw_yuno, src) < 0) {
            json_array_append_new(jn_unreached, json_string(kw_get_str(gobj, yuno, "id", "", 0)));
        } else {
            sent++;
        }
    }
    JSON_DECREF(iter)
    json_t *jn_response = unreached_yunos_response(
        gobj, json_string_value(jn_command), jn_unreached, sent, kw
    );
    JSON_DECREF(jn_command);
    if(jn_response) {
        return jn_response;
    }

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
            json_sprintf("%s: Yuno not found", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Send stats
     *------------------------------------------------*/
    int sent = 0;
    json_t *jn_unreached = json_array();
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Command to yuno
         */
        json_t *kw_yuno = json_deep_copy(kw);
        if(stats_to_yuno(gobj, yuno, stats, kw_yuno, src) < 0) {
            json_array_append_new(jn_unreached, json_string(kw_get_str(gobj, yuno, "id", "", 0)));
        } else {
            sent++;
        }
    }
    JSON_DECREF(iter)
    json_t *jn_response = unreached_yunos_response(gobj, cmd, jn_unreached, sent, kw);
    if(jn_response) {
        return jn_response;
    }

    KW_DECREF(kw);
    return 0;   /* Asynchronous response */
}

/***************************************************************************
 *  Send the stats of some yunos to the requester every period, pushed as
 *  EV_YUNO_STATS along the route the request came from -- the way the PTY
 *  mirror sends EV_TTY_DATA -- instead of the requester asking stats-yuno
 *  again and again. A subscription could not do it: they travel from a
 *  client to a server only, and the agent is the SERVER of its yunos.
 *
 *  One watch per requester -- its channel, and the client at the other end
 *  of the route, since every web client of a control center shares that
 *  control center's channel: a new call replaces it, stop=1 or the channel
 *  closing ends it, and so does `watch_ttl` passing without a new call
 *  (the requester renews it; behind a control center its leaving is never
 *  seen here). The client is told by the hop the agent can trust (see
 *  watch_route_name()): its own input channel's when direct, and through a
 *  control center the hop stamped THERE -- the channel it came in by
 *  (`input_channel`) and its connection (`cc_connection`): two tabs of one
 *  browser share the rest, and until 7.25.15 they shared one watch, the
 *  second replacing the first at each renewal. Until 7.25.20 it was the
 *  LAST hop, which the client writes: a forged one named another user's
 *  watch, to stop or replace it.
 *  A yuno may be named more than once with different services; an id that
 *  is not a yuno of this agent does not refuse the watch: it is watched,
 *  and its "state" reading says `missing` (it is named in the answer too);
 *  more than `max_watch_ids` names are refused. The watch is refused unless
 *  the requester says it takes EV_YUNO_STATS (`__relays__`), a control
 *  center in between or a client connected directly: one that does not
 *  know an event drops the connection (a control center) or logs "Event
 *  NOT DEFINED" on each one until the ttl (ycommand). The first readings
 *  follow the answer (EV_WATCH_READING, posted), to this requester only.
 *  Each period, per watched yuno, the
 *  requester gets a "state" event (running, playing, disabled, from the
 *  agent's own record) and, while the yuno runs, a "cpu" one (the stats of
 *  its __yuno__) and an "app" one (the stats of its service). The readings
 *  are taken here, on loopback, ONCE per yuno and service however many
 *  requesters watch it: the timer runs at the shortest period asked.
 *
 *      watch-yuno-stats ids=gen1,7601,7501:db period=2000
 *      watch-yuno-stats stop=1
 ***************************************************************************/
PRIVATE json_t *cmd_watch_yuno_stats(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj top_service = gobj_nearest_top_service(src);
    const char *route_service = gobj_name(top_service);
    const char *route_child = gobj_name(src);
    BOOL relayed = (top_service != priv->gobj_input_side)? TRUE : FALSE;

    /*
     *  The requester: the channel, and who is at the far end of the route,
     *  told by a hop of the ievent stack that the agent can trust.
     */
    char route_name[PATH_MAX];
    if(watch_route_name(gobj, route_name, sizeof(route_name), route_service, route_child, kw, relayed)<0) {
        // Error already logged
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: the route of this request is too long to watch it",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    if(kw_get_bool(gobj, kw, "stop", 0, KW_WILD_NUMBER)) {
        BOOL had = kw_has_key(priv->watches, route_name);
        json_object_del(priv->watches, route_name);
        watch_rearm_timer(gobj);
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("%s: %s",
                gobj_yuno_role_plus_name(),
                had? "watch stopped" : "no watch to stop"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_refusal = watch_refusal(kw, EV_YUNO_STATS, relayed);
    if(jn_refusal) {
        return msg_iev_build_response(
            gobj,
            -1,
            jn_refusal, // owned
            0,
            0,
            kw  // owned
        );
    }

    const char *ids = kw_get_str(gobj, kw, "ids", "", 0);
    if(empty_string(ids)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what yunos? ids=<id>[:<service>],...",
                gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(!kw_has_key(priv->watches, route_name) &&
            kw_size(priv->watches) >= gobj_read_integer_attr(gobj, "max_watches")) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: too many watches: %d",
                gobj_yuno_role_plus_name(),
                (int)kw_size(priv->watches)),
            0,
            0,
            kw  // owned
        );
    }

    json_int_t period = kw_get_int(gobj, kw, "period", 2000, KW_WILD_NUMBER);
    json_int_t min_period = gobj_read_integer_attr(gobj, "watch_min_period");
    if(period < min_period) {
        period = min_period;
    }

    /*
     *  {id: [service, ...]}: one yuno may be read through several services
     */
    json_t *jn_comment = NULL;
    json_t *jn_yunos = watch_ids(ids, gobj_read_integer_attr(gobj, "max_watch_ids"), &jn_comment);
    if(!jn_yunos) {
        return msg_iev_build_response(gobj, -1, jn_comment, 0, 0, kw);
    }
    json_t *jn_missing = json_array();
    const char *id; json_t *jn_services;
    json_object_foreach(jn_yunos, id, jn_services) {
        json_t *yuno = gobj_get_node(
            priv->resource,
            "yunos",
            json_pack("{s:s}", "id", id),
            json_pack("{s:b}", "only_id", 1),
            src
        );
        if(!yuno) {
            json_array_append_new(jn_missing, json_string(id));
        }
        JSON_DECREF(yuno)
    }

    json_int_t ttl = gobj_read_integer_attr(gobj, "watch_ttl");
    json_t *jn_watch = json_pack("{s:s, s:s, s:O, s:O, s:I, s:I}",
        "route_service", route_service,
        "route_child", route_child,
        "__md_iev__", kw_get_dict(gobj, kw, "__md_iev__", 0, KW_REQUIRED),
        "yunos", jn_yunos,
        "period", period,
        "expires", (json_int_t)(ttl > 0? start_msectimer((uint64_t)ttl) : 0)
    );
    if(!jn_watch) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "cannot create watch",
            "route",        "%s", route_name,
            NULL
        );
        JSON_DECREF(jn_yunos)
        JSON_DECREF(jn_missing)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot create the watch", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    json_object_set_new(priv->watches, route_name, jn_watch);
    watch_rearm_timer(gobj);

    json_t *jn_data = json_pack("{s:O, s:o, s:I, s:I}",
        "yunos", jn_yunos,
        "missing", jn_missing,   // owned
        "period", period,
        "ttl", ttl
    );
    jn_comment = json_array_size(jn_missing) > 0?
        json_sprintf("%s: watching %d yunos every %d ms, %d of them not found here",
            gobj_yuno_role_plus_name(), (int)json_object_size(jn_yunos), (int)period,
            (int)json_array_size(jn_missing)) :
        json_sprintf("%s: watching %d yunos every %d ms",
            gobj_yuno_role_plus_name(), (int)json_object_size(jn_yunos), (int)period);
    JSON_DECREF(jn_yunos)

    /*
     *  The first readings of this watch now, not one period later -- and
     *  after this answer, which is sent when the command returns.
     */
    gobj_post_event(gobj, EV_WATCH_READING, json_pack("{s:s}", "route", route_name), gobj);

    return msg_iev_build_response(gobj, 0, jn_comment, 0, jn_data, kw);
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
            json_sprintf("%s: Yuno not found", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*------------------------------------------------*
     *      Send authzs
     *------------------------------------------------*/
    int sent = 0;
    json_t *jn_unreached = json_array();
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Command to yuno
         */
        json_t *kw_yuno = json_deep_copy(kw);
        if(authzs_to_yuno(gobj, yuno, kw_yuno, src) < 0) {
            json_array_append_new(jn_unreached, json_string(kw_get_str(gobj, yuno, "id", "", 0)));
        } else {
            sent++;
        }
    }
    JSON_DECREF(iter)
    json_t *jn_response = unreached_yunos_response(gobj, cmd, jn_unreached, sent, kw);
    if(jn_response) {
        return jn_response;
    }

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
            json_sprintf("%s: What command?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(strcasecmp(command, cmd)==0) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("%s: Loop command", gobj_yuno_role_plus_name()),
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
                json_sprintf("%s: Service '%s' not found", gobj_yuno_role_plus_name(), service),
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
                json_sprintf("%s: Service '%s' not found", gobj_yuno_role_plus_name(), service),
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
PRIVATE json_t *cmd_set_graceful_kill(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    gobj_write_integer_attr(gobj, "signal2kill", SIGQUIT);
    return msg_iev_build_response(gobj,
        0,
        json_sprintf("%s: Set kill mode = graceful (SIGQUIT)",
            gobj_yuno_role_plus_name()),
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
        json_sprintf("%s: Set kill mode = quick (with SIGKILL)",
            gobj_yuno_role_plus_name()),
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

    /*
     *  What it finds is the ANSWER, as C_TRANGER's check-json: an ERROR per
     *  json held more than max_refcount times (with the old default 1,
     *  every node of the agent's treedb) filled the log
     */
    json_int_t max_refcount = kw_get_int(gobj, kw, "max_refcount", 0, 0);

    json_t *tranger = gobj_read_pointer_attr(priv->resource, "tranger");
    json_t *jn_report = json_refcounts_report(tranger, max_refcount, 20);
    if(!jn_report) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf("%s: no tranger to check", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );  // Error already logged
    }
    json_int_t corrupted = kw_get_int(gobj, jn_report, "corrupted", 0, 0);
    json_int_t over_max = kw_get_int(gobj, jn_report, "over_max", 0, 0);
    BOOL truncated = kw_get_bool(gobj, jn_report, "truncated", 0, 0);
    if(corrupted > 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Corrupted jsons in the tranger",
            "corrupted",    "%" JSON_INTEGER_FORMAT, corrupted,
            NULL
        );
    }
    /*
     *  -1 is a fault, as C_TRANGER's: a corrupted json or a walk cut short;
     *  what is held more than max_refcount times is the answer, not a fault
     */
    return msg_iev_build_response(gobj,
        (corrupted > 0 || truncated)? -1 : 0,
        json_sprintf(
            "%s: check refcounts of tranger: %" JSON_INTEGER_FORMAT " corrupted, "
            "%" JSON_INTEGER_FORMAT " held more than %" JSON_INTEGER_FORMAT " times%s",
            gobj_yuno_role_plus_name(), corrupted, over_max, max_refcount,
            truncated? " (the walk stopped short: no memory left, see the log)" : ""
        ),
        0,
        jn_report,  // owned
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

    BOOL all = kw_get_bool(gobj, kw, "all", 0, 0);
    json_object_del(kw, "all");     // the agent's, not the treedb's
    json_t *jn_resp = gobj_command(priv->resource, cmd, kw, src);
    int ret = kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    if(ret>=0) {
        ret = restart_nodes(gobj, all);
    }
    return jn_resp;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_deactivate_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL all = kw_get_bool(gobj, kw, "all", 0, 0);
    json_object_del(kw, "all");     // the agent's, not the treedb's
    json_t *jn_resp = gobj_command(priv->resource, cmd, kw, src);
    int ret = kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    if(ret>=0) {
        ret = restart_nodes(gobj, all);
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
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), cmd),
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
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), cmd),
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

    add_binary_file_time(jn_data);

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), cmd),
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
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), cmd),
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
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), cmd),
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
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
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
                        "msgset",       "%s", MSGSET_INTERNAL,
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
        json_sprintf("%s: ==> List consoles of agent: '%s'",
            gobj_yuno_role_plus_name(), node_uuid()),
        0,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *  The dict named `key` in `dict`. A console (or a route) is named by the
 *  user: the name is a KEY, as it is written (json_object_set), never a
 *  path -- read as one, a name holding the delimiter was not found, or
 *  found a field of another console.
 ***************************************************************************/
PRIVATE json_t *named_dict(hgobj gobj, json_t *dict, const char *key, BOOL required)
{
    json_t *jn = json_object_get(dict, key);
    if(json_is_object(jn)) {
        return jn;
    }
    if(required) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "named dict not found",
            "key",          "%s", key,
            NULL
        );
    }
    return NULL;
}

/***************************************************************************
 *  Take the dict named `key` out of `dict`: yours, NULL if not there.
 ***************************************************************************/
PRIVATE json_t *named_dict_take(json_t *dict, const char *key)
{
    json_t *jn = json_object_get(dict, key);
    if(!json_is_object(jn)) {
        return NULL;
    }
    json_incref(jn);
    json_object_del(dict, key);
    return jn;
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
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
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
    int cx = kw_get_int(gobj, kw, "cx", 80, KW_WILD_NUMBER);
    int cy = kw_get_int(gobj, kw, "cy", 24, KW_WILD_NUMBER);

    if(empty_string(name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: What console name?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(process)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: What process?", gobj_yuno_role_plus_name()),
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
        if(kw_size(priv->list_consoles) >= gobj_read_integer_attr(gobj, "max_consoles")) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf(
                    "%s: Too much opened consoles: %d",
                    gobj_yuno_role_plus_name(),
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
                json_sprintf("%s: Cannot open console: '%s'", gobj_yuno_role_plus_name(), name),
                0,
                0,
                kw  // owned
            );
        }
        gobj_set_volatil(gobj_console, TRUE);

        /*
         *  Save console
         */
        json_t *jn_console = json_pack("{s:s, s:{}}",
            "process", process,
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
        json_t *jn_console = named_dict(gobj, priv->list_consoles, name, TRUE);
        gobj_console = gobj_find_service(name, FALSE);
        if(!gobj_console) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: Console gobj not found: '%s'",
                    gobj_yuno_role_plus_name(), name),
                0,
                0,
                kw  // owned
            );
        }
        int ret = add_console_route(gobj, name, jn_console, src, kw);
        if(ret < 0 && ret != -2) {
            // -2 is not an error: same channel re-opening its console
            // (browser refresh), the route metadata was refreshed.
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("%s: Error opening console: '%s'",
                    gobj_yuno_role_plus_name(), name),
                0,
                0,
                kw  // owned
            );
        }

        /*
         *  Re-attach: the live PTY published EV_TTY_OPEN at start only,
         *  replay it to the requester so its console leaves "Connecting…".
         */
        char route_name[NAME_MAX];
        snprintf(route_name, sizeof(route_name), "%s.%s",
            gobj_name(gobj_nearest_top_service(src)),
            gobj_name(src)
        );
        json_t *jn_routes = kw_get_dict(gobj, jn_console, "routes", 0, KW_REQUIRED);
        json_t *jn_route = named_dict(gobj, jn_routes, route_name, TRUE);
        if(jn_route) {
            // Same shape as C_PTY's EV_TTY_OPEN publish (minus fd/slave_name)
            json_t *jn_data = json_pack("{s:s, s:s, s:s, s:s, s:i, s:i}",
                "name", name,
                "process", kw_get_str(gobj, jn_console, "process", "", KW_REQUIRED),
                "uuid", node_uuid(),
                "cwd", gobj_read_str_attr(gobj_console, "cwd"),
                "rows", (int)gobj_read_integer_attr(gobj_console, "rows"),
                "cols", (int)gobj_read_integer_attr(gobj_console, "cols")
            );
            gobj_send_event(
                src,
                EV_TTY_OPEN,
                msg_iev_build_response(gobj,
                    0,  // result
                    0,  // comment
                    0,  // schema
                    jn_data,  // owned
                    kw_incref(jn_route)  // owned
                ),
                gobj
            );
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
        json_sprintf("%s: Console opened: '%s'", gobj_yuno_role_plus_name(), name),  // owned
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
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }


    /*----------------------------------------*
     *  Close console
     *----------------------------------------*/
    const char *name = kw_get_str(gobj, kw, "name", "", 0);
    // BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    if(empty_string(name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: What console name?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    json_t *jn_console = named_dict(gobj, priv->list_consoles, name, FALSE);
    if(!jn_console) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: Console not found: '%s'", gobj_yuno_role_plus_name(), name),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Delete console or route
     */
    int ret = 0;
    hgobj gobj_console = gobj_find_service(name, TRUE);
    if(gobj_console) {
        ret = gobj_stop(gobj_console); // volatil, auto-destroy
    } else {
        ret = -1;
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(
        gobj,
        ret,
        json_sprintf("%s: Console closed: '%s'", gobj_yuno_role_plus_name(), name),
        0,
        0, // owned
        kw  // owned
    );
}

/***************************************************************************
 *  Command: cert-sync-now
 *  Trigger a cert-sync tick immediately (admin override).
 ***************************************************************************/
PRIVATE json_t *cmd_cert_sync_now(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    int changed = cert_sync_tick(gobj);
    json_t *resp = build_command_response(
        gobj,
        0,
        json_sprintf("%s: cert-sync tick done: %s",
            gobj_yuno_role_plus_name(),
            changed > 0 ? "reloaded" : "no change"),
        0,
        json_pack("{s:s, s:i, s:s}",
            "result",   gobj_read_str_attr(gobj, "cert_sync_last_result"),
            "changed",  changed,
            "store_dir", gobj_read_str_attr(gobj, "cert_sync_store_dir")
        )
    );
    JSON_DECREF(kw);
    return resp;
}

/***************************************************************************
 *  Command: cert-sync-status
 *  Show current cert-sync state: interval, last check, last action, etc.
 ***************************************************************************/
PRIVATE json_t *cmd_cert_sync_status(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /* Read /var/lib/yuneta/last-deploy-hook-run if present (set by the
     * letsencrypt deploy hook). A missing/stale value is a strong hint
     * that the hook is failing.
     */
    json_int_t hook_last_run = 0;
    FILE *f = fopen("/var/lib/yuneta/last-deploy-hook-run", "r");
    if(f) {
        long long v = 0;
        if(fscanf(f, "%lld", &v) == 1) {
            hook_last_run = (json_int_t)v;
        }
        fclose(f);
    }

    json_t *data = json_pack("{s:b, s:i, s:s, s:s, s:i, s:i, s:s, s:i, s:i}",
        "enabled",           priv->cert_sync_enabled,
        "interval_sec",      (json_int_t)priv->cert_sync_interval_sec,
        "store_dir",         gobj_read_str_attr(gobj, "cert_sync_store_dir"),
        "copy_cmd",          gobj_read_str_attr(gobj, "cert_sync_copy_cmd"),
        "last_check",        gobj_read_integer_attr(gobj, "cert_sync_last_check"),
        "last_action",       gobj_read_integer_attr(gobj, "cert_sync_last_action"),
        "last_result",       gobj_read_str_attr(gobj, "cert_sync_last_result"),
        "failures",          gobj_read_integer_attr(gobj, "cert_sync_failures"),
        "deploy_hook_last_run", hook_last_run
    );

    json_t *resp = build_command_response(gobj, 0, 0, 0, data);
    JSON_DECREF(kw);
    return resp;
}




            /***************************
             *      Local Methods
             ***************************/




/*****************************************************************
 *
 *****************************************************************/
PRIVATE const char *sequence_name(hgobj gobj)
{
    static char se_name[120];
    static int n = 0;
    n++;
    snprintf(se_name, sizeof(se_name), "%d", n);
    return se_name;
}

/*****************************************************************
 *  The cause of a failure, for its answer: what the last ERROR
 *  said, or "(see the log)" when nothing did. A refusal said with a
 *  WARNING (treedb_delete_node()'s "has down links") does not fill
 *  gobj_log_last_message(): up to 7.29.3 the answer named an empty
 *  cause, or the cause of an older failure. Clear it before the call
 *  whose cause is answered (gobj_log_set_last_message("%s", "")).
 *****************************************************************/
PRIVATE const char *last_failure_cause(void)
{
    const char *m = gobj_log_last_message();
    return empty_string(m)? "(see the log)" : m;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE int is_yuneta_agent(pid_t pid)
{
    /*
     *  0 an agent runs with the pid, 1 nothing does, 2 something that is
     *  not an agent does: a zombie (its cmdline is empty) or another
     *  program; -1 cannot tell (errno). EPERM is an agent of another user
     *  as much as a stranger: its cmdline is readable, so it is read.
     */
    if(kill(pid, 0) < 0 && errno != EPERM) {
        return (errno == ESRCH)? 1 : -1;
    }
    char cmdline[1024];
    if(read_process_cmdline(cmdline, sizeof(cmdline), pid) < 0) {
        return (errno == ENOENT || errno == ESRCH)? 1 : -1;
    }
    if(strstr(cmdline, "yuneta_agent ")) {
        return 0;
    }
    return 2;
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
        /*
         *  Re-open through the same channel (e.g. a browser refresh behind
         *  the controlcenter): the stored __md_iev__ belongs to the previous
         *  requester, refresh it so replies route to the new one.
         */
        json_t *jn_route = json_object_get(jn_routes, route_name);
        json_object_set(
            jn_route,
            "__md_iev__",
            kw_get_dict(gobj, kw, "__md_iev__", 0, KW_REQUIRED)
        );
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
 *
 ***************************************************************************/
PRIVATE int add_console_in_input_gate(hgobj gobj, const char *name, hgobj src)
{
    /*  The console name is the user's: a KEY of "consoles", as the close
     *  removes it, not a segment of a path.  */
    json_t *consoles = gobj_kw_get_user_data( // save in input gate
        src,
        "consoles",
        json_object(), // owned
        KW_CREATE
    );
    if(!json_is_object(consoles) || json_object_set_new(consoles, name, json_true()) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "cannot save the console in the input gate",
            "console",      "%s", name,
            NULL
        );
        return -1;
    }

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
    json_t *jn_console = named_dict_take(priv->list_consoles, name);

    hgobj gobj_console = gobj_find_service(name, FALSE);

    if(!jn_console) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
                    "msgset",       "%s", MSGSET_INTERNAL,
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
    hgobj gobj_channel = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    json_t *consoles = gobj_kw_get_user_data(gobj_channel, "consoles", 0, 0);
    if(!consoles) {
        return 0;
    }

    /*
     *  Copy the key: `name` IS the key of `consoles`, and delete_console()
     *  deletes it from there (the route of this very channel) while it
     *  goes on reading it -- with two or more routes, freed memory. As the
     *  stop of the agent does it.
     */
    const char *name; json_t *jn_; void *n;
    json_object_foreach_safe(consoles, n, name, jn_) {
        char *name_ = gbmem_strdup(name);   // any length: a gobj name can exceed NAME_MAX - 1
        delete_console(gobj, name_);
        GBMEM_FREE(name_);
    }

    return 0;
}

/***************************************************************************
 *  Run the watch timer at the shortest period of the watches, or stop it.
 ***************************************************************************/
PRIVATE int watch_rearm_timer(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t period = 0;
    const char *route_name; json_t *jn_watch;
    json_object_foreach(priv->watches, route_name, jn_watch) {
        json_int_t p = kw_get_int(gobj, jn_watch, "period", 0, KW_REQUIRED);
        if(p > 0 && (period == 0 || p < period)) {
            period = p;
        }
    }
    if(period == priv->watch_period) {
        return 0;
    }
    priv->watch_period = period;
    if(period == 0) {
        clear_timeout(priv->watch_timer);
        /*
         *  Nobody watches any more: the yunos stop publishing
         */
        void *n; const char *yuno_id; json_t *jn_;
        json_object_foreach_safe(priv->publishing, n, yuno_id, jn_) {
            publications_stop(gobj, yuno_id);
        }
    } else {
        set_timeout_periodic(priv->watch_timer, period);
    }
    return 0;
}

/***************************************************************************
 *  Send one EV_YUNO_STATS to every requester watching `yuno_id` (and
 *  `service`, when given), or to the one of `only_route`. A requester that
 *  is gone takes its watch with it.
 ***************************************************************************/
PRIVATE int watch_send(
    hgobj gobj,
    const char *only_route, // NULL: every requester
    const char *yuno_id,
    const char *service,    // NULL: whatever service the requester reads
    json_t *jn_data         // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Walk a copy: a send can close the requester's channel on the spot,
     *  and its ac_on_close removes the watch from priv->watches.
     */
    json_t *jn_watches = json_copy(priv->watches);
    json_t *jn_gone = json_array();
    const char *route_name; json_t *jn_watch;
    json_object_foreach(jn_watches, route_name, jn_watch) {
        if(only_route && strcmp(route_name, only_route) != 0) {
            continue;
        }
        json_t *jn_yunos = kw_get_dict(gobj, jn_watch, "yunos", 0, KW_REQUIRED);
        json_t *jn_services = json_object_get(jn_yunos, yuno_id);
        if(!jn_services) {
            continue;
        }
        if(service && !json_str_in_list(gobj, jn_services, service, FALSE)) {
            continue;
        }
        const char *route_service = kw_get_str(gobj, jn_watch, "route_service", "", KW_REQUIRED);
        const char *route_child = kw_get_str(gobj, jn_watch, "route_child", "", KW_REQUIRED);
        hgobj gobj_route_service = gobj_find_service(route_service, FALSE);
        hgobj gobj_input_gate = gobj_route_service?
            gobj_child_by_name(gobj_route_service, route_child) : 0;
        if(!gobj_input_gate || !requester_is_listening(gobj, gobj_input_gate, EV_YUNO_STATS)) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "watch-yuno-stats requester gone, watch dropped",
                "route",        "%s", route_name,
                NULL
            );
            json_array_append_new(jn_gone, json_string(route_name));
            continue;
        }
        /*
         *  As an iev, like the answers of ac_stats_yuno_answer(): the channel
         *  (or the controlcenter's C_IEVENT_CLI) serializes it to the peer.
         */
        json_t *kw_ev = msg_iev_build_response(gobj,
            0,  // result
            0,  // comment
            0,  // schema
            json_incref(jn_data), // owned
            kw_incref(jn_watch)  // owned, used to get ONLY __md_iev__
        );
        gobj_send_event(
            gobj_input_gate,
            EV_SEND_IEV,
            iev_create(gobj, EV_YUNO_STATS, kw_ev),
            gobj
        );
    }

    if(json_array_size(jn_gone) > 0) {
        size_t idx; json_t *jn_route;
        json_array_foreach(jn_gone, idx, jn_route) {
            json_object_del(priv->watches, json_string_value(jn_route));
        }
        watch_rearm_timer(gobj);
    }
    JSON_DECREF(jn_gone)
    JSON_DECREF(jn_watches)
    return 0;
}

/***************************************************************************
 *  One reading of every watched yuno: its state at once, and its cpu and
 *  service stats asked on loopback (they come back through
 *  ac_stats_yuno_answer, marked in __md_iev__`agent_watch). With
 *  `only_route`, of the yunos of that watch only, sent to it only (the
 *  first readings of a new or renewed watch).
 ***************************************************************************/
PRIVATE int watch_tick(hgobj gobj, const char *only_route)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Watches not asked again within watch_ttl go
     */
    BOOL expired = FALSE;
    const char *route_name; json_t *jn_watch; void *n;
    json_object_foreach_safe(priv->watches, n, route_name, jn_watch) {
        uint64_t expires = (uint64_t)kw_get_int(gobj, jn_watch, "expires", 0, 0);
        if(test_msectimer(expires)) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "watch-yuno-stats not renewed, expired",
                "route",        "%s", route_name,
                NULL
            );
            json_object_del(priv->watches, route_name);
            expired = TRUE;
        }
    }
    if(expired) {
        watch_rearm_timer(gobj);
    }

    /*
     *  The (yuno, service) pairs asked by anybody, each read once
     */
    json_t *jn_asked = json_object();
    json_object_foreach(priv->watches, route_name, jn_watch) {
        if(only_route && strcmp(route_name, only_route) != 0) {
            continue;
        }
        json_t *jn_yunos = kw_get_dict(gobj, jn_watch, "yunos", 0, KW_REQUIRED);
        const char *yuno_id; json_t *jn_watched;
        json_object_foreach(jn_yunos, yuno_id, jn_watched) {
            json_t *jn_services = json_object_get(jn_asked, yuno_id);
            if(!jn_services) {
                jn_services = json_object();
                json_object_set_new(jn_asked, yuno_id, jn_services);
            }
            size_t i; json_t *jn_service;
            json_array_foreach(jn_watched, i, jn_service) {
                json_object_set_new(jn_services, json_string_value(jn_service), json_true());
            }
        }
    }

    const char *yuno_id; json_t *jn_services;
    json_object_foreach(jn_asked, yuno_id, jn_services) {
        json_t *yuno = gobj_get_node(
            priv->resource,
            "yunos",
            json_pack("{s:s}", "id", yuno_id),
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            gobj
        );
        BOOL running = yuno? kw_get_bool(gobj, yuno, "yuno_running", 0, 0) : FALSE;
        hgobj channel_gobj = yuno?
            (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, 0) : 0;

        json_t *jn_state = json_pack("{s:s, s:s, s:b, s:b, s:b, s:b}",
            "yuno_id", yuno_id,
            "kind", "state",
            "missing", yuno? 0 : 1,
            "yuno_running", running,
            "yuno_playing", yuno? kw_get_bool(gobj, yuno, "yuno_playing", 0, 0) : 0,
            "yuno_disabled", yuno? kw_get_bool(gobj, yuno, "yuno_disabled", 0, 0) : 0
        );
        watch_send(gobj, only_route, yuno_id, NULL, jn_state);
        JSON_DECREF(jn_state)

        if(running && channel_gobj) {
            /*
             *  What the yuno publishes is not asked for; the first
             *  readings of a watch (only_route) are always asked
             */
            if(!only_route) {
                publications_arm(gobj, yuno, yuno_id, jn_services);
            }
            if(only_route || !publication_is_on(gobj, yuno_id, "__yuno__")) {
                json_t *kw_cpu = json_pack("{s:s}", "service", "__yuno__");
                kw_set_subdict_value(gobj, kw_cpu, "__md_iev__", "agent_watch",
                    json_pack("{s:s, s:s, s:s, s:s}", "yuno_id", yuno_id, "kind", "cpu", "service", "",
                        "route", only_route? only_route : "")
                );
                stats_to_yuno(gobj, yuno, "", kw_cpu, gobj);
            }

            const char *service; json_t *jn_;
            json_object_foreach(jn_services, service, jn_) {
                if(!only_route && publication_is_on(gobj, yuno_id, service)) {
                    continue;
                }
                json_t *kw_app = json_object();
                if(!empty_string(service)) {
                    json_object_set_new(kw_app, "service", json_string(service));
                }
                kw_set_subdict_value(gobj, kw_app, "__md_iev__", "agent_watch",
                    json_pack("{s:s, s:s, s:s, s:s}", "yuno_id", yuno_id, "kind", "app", "service", service,
                        "route", only_route? only_route : "")
                );
                stats_to_yuno(gobj, yuno, "", kw_app, gobj);
            }
        } else if(!only_route) {
            json_object_del(priv->publishing, yuno_id);  // no process: nothing is published
        }
        JSON_DECREF(yuno)
    }

    /*
     *  The yunos nobody watches any more stop publishing
     */
    if(!only_route) {
        void *n2; const char *pub_yuno_id; json_t *jn_pub;
        json_object_foreach_safe(priv->publishing, n2, pub_yuno_id, jn_pub) {
            if(!json_object_get(jn_asked, pub_yuno_id)) {
                publications_stop(gobj, pub_yuno_id);
            }
        }
    }

    JSON_DECREF(jn_asked)
    return 0;
}

/***************************************************************************
 *  publish-stats to a yuno: the stats of `service` (`__yuno__` is the cpu
 *  reading of a watch, "" the service named as the role, as
 *  stats_to_yuno() reads it) at the period of the watches, with watch_ttl
 *  as its ttl. Its readings come back as the agent's own stats answers,
 *  marked in __md_iev__`agent_watch like the ones watch_tick() asks for;
 *  the answer to the command itself is marked in __md_iev__`agent_publish
 *  (publication_answer()).
 ***************************************************************************/
PRIVATE int publication_ask(
    hgobj gobj,
    json_t *yuno,   // not owned
    const char *yuno_id,
    const char *service,
    json_int_t pid,
    BOOL stop
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL cpu = strcmp(service, "__yuno__")==0;
    const char *stats_service = service;
    if(empty_string(stats_service)) {
        stats_service = kw_get_str(gobj, yuno, "yuno_role", "", 0);
    }
    json_int_t ttl = gobj_read_integer_attr(gobj, "watch_ttl");

    json_t *kw_pub = json_pack("{s:s, s:s, s:I, s:I, s:b}",
        "service", "__yuno__",      // the command is the yuno's (C_YUNO)
        "stats_service", stats_service,
        "period", (json_int_t)priv->watch_period,
        "ttl", ttl > 0? ttl : (json_int_t)0,
        "stop", stop
    );
    kw_set_subdict_value(gobj, kw_pub, "__md_iev__", "agent_watch",
        json_pack("{s:s, s:s, s:s, s:s}", "yuno_id", yuno_id, "kind", cpu? "cpu" : "app",
            "service", cpu? "" : service, "route", "")
    );
    kw_set_subdict_value(gobj, kw_pub, "__md_iev__", "agent_publish",
        json_pack("{s:s, s:s, s:I, s:b}", "yuno_id", yuno_id, "service", service,
            "pid", pid, "stop", stop)
    );
    return command_to_yuno(gobj, yuno, "publish-stats", kw_pub, gobj);
}

/***************************************************************************
 *  The yunos publish their own stats (publish-stats, C_YUNO) instead of
 *  being asked for them every period. For a watched yuno and each service
 *  of it somebody watches (`__yuno__`: its cpu reading), ask for a
 *  publication; ask again at half its ttl, when the period of the watches
 *  changes, or when the process does (a new one publishes nothing); stop
 *  the services nobody watches. Until a publication is on, and for good
 *  when the yuno refuses it (one built before 7.28.11, or a service it does
 *  not have), the agent samples the yuno as before.
 ***************************************************************************/
PRIVATE int publications_arm(hgobj gobj, json_t *yuno, const char *yuno_id, json_t *jn_services)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t pid = kw_get_int(gobj, yuno, "yuno_pid", 0, 0);
    json_t *jn_pub = json_object_get(priv->publishing, yuno_id);
    if(jn_pub && kw_get_int(gobj, jn_pub, "pid", 0, KW_REQUIRED) != pid) {
        json_object_del(priv->publishing, yuno_id);
        jn_pub = NULL;
    }

    BOOL again = FALSE;
    if(!jn_pub) {
        jn_pub = json_pack("{s:I, s:I, s:I, s:{}}",
            "pid", pid,
            "period", (json_int_t)0,
            "renew", (json_int_t)0,
            "services"
        );
        json_object_set_new(priv->publishing, yuno_id, jn_pub);
        again = TRUE;
    }
    uint64_t renew = (uint64_t)kw_get_int(gobj, jn_pub, "renew", 0, KW_REQUIRED);
    if(kw_get_int(gobj, jn_pub, "period", 0, KW_REQUIRED) != priv->watch_period ||
            (renew && test_msectimer(renew))) {
        again = TRUE;
    }
    if(again) {
        json_int_t ttl = gobj_read_integer_attr(gobj, "watch_ttl");
        json_object_set_new(jn_pub, "period", json_integer(priv->watch_period));
        json_object_set_new(jn_pub, "renew",
            json_integer(ttl > 0? (json_int_t)start_msectimer(ttl/2) : 0));
    }

    json_t *jn_states = kw_get_dict(gobj, jn_pub, "services", 0, KW_REQUIRED);
    json_t *jn_wanted = json_object();
    json_object_set_new(jn_wanted, "__yuno__", json_true());
    const char *service; json_t *jn_;
    json_object_foreach(jn_services, service, jn_) {
        json_object_set_new(jn_wanted, service, json_true());
    }

    json_object_foreach(jn_wanted, service, jn_) {
        const char *state = json_string_value(json_object_get(jn_states, service));
        if(state && strcmp(state, "refused")==0) {
            continue;   // it does not publish: sampled
        }
        if(!state || again) {
            if(!state) {
                json_object_set_new(jn_states, service, json_string("asked"));
            }
            publication_ask(gobj, yuno, yuno_id, service, pid, FALSE);
        }
    }

    void *n;
    json_object_foreach_safe(jn_states, n, service, jn_) {
        if(json_object_get(jn_wanted, service)) {
            continue;
        }
        if(strcmp(json_string_value(jn_), "refused")!=0) {
            publication_ask(gobj, yuno, yuno_id, service, pid, TRUE);
        }
        json_object_del(jn_states, service);
    }

    JSON_DECREF(jn_wanted)
    return 0;
}

/***************************************************************************
 *  TRUE when the yuno publishes the stats of `service` itself
 ***************************************************************************/
PRIVATE BOOL publication_is_on(hgobj gobj, const char *yuno_id, const char *service)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_pub = json_object_get(priv->publishing, yuno_id);
    json_t *jn_states = jn_pub? json_object_get(jn_pub, "services") : NULL;
    const char *state = jn_states? json_string_value(json_object_get(jn_states, service)) : NULL;
    return (state && strcmp(state, "on")==0)? TRUE : FALSE;
}

/***************************************************************************
 *  Nobody watches the yuno any more: its publications stop
 ***************************************************************************/
PRIVATE int publications_stop(hgobj gobj, const char *yuno_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_pub = json_object_get(priv->publishing, yuno_id);
    if(!jn_pub) {
        return 0;
    }
    json_t *yuno = gobj_get_node(
        priv->resource,
        "yunos",
        json_pack("{s:s}", "id", yuno_id),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );
    json_int_t pid = kw_get_int(gobj, jn_pub, "pid", 0, KW_REQUIRED);
    if(yuno && kw_get_bool(gobj, yuno, "yuno_running", 0, 0) &&
            kw_get_int(gobj, yuno, "yuno_pid", 0, 0) == pid) {
        json_t *jn_states = kw_get_dict(gobj, jn_pub, "services", 0, KW_REQUIRED);
        const char *service; json_t *jn_state;
        json_object_foreach(jn_states, service, jn_state) {
            if(strcmp(json_string_value(jn_state), "refused")!=0) {
                publication_ask(gobj, yuno, yuno_id, service, pid, TRUE);
            }
        }
    }
    JSON_DECREF(yuno)
    json_object_del(priv->publishing, yuno_id);
    return 0;
}

/***************************************************************************
 *  The answer of a yuno to a publish-stats of the agent: it publishes (the
 *  agent stops asking it for those stats) or refuses (it goes on asking).
 *  An answer of another process, or of a service nobody watches, is late.
 ***************************************************************************/
PRIVATE int publication_answer(hgobj gobj, json_t *jn_publish_md, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(kw_get_bool(gobj, jn_publish_md, "stop", 0, 0)) {
        return 0;
    }
    const char *yuno_id = kw_get_str(gobj, jn_publish_md, "yuno_id", "", KW_REQUIRED);
    const char *service = kw_get_str(gobj, jn_publish_md, "service", "", KW_REQUIRED);
    json_int_t pid = kw_get_int(gobj, jn_publish_md, "pid", 0, KW_REQUIRED);

    json_t *jn_pub = json_object_get(priv->publishing, yuno_id);
    if(!jn_pub || kw_get_int(gobj, jn_pub, "pid", 0, KW_REQUIRED) != pid) {
        return 0;
    }
    json_t *jn_states = kw_get_dict(gobj, jn_pub, "services", 0, KW_REQUIRED);
    const char *state = json_string_value(json_object_get(jn_states, service));
    if(!state) {
        return 0;
    }

    if(kw_get_int(gobj, kw, "result", -1, 0) < 0) {
        if(strcmp(state, "refused")!=0) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "publish-stats refused, the agent asks for these stats",
                "yuno_id",      "%s", yuno_id,
                "service",      "%s", service,
                "comment",      "%s", kw_get_str(gobj, kw, "comment", "", 0),
                NULL
            );
        }
        json_object_set_new(jn_states, service, json_string("refused"));
    } else {
        json_object_set_new(jn_states, service, json_string("on"));
    }
    return 0;
}

/***************************************************************************
 *  A reading of a watch came back: send it to the requesters watching it.
 ***************************************************************************/
PRIVATE int watch_answer(hgobj gobj, json_t *jn_watch_md, json_t *kw)
{
    const char *yuno_id = kw_get_str(gobj, jn_watch_md, "yuno_id", "", KW_REQUIRED);
    const char *kind = kw_get_str(gobj, jn_watch_md, "kind", "", KW_REQUIRED);
    const char *service = kw_get_str(gobj, jn_watch_md, "service", "", 0);
    const char *route = kw_get_str(gobj, jn_watch_md, "route", "", 0);

    json_t *jn_data = json_pack("{s:s, s:s, s:s, s:I, s:O?, s:O?}",
        "yuno_id", yuno_id,
        "kind", kind,
        "service", service,
        "result", (json_int_t)kw_get_int(gobj, kw, "result", 0, 0),
        "comment", kw_get_dict_value(gobj, kw, "comment", 0, 0),
        "data", kw_get_dict_value(gobj, kw, "data", 0, 0)
    );
    if(!jn_data) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "cannot build the watch reading",
            "yuno_id",      "%s", yuno_id,
            "kind",         "%s", kind,
            NULL
        );
        return -1;
    }
    watch_send(gobj, empty_string(route)? NULL : route, yuno_id,
        strcmp(kind, "app")==0? service : NULL, jn_data);
    JSON_DECREF(jn_data)
    return 0;
}

/***************************************************************************
 *  The channel of a requester closed: its watch goes.
 ***************************************************************************/
PRIVATE int delete_watch_of_channel(hgobj gobj, hgobj channel_gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *route_service = gobj_name(gobj_nearest_top_service(channel_gobj));
    const char *route_child = gobj_name(channel_gobj);

    BOOL deleted = FALSE;
    const char *route_name; json_t *jn_watch; void *n;
    json_object_foreach_safe(priv->watches, n, route_name, jn_watch) {
        if(strcmp(kw_get_str(gobj, jn_watch, "route_service", "", 0), route_service)==0 &&
                strcmp(kw_get_str(gobj, jn_watch, "route_child", "", 0), route_child)==0) {
            json_object_del(priv->watches, route_name);
            deleted = TRUE;
        }
    }
    if(deleted) {
        watch_rearm_timer(gobj);
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
        /*
         *  run_process2() and not system(): the command starts with no
         *  signal blocked (system() gives it the agent's mask, the signals
         *  it keeps for its signalfd), and SIGQUIT is not set to SIG_IGN
         *  while it runs (that throws away a stop that is pending)
         */
        char *const argv[] = {"sh", "-c", (char *)startup_command, NULL};
        int status = run_process2("/bin/sh", argv);
        if(status != 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "The startup command FAILED",
                "script",       "%s", startup_command,
                "status",       "%d", status,
                "exit_code",    "%d", (status > 0 && WIFEXITED(status))? WEXITSTATUS(status): -1,
                NULL
            );
        }
    }
    return 0;
}

/***************************************************************************
 *  role^name of a yuno: the name of its run script and of its
 *  configuration files. One that does not fit is refused (-1, logged, bf
 *  empty): up to 7.29.3 it was cut, and the files of the yuno were written
 *  under the cut name.
 ***************************************************************************/
PRIVATE int build_role_plus_name(hgobj gobj, char *bf, int bf_len, json_t *yuno)
{
    const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);
    const char *yuno_name = kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED);

    int n;
    if(empty_string(yuno_name)) {
        n = snprintf(bf, bf_len, "%s",
            yuno_role
        );
    } else {
        n = snprintf(bf, bf_len, "%s^%s",
            yuno_role,
            yuno_name
        );
    }
    if(n < 0 || n >= bf_len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "role^name of a yuno too long for a file name",
            "yuno_role",    "%s", yuno_role,
            "yuno_name",    "%s", yuno_name,
            NULL
        );
        if(bf_len > 0) {
            *bf = 0;
        }
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int build_role_plus_id(hgobj gobj, char *bf, int bf_len, json_t *yuno)
{
    const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);
    const char *yuno_name = kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED);
    const char *yuno_id = kw_get_str(gobj, yuno, "id", yuno_name, KW_REQUIRED);

    int n;
    if(empty_string(yuno_id)) {
        n = snprintf(bf, bf_len, "%s",
            yuno_role
        );
    } else {
        n = snprintf(bf, bf_len, "%s^%s",
            yuno_role,
            yuno_id
        );
    }
    if(n < 0 || n >= bf_len) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "role^id of a yuno too long for a directory name",
            "yuno_role",    "%s", yuno_role,
            "yuno_id",      "%s", yuno_id,
            NULL
        );
        if(bf_len > 0) {
            *bf = 0;
        }
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  A name of a binary (its id/role, its version, a tag): one directory or
 *  file name of /yuneta/repos/<tags>/<role>/<version>, and part of the
 *  temporary file that yuno_basic_information() runs through the shell.
 *  Only [A-Za-z0-9._-], not empty, not starting with '.', no "..".
 *  Logged when refused.
 *
 *  build_path() drops a "." segment: a version "." named the role's own
 *  directory (a delete-binary of that record removed every version), a
 *  role "." the directory of the tags, where other roles live.
 ***************************************************************************/
PRIVATE BOOL is_safe_binary_name(hgobj gobj, const char *what, const char *name)
{
    static const char allowed[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-";
    if(empty_string(name) || name[0] == '.' ||
            strspn(name, allowed) != strlen(name) || strstr(name, "..")) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Binary name refused: only [A-Za-z0-9._-], not empty, "
                                  "not starting with '.', no '..'",
            "what",         "%s", what,
            "name",         "%s", name? name: "",
            NULL
        );
        return FALSE;
    }
    return TRUE;
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
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "tags is not a list",
                NULL
            );
            return 0;
        }
        json_array_foreach(jn_l, index, jn_s_domain_name) {
            if(!json_is_string(jn_s_domain_name)) {
                gobj_log_error(0, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "a tag is not a string",
                    NULL
                );
                *bf = 0;
                return 0;
            }
            if(!is_safe_binary_name(0, "tag", json_string_value(jn_s_domain_name))) {
                *bf = 0;
                return 0;   // Error already logged
            }
            if(index == 0) {
                ln = snprintf(p, bflen, "%s", json_string_value(jn_s_domain_name));
            } else {
                ln = snprintf(p, bflen, "/%s", json_string_value(jn_s_domain_name));
            }
            if(ln<0) {
                gobj_log_error(0, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "snprintf() FAILED building tags dir",
                    "domain",       "%s", json_string_value(jn_s_domain_name),
                    NULL
                );
                *bf = 0;
                return 0;
            }
            if(ln >= bflen) {
                /*
                 *  snprintf truncated: the buffer is full and already
                 *  NUL-terminated. Stop before p/bflen advance past the end
                 *  (bflen would go negative and widen to a huge size_t in the
                 *  next snprintf -> out-of-bounds write). A truncated tags
                 *  path would silently place the yuno under a WRONG repos
                 *  dir — surface it.
                 */
                gobj_log_error(0, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "tags dir truncated, domain list too long",
                    "tags",         "%s", bf,
                    NULL
                );
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
    char tags[PATH_MAX];
    if(!multiple_dir(tags, sizeof(tags), jn_tags)) {
        *bf = 0;
        return 0;   // Error already logged
    }

    /*
     *  A path cut here is an ANCESTOR (.../<role>, .../<tags>): up to 7.29.3
     *  delete-binary rmrdir'ed it, every version of the role, and an install
     *  stored the binary there.
     */
    if(!build_path(bf, bfsize, work_dir, "repos", tags, yuno_role, yuno_version, NULL)) {
        *bf = 0;
        return 0;   // Error already logged
    }

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
    if(!yuneta_repos_yuno_dir(
        repos_dir,
        sizeof(repos_dir),
        jn_tags,
        yuno_role,
        yuno_version,
        create
    )) {
        *bf = 0;
        return 0;   // Error already logged
    }

    if(!build_path(bf, bfsize, repos_dir, filename, NULL)) {
        *bf = 0;
        return 0;   // Error already logged
    }
    return bf;
}

/***************************************************************************
 *  The directory of ONE version of a binary record,
 *  /yuneta/repos/<tags>/<role>/<version>, or NULL (logged, bf empty) when
 *  a name of the record is refused or the path does not fit. Never a
 *  shorter path: a "." or an empty name dropped, or a path cut, names an
 *  ANCESTOR -- every version of the role, or other roles.
 ***************************************************************************/
PRIVATE char *binary_version_dir(
    hgobj gobj,
    char *bf,
    int bfsize,
    json_t *jn_tags, // not owned
    const char *role,
    const char *version
)
{
    if(!is_safe_binary_name(gobj, "id", role) ||
            !is_safe_binary_name(gobj, "version", version)) {
        *bf = 0;
        return 0;   // Error already logged
    }
    /*
     *  The tags are checked by multiple_dir() with the same rule
     */
    return yuneta_repos_yuno_dir(bf, bfsize, jn_tags, role, version, FALSE);
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
    int n = snprintf(url, sizeof(url), "%s.%s.%s", realm_name, realm_role, realm_env);
    if(n < 0 || n >= (int)sizeof(url)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "name.role.env of a realm too long for a directory name",
            "realm_name",   "%s", realm_name,
            "realm_role",   "%s", realm_role,
            "realm_env",    "%s", realm_env,
            NULL
        );
        json_decref(realm);
        *bf = 0;
        return 0;
    }

    char role_plus_id[NAME_MAX];
    if(build_role_plus_id(gobj, role_plus_id, sizeof(role_plus_id), yuno) < 0) {
        json_decref(realm);
        *bf = 0;
        return 0;   // Error already logged
    }

    char *ret = build_path(bf, bfsize, "realms", realm_owner, url, role_plus_id, NULL);
    json_decref(realm);     // realm_owner lives in it
    if(!ret) {
        *bf = 0;
        return 0;   // Error already logged
    }
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char * build_yuno_bin_path(hgobj gobj, json_t *yuno, char *bf, int bfsize, BOOL create_dir)
{
    char private_domain[PATH_MAX];
    if(!build_yuno_private_domain(gobj, yuno, private_domain, sizeof(private_domain))) {
        *bf = 0;
        return 0;   // Error already logged
    }

    const char *work_dir = yuneta_root_dir();
    if(!build_path(bf, bfsize, work_dir, private_domain, "bin", NULL)) {
        *bf = 0;
        return 0;   // Error already logged
    }

    if(create_dir) {
        if(mkrdir(bf, yuneta_xpermission())<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
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
 *  A configuration file the agent did not finish (it died between the
 *  write and the rename) leaves a temporary file in the yuno's bin
 *  directory. Removed at the agent's start, when no write is under way,
 *  for every yuno: up to 7.25.21 only the next launch of that yuno removed
 *  it, and a yuno not launched again kept it.
 ***************************************************************************/
PRIVATE int remove_temp_config_files_of_yunos(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *iter = gobj_list_nodes(
        priv->resource,
        "yunos",
        json_object(), // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    int ret = 0;
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        char private_domain[PATH_MAX];
        if(!build_yuno_private_domain(gobj, yuno, private_domain, sizeof(private_domain))) {
            // Error already logged (no realm): no bin directory to look at
            ret = -1;
            continue;
        }
        char yuno_bin_path[PATH_MAX];
        if(!build_yuno_bin_path(gobj, yuno, yuno_bin_path, sizeof(yuno_bin_path), FALSE)) {
            ret = -1;   // Error already logged
            continue;
        }
        if(remove_temp_yuno_config_files(gobj, yuno_bin_path) < 0) {
            ret = -1;   // Error already logged
        }
    }
    JSON_DECREF(iter)

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char * build_yuno_log_path(hgobj gobj, json_t *yuno, char *bf, int bfsize, BOOL create_dir)
{
    char private_domain[PATH_MAX];
    if(!build_yuno_private_domain(gobj, yuno, private_domain, sizeof(private_domain))) {
        *bf = 0;
        return 0;   // Error already logged
    }

    const char *work_dir = yuneta_root_dir();
    if(!build_path(bf, bfsize, work_dir, private_domain, "logs", NULL)) {
        *bf = 0;
        return 0;   // Error already logged
    }

    if(create_dir) {
        if(mkrdir(bf, yuneta_xpermission())<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
                "msgset",       "%s", MSGSET_INTERNAL,
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
                "msgset",       "%s", MSGSET_INTERNAL,
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
                "msgset",       "%s", MSGSET_INTERNAL,
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
                "msgset",       "%s", MSGSET_INTERNAL,
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
    char *service_yuno_name = strchr(service, '.'); // `service` or `service.yuno_name`
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
                "msgset",           "%s", MSGSET_SERVICE,
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
                "msgset",           "%s", MSGSET_SERVICE,
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
        const char *ip =  SDATA_GET_STR(hs_service, "ip");
        uint32_t port_ =  SDATA_GET_INT(hs_service, "port");
        char port[32];
        snprintf(port, sizeof(port), "%d", port_);
        json_t * jn_config_variables = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
            "__realm_id__", realm_id,
            "__yuno_name__", yuno_name,
            "__yuno_role__", yuno_role,
            "__yuno_service__", yuno_service,
            "__ip__", ip,
            "__port__", port,
            "__schema__", schema,
            "__url__", url
        );

        json_t *kw_connector = json_replace_var( // old kw_apply_json_config_variables
            json_incref(jn_connector), // owned
            jn_config_variables // owned
        );

        json_array_append_new(jn_services, kw_connector);
        json_decref(hs_service);
    }

    int ret = write_yuno_json_file( // save: service connectors
        gobj,
        jn_yuno_services, // owned
        config_path
    );

    json_decref(hs_binary);

    return ret;
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

    if(bfbinary && bfbinary_size > 0) {
        bfbinary[0] = '\0';
    }

    /*
     *  Build the domain of yuno (defined by his realm)
     */
    char domain_dir[PATH_MAX];
    if(!build_yuno_private_domain(gobj, yuno, domain_dir, sizeof(domain_dir))) {
        return 0;   // Error already logged
    }

    char yuno_bin_path[PATH_MAX];
    if(!build_yuno_bin_path(gobj, yuno, yuno_bin_path, sizeof(yuno_bin_path), TRUE)) {
        return 0;   // Error already logged
    }

    json_t *hs_realm = get_yuno_realm(gobj, yuno);
    if(!hs_realm) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "get_yuno_realm() FAILED",
            "yuno_id",      "%s", yuno_id,
            NULL
        );
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

    /*
     *  Node placement, injected as __yuno__ attrs (c_yuno reads
     *  "sched_priority"/"cpu_core"). These are only defaults: the user config
     *  file, merged after this one, wins.
     */
    json_int_t sched_priority = kw_get_int(gobj, yuno, "sched_priority", 20, 0);
    json_int_t cpu_core = kw_get_int(gobj, yuno, "cpu_core", 0, 0);

    json_t *binary = get_yuno_binary(gobj, yuno);
    if(!binary) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "get_yuno_binary() FAILED",
            "yuno_id",      "%s", yuno_id,
            "yuno_role",    "%s", yuno_role,
            "yuno_release", "%s", yuno_release,
            NULL
        );
        json_decref(hs_realm);
        return 0;
    }
    const char *binary_path = kw_get_str(gobj, binary, "binary", "", KW_REQUIRED);
    char role_plus_name[NAME_MAX];
    if(build_role_plus_name(gobj, role_plus_name, sizeof(role_plus_name), yuno) < 0) {
        json_decref(hs_realm);
        json_decref(binary);
        return 0;   // Error already logged
    }

    /*
     *  Build the run script
     */
    snprintf(bfbinary, bfbinary_size, "%s", binary_path);

    char config_file_name[PATH_MAX+15];
    char config_path[(PATH_MAX+15)*2];
    int n_config = 0;
    BOOL write_failed = FALSE;
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
                "msgset",       "%s", MSGSET_PARAMETER,
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
        json_t *jn_content = json_pack("{s:o, s:o, s:{s:s, s:s, s:s, s:s, s:s, s:b, s:I, s:I, s:I}}",
            "global", jn_global,
            "environment", jn_environment,
            "yuno",
                "yuno_id", yuno_id,
                "yuno_name", yuno_name,
                "yuno_tag", yuno_tag,
                "yuno_release", yuno_release,
                "bind_ip", bind_ip,
                "yuno_multiple", multiple,
                "launch_id", (json_int_t)launch_id,
                "sched_priority", (json_int_t)sched_priority,
                "cpu_core", (json_int_t)cpu_core
        );
        json_t *jn_agent_environment = gobj_read_json_attr(gobj, "agent_environment");
        if(jn_agent_environment) {
            // HACK Override the yuno environment by agent environment.
            json_object_update(jn_environment, jn_agent_environment);
        }

        if(write_yuno_json_file( // save: environment and yuno variables
                gobj,
                jn_content, // owned
                config_path)<0) {
            write_failed = TRUE;    // Error already logged
        }
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

        if(write_yuno_config_file( // save: agent connector
                gobj,
                gbuf_config, // owned
                config_path)<0) {
            write_failed = TRUE;    // Error already logged
        }
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

            jn_config_required_services = kw_get_list(gobj, content, "yuno`required_services", 0, 0);
            if(jn_config_required_services) {
                json_incref(jn_config_required_services);
            }

            /*
             *  Exactly as it is: up to 7.29.2 through json2str(), its reals
             *  at 12 digits, into a gbuffer of 256 KB that cut a larger one
             */
            JSON_INCREF(content);
            if(write_yuno_json_file( // save: user configurations
                    gobj,
                    content, // owned
                    config_path)<0) {
                write_failed = TRUE;    // Error already logged
            }
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
            if(write_service_client_connectors( // save: service connectors
                    gobj,
                    yuno,
                    config_path,
                    jn_required_services)<0) {
                write_failed = TRUE;    // Error already logged
            }
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

    /*
     *  An earlier launch may have written more of them, with the mode of
     *  that time: the launch script no longer names them, the secrets are
     *  still in them. A failure is logged, and does not stop the launch.
     */
    narrow_stale_yuno_config_files(gobj, yuno_bin_path, role_plus_name, n_config);

    if(write_failed) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "A configuration file of the yuno cannot be written, not run",
            "yuno_id",      "%s", yuno_id,
            NULL
        );
        return 0;
    }
    return gbuf_script;
}

/***************************************************************************
 *  Reorder a yunos iter in place by the per-yuno 'start_priority' (band 0..9).
 *  ascending=TRUE for run-yuno (utilities first: logcenter/emailsender/auth_bff,
 *  then gates, then dba); ascending=FALSE for kill/pause so utilities die last
 *  (e.g. logcenter stays up to capture everyone's shutdown). Stable: yunos that
 *  share a priority keep their treedb order. Out-of-range values are clamped to
 *  the band; a yuno that predates the column reads its default (5).
 ***************************************************************************/
PRIVATE void sort_yunos_by_start_priority(hgobj gobj, json_t *iter, BOOL ascending)
{
    size_t size = json_array_size(iter);
    if(size < 2) {
        return;
    }

    json_t *sorted = json_array();
    for(int band=0; band<=9; band++) {
        int prio = ascending? band : (9 - band);
        size_t idx; json_t *yuno;
        json_array_foreach(iter, idx, yuno) {
            int p = (int)kw_get_int(gobj, yuno, "start_priority", 5, 0);
            if(p < 0) {
                p = 0;
            } else if(p > 9) {
                p = 9;
            }
            if(p == prio) {
                json_array_append(sorted, yuno);
            }
        }
    }

    if(json_array_size(sorted) != size) {
        /*
         *  Must never happen (clamp guarantees a bucket for every yuno): keep
         *  the original order rather than silently dropping any yuno.
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "sort_yunos_by_start_priority() lost yunos, keeping order",
            "before",       "%d", (int)size,
            "after",        "%d", (int)json_array_size(sorted),
            NULL
        );
        json_decref(sorted);
        return;
    }

    /*
     *  Replace iter contents in place: callers keep the same array pointer
     *  (cmd_run_yuno hands it to the counter's deferred free).
     */
    json_array_clear(iter);
    size_t idx; json_t *yuno;
    json_array_foreach(sorted, idx, yuno) {
        json_array_append(iter, yuno);
    }
    json_decref(sorted);
}

/***************************************************************************
 *  TRUE if the yuno was already launched and has not registered yet.
 *
 *  `yuno_running` only turns TRUE in ac_on_open(), so between run_yuno() and
 *  that handshake a yuno looks stopped and gets launched again. run_yuno()
 *  marks it and ac_on_open() drops the mark.
 *
 *  The mark holds a `timeout_expiration` msectimer because a yuno that dies
 *  before opening never clears it and the agent doesn't watch pids: a stale
 *  mark would otherwise block run-yuno for that yuno until the agent restarts.
 *  Same window the command counters already give a yuno to connect back.
 ***************************************************************************/
PRIVATE BOOL is_launching(hgobj gobj, const char *yuno_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_timer = json_object_get(priv->launching_yunos, yuno_id);
    if(!jn_timer) {
        return FALSE;
    }

    if(test_msectimer((uint64_t)json_integer_value(jn_timer))) {
        json_object_del(priv->launching_yunos, yuno_id);
        return FALSE;
    }

    return TRUE;
}

/***************************************************************************
 *  The pids of the processes that run this yuno, whether the agent knows
 *  them or not (its watcher and its child both): argv[0] is its role and
 *  its configuration files are in its bin dir. A json list of pids, owned;
 *  NULL if /proc cannot be read (logged): then nobody can tell.
 *
 *  yuno_running and yuno_pid say only what the agent saw on the yuno's
 *  channel, and are not persistent: a yuno that lost its channel, or that
 *  outlived a restart of the agent, is "not running" there while it lives.
 *  Its yuno.pid is no guide either: a yuno writes it where its own config
 *  says (logcenter's is under realms/agent/), not in the bin dir the agent
 *  made. A zombie has no cmdline: it is dead.
 ***************************************************************************/
PRIVATE json_t *find_living_yuno_pids(hgobj gobj, json_t *yuno)
{
    const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);

    char yuno_bin_path[PATH_MAX];
    if(!build_yuno_bin_path(gobj, yuno, yuno_bin_path, sizeof(yuno_bin_path), FALSE)) {
        return NULL;    // Error already logged
    }
    char config_mark[PATH_MAX];
    if(snprintf(config_mark, sizeof(config_mark), "%s/1-", yuno_bin_path) >= (int)sizeof(config_mark)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "yuno bin path too long: its processes cannot be looked for",
            "yuno_id",      "%s", SDATA_GET_ID(yuno),
            "path",         "%s", yuno_bin_path,
            NULL
        );
        return NULL;
    }

    DIR *dir = opendir("/proc");
    if(!dir) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read /proc: the processes of a yuno cannot be looked for",
            "yuno_id",      "%s", SDATA_GET_ID(yuno),
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    json_t *jn_pids = json_array();
    pid_t self = getpid();
    struct dirent *de;
    while((de = readdir(dir)) != NULL) {
        if(!all_numbers(de->d_name)) {
            continue;   // not a process
        }
        pid_t pid = (pid_t)atoi(de->d_name);
        if(pid <= 0 || pid == self) {
            continue;
        }
        char cmdline[2*PATH_MAX];
        if(read_process_cmdline(cmdline, sizeof(cmdline), pid) < 0) {
            continue;   // gone meanwhile
        }
        char *end_argv0 = strchr(cmdline, ' ');
        if(!end_argv0) {
            continue;   // no arguments: not a yuno launched with its configuration
        }
        *end_argv0 = 0;
        const char *argv0 = strrchr(cmdline, '/');
        argv0 = argv0? argv0 + 1 : cmdline;
        if(strcmp(argv0, yuno_role) != 0) {
            continue;
        }
        if(!strstr(end_argv0 + 1, config_mark)) {
            continue;
        }
        json_array_append_new(jn_pids, json_integer(pid));
    }
    closedir(dir);

    return jn_pids;
}

/***************************************************************************
 *  A yuno alive that the agent does not know running: not launched again.
 *  It comes back on its own once it reaches the agent (a yuno slow to
 *  start, or one that outlived a restart of the agent). A second instance
 *  fought the first for its exclusive resources: up to 7.25.21 it opened
 *  the persistent queues of a living one "as not master" and aborted.
 ***************************************************************************/
PRIVATE BOOL yuno_lives_unregistered(hgobj gobj, json_t *yuno)
{
    json_t *jn_pids = find_living_yuno_pids(gobj, yuno);
    if(!jn_pids) {
        return FALSE;   // Error already logged: launched as before
    }
    if(json_array_size(jn_pids) == 0) {
        JSON_DECREF(jn_pids)
        return FALSE;
    }

    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_OPERATIONAL,
        "msg",          "%s", "yuno alive but not connected to the agent: not launched again",
        "yuno_id",      "%s", SDATA_GET_ID(yuno),
        "yuno_role",    "%s", kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED),
        "yuno_name",    "%s", kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED),
        "yuno_release", "%s", kw_get_str(gobj, yuno, "yuno_release", "", KW_REQUIRED),
        "pids",         "%j", jn_pids,
        NULL
    );
    JSON_DECREF(jn_pids)
    return TRUE;
}

/***************************************************************************
 *  A bounce of the node (restart_nodes()) kills every yuno before it runs
 *  them again: also the ones alive that the agent does not know running,
 *  or they would outlive it beside their new instance
 ***************************************************************************/
PRIVATE void kill_yuno_unregistered(
    hgobj gobj,
    json_t *yuno,
    json_t *jn_killed   // the pids killed are appended here
)
{
    json_t *jn_pids = find_living_yuno_pids(gobj, yuno);
    if(!jn_pids) {
        return; // Error already logged
    }

    int idx; json_t *jn_pid;
    json_array_foreach(jn_pids, idx, jn_pid) {
        pid_t pid = (pid_t)json_integer_value(jn_pid);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL,
            "msg",          "%s", "yuno alive but not connected to the agent: killed with the node",
            "yuno_id",      "%s", SDATA_GET_ID(yuno),
            "yuno_role",    "%s", kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED),
            "pid",          "%d", (int)pid,
            NULL
        );
        json_array_append_new(jn_killed, json_integer(pid));
        if(kill(pid, SIGKILL) < 0 && errno != ESRCH) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot kill a yuno not connected to the agent",
                "yuno_id",      "%s", SDATA_GET_ID(yuno),
                "pid",          "%d", (int)pid,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
    }
    JSON_DECREF(jn_pids)
}

/***************************************************************************
 *  kill-yuno of the yunos alive that the agent does not know running: the
 *  processes of each one (its watcher and its child) get `signal2kill`; a
 *  SIGQUIT tells the watcher not to relaunch, and it ends with its child's
 *  exit (or at once, when it is waiting to relaunch), SIGKILL takes both. Return a list of {id, yuno_role, yuno_name, pids}, owned.
 ***************************************************************************/
PRIVATE json_t *signal_unconnected_yunos(
    hgobj gobj,
    json_t *kw_filter,  // owned
    BOOL app,
    int signal2kill,
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_signalled = json_array();
    json_t *iter = gobj_list_nodes(
        priv->resource,
        "yunos",
        kw_filter,  // owned
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        src
    );

    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        const char *id = SDATA_GET_ID(yuno);
        if(app && atoi(id) < 1000) {
            continue;
        }
        /*
         *  A yuno spared by a restart is never "running" for the agent: its
         *  kill-yuno comes here, not to the loop of the running ones
         */
        hold_restarted_yuno(gobj, id, FALSE, "kill");
        json_t *jn_pids = find_living_yuno_pids(gobj, yuno);
        if(!jn_pids) {
            continue;   // Error already logged
        }
        if(json_array_size(jn_pids) == 0) {
            JSON_DECREF(jn_pids)
            continue;
        }
        int idx2; json_t *jn_pid;
        json_array_foreach(jn_pids, idx2, jn_pid) {
            pid_t pid = (pid_t)json_integer_value(jn_pid);
            if(kill(pid, signal2kill) < 0 && errno != ESRCH) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot signal a yuno not connected to the agent",
                    "yuno_id",      "%s", id,
                    "pid",          "%d", (int)pid,
                    "signal",       "%d", signal2kill,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
            }
        }
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL,
            "msg",          "%s", "yuno alive but not connected to the agent: signalled by kill-yuno",
            "yuno_id",      "%s", id,
            "yuno_role",    "%s", kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED),
            "signal",       "%d", signal2kill,
            "pids",         "%j", jn_pids,
            NULL
        );
        json_array_append_new(jn_signalled, json_pack("{s:s, s:s, s:s, s:o}",
            "id", id,
            "yuno_role", kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED),
            "yuno_name", kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED),
            "pids", jn_pids     // owned
        ));
    }
    JSON_DECREF(iter)

    return jn_signalled;
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hold_restarted_yuno(gobj, SDATA_GET_ID(yuno), TRUE, "run");  // launched: not for the restart to run

    /*
     *  Launch id
     */
    static uint16_t counter = 0;
    uint64_t t;
    time((time_t*)&t);
    t = t<<(sizeof(uint16_t)*8);
    t += ++counter;
    json_object_set_new(yuno, "launch_id", json_integer(t));

    char bfbinary[PATH_MAX];
    bfbinary[0] = '\0';
    gbuffer_t *gbuf_sh = gbuffer_create(4*1024, 32*1024);
    if(!build_yuno_running_script(gobj, gbuf_sh, yuno, bfbinary, sizeof(bfbinary), FALSE)) {
        // Error already logged
        gbuffer_decref(gbuf_sh);
        return -1;
    }

    const char *realm_id = kw_get_str(gobj, yuno, "realm_id`0", "", KW_REQUIRED);
    const char *yuno_id = kw_get_str(gobj, yuno, "id", "", KW_REQUIRED);
    const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", KW_REQUIRED);
    const char *yuno_name = kw_get_str(gobj, yuno, "yuno_name", "", KW_REQUIRED);
    const char *yuno_tag = kw_get_str(gobj, yuno, "yuno_tag", "", KW_REQUIRED);
    const char *yuno_release = kw_get_str(gobj, yuno, "yuno_release", "", KW_REQUIRED);

    /*
     *  PATH_MAX, as build_yuno_running_script() and restart_nodes() build
     *  it: up to 7.29.2 a path cut at NAME_MAX here put the launch noted
     *  (note_yuno_launch()) and the .sh where nobody looks for them
     */
    char yuno_bin_path[PATH_MAX];
    if(!build_yuno_bin_path(gobj, yuno, yuno_bin_path, sizeof(yuno_bin_path), TRUE)) {
        gbuffer_decref(gbuf_sh);
        return -1;  // Error already logged
    }

    char role_plus_name[NAME_MAX];
    if(build_role_plus_name(gobj, role_plus_name, sizeof(role_plus_name), yuno) < 0) {
        gbuffer_decref(gbuf_sh);
        return -1;  // Error already logged
    }

    char script_name[NAME_MAX];
    char script_path[PATH_MAX];
    int n = snprintf(script_name, sizeof(script_name), "%s.sh", role_plus_name);
    if(n < 0 || (size_t)n >= sizeof(script_name) ||
            strlen(yuno_bin_path) + 1 + (size_t)n >= sizeof(script_path)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The path of the run script of a yuno is too long: not launched",
            "yuno_id",      "%s", yuno_id,
            "bin_path",     "%s", yuno_bin_path,
            "role_plus_name", "%s", role_plus_name,
            NULL
        );
        gbuffer_decref(gbuf_sh);
        return -1;
    }
    if(!build_path(script_path, sizeof(script_path), yuno_bin_path, script_name, NULL)) {
        gbuffer_decref(gbuf_sh);
        return -1;  // Error already logged
    }

    char exec_cmd[PATH_MAX + 16];
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
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot run the yuno",
            "realm_id",     "%s", realm_id,
            "yuno_id",      "%s", yuno_id,
            "yuno_role",    "%s", yuno_role,
            "yuno_name",    "%s", yuno_name?yuno_name:"",
            "yuno_tag",     "%s", yuno_name?yuno_tag:"",
            "yuno_release", "%s", yuno_release?yuno_release:"",
            "ret",          "%d", ret,
            NULL
        );
    } else {
        /*
         *  Launched: mark it until its EV_ON_OPEN clears the mark, so nobody
         *  launches it again while it is still coming up. See is_launching().
         */
        json_object_set_new(
            priv->launching_yunos,
            yuno_id,
            json_integer((json_int_t)start_msectimer(priv->timeout_expiration))
        );
        note_yuno_launch(gobj, priv->resource, yuno_id, yuno_bin_path, role_plus_name);
    }

    int fd = newfile(script_path, yuneta_xpermission(), TRUE);
    if(fd<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "newfile() FAILED",
            "path",         "%s", script_path,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    } else {
        ssize_t x = write(fd, bfbinary, strlen(bfbinary));
        x = write(fd, " --config-file='", strlen(" --config-file='"));
        if(x) {} // avoid warning
        x = write(fd, bfarg, strlen(bfarg));
        if(x) {} // avoid warning
        x = write(fd, "' $1\n", strlen("' $1\n"));
        if(x) {} // avoid warning
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
    pid_t pid = (pid_t)kw_get_int(gobj, yuno, "yuno_pid", 0, KW_REQUIRED);
    pid_t watcher_pid = (pid_t)kw_get_int(gobj, yuno, "watcher_pid", 0, KW_REQUIRED);
    if(!pid) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
                "msgset",       "%s", MSGSET_PARAMETER,
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
            ret = -1;
        }
    }

    if(signal2kill == SIGKILL && ret == 0) {
        // Kill the watcher
        if(watcher_pid) {
            if(kill(watcher_pid, signal2kill)<0) {
                int last_errno = errno;
                if(last_errno != ESRCH) { // No such process
                    // Not a failure of the kill: a watcher whose child ends by SIGKILL leaves by itself
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER,
                        "msg",          "%s", "Cannot kill the watcher of the yuno: the yuno got its SIGKILL",
                        "yuno_id",      "%s", yuno_id,
                        "watcher_pid",  "%d", (int)watcher_pid,
                        "yuno_role",    "%s", yuno_role,
                        "yuno_name",    "%s", yuno_name?yuno_name:"",
                        "yuno_release", "%s", yuno_release?yuno_release:"",
                        "error",        "%d", last_errno,
                        "strerror",     "%s", strerror(last_errno),
                        NULL
                    );
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

    json_t *iev = iev_create(
        gobj,
        EV_PLAY_YUNO,
        kw    // owned
    );

    return gobj_send_event(
        channel_gobj,
        EV_SEND_IEV,
        iev,
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

    json_t *iev = iev_create(
        gobj,
        EV_PAUSE_YUNO,
        kw    // owned
    );

    return gobj_send_event(
        channel_gobj,
        EV_SEND_IEV,
        iev,
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
 *  After a command, stats or authzs was sent to the yunos that matched: a
 *  yuno that runs but whose row has no channel yet (starting) or any more
 *  (going) got nothing, and nothing will answer for it. When none got it,
 *  the requester is answered NOW -- a control center running a scenario
 *  otherwise waited its step's whole deadline, holding every other run --
 *  and when only some did, it is logged.
 *  Returns the response (kw owned by it), or NULL: the answers are
 *  asynchronous and kw stays the caller's.
 ***************************************************************************/
PRIVATE json_t *unreached_yunos_response(
    hgobj gobj,
    const char *what,
    json_t *jn_unreached,   // owned
    int sent,
    json_t *kw              // owned only when a response is returned
)
{
    json_t *jn_response = NULL;
    if(json_array_size(jn_unreached) > 0) {
        char ids[256] = "";
        size_t len = 0;
        size_t i; json_t *jn_id;
        json_array_foreach(jn_unreached, i, jn_id) {
            int n = snprintf(ids + len, sizeof(ids) - len, "%s%s",
                i? "," : "", json_string_value(jn_id));
            if(n < 0 || (size_t)n >= sizeof(ids) - len) {
                len = sizeof(ids) - 1;  // the log below carries the whole list
                break;
            }
            len += (size_t)n;
        }
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "yuno running but not connected to the agent, nothing sent to it",
            "what",         "%s", what? what : "",
            "yunos",        "%j", jn_unreached,
            "sent",         "%d", sent,
            NULL
        );
        if(sent == 0) {
            jn_response = msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: %s: yuno %s runs but is not connected to the agent "
                    "(starting or stopping): nothing sent",
                    gobj_yuno_role_plus_name(), what? what : "", ids),
                0,
                0,
                kw  // owned
            );
        }
    }
    JSON_DECREF(jn_unreached)
    return jn_response;
}

/***************************************************************************
 *  -1: the yuno has no channel (the caller says so, see
 *  unreached_yunos_response).
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
    /*  Default `service` to the yuno_role of the matched yuno.  The
     *  convention is `gobj_create_default_service(yuno_role, GCLASS,
     *  ...)`, so service name == yuno role; the operator then gets
     *  the real SDF_RSTATS counters of the citizen gclass without
     *  having to spell out the service.  To explicitly query the
     *  top C_YUNO attributes (the previous default), pass
     *  `service=__yuno__`.  Yunos that don't register a service
     *  named like their role will surface a "Service not found"
     *  reply — also the right signal to learn the actual service. */
    if(empty_string(kw_get_str(gobj, kw, "service", "", 0))) {
        const char *yuno_role = kw_get_str(gobj, yuno, "yuno_role", "", 0);
        if(!empty_string(yuno_role)) {
            json_object_set_new(kw, "service", json_string(yuno_role));
        }
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
    hgobj gobj,
    json_t *yuno,
    json_t* kw,
    hgobj src
)
{
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
 *  Promote the highest yuno_release per id so the reload makes it primary.
 *
 *  The treedb primary for a yuno-id is the highest-ROWID record, not the
 *  highest yuno_release: lifecycle writes (kill/run/snap) append records for
 *  whatever release is active, so an older release can sit on top and a
 *  pending binary upgrade never takes effect. That is why the agent's own
 *  lifecycle writes (run, kill, play, pause) are volatil (the "force
 *  volatil" of the original code); a write that is saved still appends.
 *
 *  An append does NOT move the in-memory primary index — only a reload
 *  rebuilds it. So this MUST run BEFORE the restart_nodes() reload: for each
 *  id whose highest non-disabled yuno_release is newer than the current
 *  primary, re-append that release (gobj_update_node) so it becomes the
 *  highest rowid; the imminent reload then makes it the primary and
 *  run_enabled_yunos() launches the newest version.
 ***************************************************************************/
PRIVATE int promote_highest_release_yunos(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    json_t *primaries = gobj_list_nodes(
        priv->resource,
        resource,
        0, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    int idx; json_t *primary;
    json_array_foreach(primaries, idx, primary) {
        const char *id = kw_get_str(gobj, primary, "id", 0, 0);
        const char *primary_release = kw_get_str(gobj, primary, "yuno_release", "", 0);
        if(empty_string(id)) {
            continue;
        }

        json_t *instances = gobj_list_instances(
            priv->resource,
            resource,
            "",
            json_pack("{s:s}", "id", id),
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            gobj
        );

        const char *best_release = primary_release;
        int jx; json_t *inst;
        json_array_foreach(instances, jx, inst) {
            if(kw_get_bool(gobj, inst, "yuno_disabled", 0, 0)) {
                continue;
            }
            const char *rel = kw_get_str(gobj, inst, "yuno_release", "", 0);
            if(empty_string(rel)) {
                continue;
            }
            if(version_cmp(rel, best_release) > 0) {
                best_release = rel;
            }
        }

        /*
         *  best_release starts AT primary_release and only ever moves up,
         *  so this cannot fire -- which is exactly why it is worth saying
         *  if it ever does. The eleven days this guard commemorates were a
         *  promotion running backwards while every line in the log read
         *  like routine progress.
         */
        if(version_cmp(best_release, primary_release) < 0) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_INTERNAL,
                "msg",      "%s", "promotion would go BACKWARDS, refused",
                "id",       "%s", id,
                "from",     "%s", primary_release,
                "to",       "%s", best_release,
                NULL
            );
            JSON_DECREF(instances)
            continue;
        }

        if(version_cmp(best_release, primary_release) > 0) {
            gobj_log_info(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_STARTUP,
                "msg",      "%s", "promote highest yuno_release to primary",
                "id",       "%s", id,
                "from",     "%s", primary_release,
                "to",       "%s", best_release,
                NULL
            );
            json_decref(
                gobj_update_node(   // re-append → highest rowid for the reload
                    priv->resource,
                    resource,
                    json_pack("{s:s, s:s}", "id", id, "yuno_release", best_release),
                    0, // options
                    gobj
                )
            );
        }
        JSON_DECREF(instances);
    }
    JSON_DECREF(primaries);
    return 0;
}

/***************************************************************************
 *  Try to run the activated yunos.
 *  This function is called once by timer at startup
 ***************************************************************************/
PRIVATE int run_enabled_yunos(
    hgobj gobj,
    BOOL spare_the_living   // FALSE only after restart_nodes() killed them all, and all are gone
)
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

    /*
     *  Launch in ascending start_priority order so a node bounce (restart_nodes
     *  / deactivate-snap) brings utilities up before gates and dba.
     */
    sort_yunos_by_start_priority(gobj, iter_yunos, TRUE);

    int idx; json_t *yuno;
    json_array_foreach(iter_yunos, idx, yuno) {
        if(priv->restart_sparing && priv->restart_wait &&
                json_object_get(priv->restart_wait, SDATA_GET_ID(yuno))) {
            continue;   // spared by the restart: its window launches it, once it is gone
        }
        if(priv->restart_held && json_object_get(priv->restart_held, SDATA_GET_ID(yuno))) {
            continue;   // stopped or disabled by the operator while the restart waited
        }
        launch_enabled_yuno(gobj, yuno, spare_the_living);
    }
    JSON_DECREF(iter_yunos);

    return 0;
}

/***************************************************************************
 *  Launch a yuno if it is enabled, not running and not being launched;
 *  with `spare_the_living`, not if a process of it lives unregistered
 ***************************************************************************/
PRIVATE void launch_enabled_yuno(hgobj gobj, json_t *yuno, BOOL spare_the_living)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL disabled = kw_get_bool(gobj, yuno, "yuno_disabled", 0, KW_REQUIRED);
    if(disabled) {
        return;
    }
    BOOL running = kw_get_bool(gobj, yuno, "yuno_running", 0, KW_REQUIRED);
    if(running || is_launching(gobj, kw_get_str(gobj, yuno, "id", "", 0))) {
        return;
    }
    if(spare_the_living && yuno_lives_unregistered(gobj, yuno)) {
        return;   // Warning already logged
    }
    run_yuno(gobj, yuno, 0);
    // volatil: memory only -- saved, a lifecycle write would put this release over a newer one
    json_decref(
        gobj_update_node(
            priv->resource,
            "yunos",
            json_incref(yuno),
            json_pack("{s:b}", "volatil", 1),
            gobj
        )
    );
}

/***************************************************************************
 *  Try to run the util yunos.
 *  This function is called once by timer at startup
 ***************************************************************************/
PRIVATE int run_util_yunos(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char *resource = "yunos";

    gobj_log_debug(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_STARTUP,
        "msg",          "%s", "run_util_yunos",
        NULL
    );

    /*
     *  Esta bien así, no le paso nada, que devuelva all yunos de all reinos.
     */
    json_t *iter_yunos = gobj_list_nodes(
        priv->resource,
        resource,
        json_pack("{s:s}", "yuno_tag", "util"),
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
            if(!running && !is_launching(gobj, kw_get_str(gobj, yuno, "id", "", 0))) {
                if(yuno_lives_unregistered(gobj, yuno)) {
                    continue;   // Warning already logged
                }
                run_yuno(gobj, yuno, 0);
                // volatil: memory only -- saved, a lifecycle write would put this release over a newer one
                json_decref(
                    gobj_update_node(
                        priv->resource,
                        resource,
                        json_incref(yuno),
                        json_pack("{s:b}", "volatil", 1),
                        gobj
                    )
                );

            }
        }
    }
    JSON_DECREF(iter_yunos);

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
        char date[90];  // current_timestamp() wants 90 bytes
        current_timestamp(date, sizeof(date));
        json_t *jn_records = json_array();

        /*
         *  The command word is told as the parser tells it: by the command
         *  table of the agent (any case, quotes, aliases such as EV_WRITE_TTY)
         */
        const sdata_desc_t *command_table = gobj_command_desc(gobj, NULL, FALSE);

        /*
         *  A console write (one per keystroke) keeps only the fact, one
         *  record per burst: see audit_tty_command(). Any other command,
         *  its record: see audit_record_build().
         */
        audit_command_records(  // on -1 error already logged
            priv->audit_tty_bursts,
            command,
            kw,             // not owned
            date,
            AUDIT_TTY_BURST_SECONDS,
            command_table,
            jn_records
        );
        write_audit_records(gobj, jn_records);
        JSON_DECREF(jn_records)
    }
    return 0;
}

/***************************************************************************
 *  Write the records to the audit file, and flush them: a record reaches
 *  the file (one write()) before the command runs, so a crash of the
 *  agent loses no record, not even the first write of a console burst
 *  (who typed). Up to now they waited in the buffer of the file for the
 *  flush of C_YUNO (every 2 seconds). A command costs one write(), about
 *  0.7 us more per record (perf_rotatory, audit_record_flush).
 ***************************************************************************/
PRIVATE void write_audit_records(hgobj gobj, json_t *jn_records)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(json_array_size(jn_records) == 0) {
        return;
    }

    size_t idx;
    json_t *jn_record;
    json_array_foreach(jn_records, idx, jn_record) {
        char *audit = json2uglystr(jn_record);
        if(!audit) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Cannot serialize an audit record",
                NULL
            );
            continue;
        }
        rotatory_write(priv->audit_file, LOG_AUDIT, audit, strlen(audit));
        rotatory_write(priv->audit_file, LOG_AUDIT, "\n", 1);  // double new line: the separator field
        gbmem_free(audit);
    }
    rotatory_flush(priv->audit_file);
}

/***************************************************************************
 *  A new audit file has begun (new day, or the size limit was reached):
 *  the moment to apply the retention. This runs inside the
 *  rotatory_write() of the first record of the new file, before that
 *  record is written: once a day, or at a size rotation.
 ***************************************************************************/
PRIVATE int audit_newfile_cb(void *user_data, const char *old_filename, const char *new_filename)
{
    hgobj gobj = user_data;
    remove_old_audit_files(gobj);
    return 0;
}

/***************************************************************************
 *  Retention of the audit directory: remove the audit files older than
 *  audit_keep_days. Only the files named by the audit mask are candidates
 *  (see rotatory_remove_old_files()), never a link or anything else.
 ***************************************************************************/
PRIVATE int remove_old_audit_files(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t keep_days = gobj_read_integer_attr(gobj, "audit_keep_days");
    if(keep_days == 0) {
        return 0;
    }
    if(keep_days < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "audit_keep_days negative, no audit file removed",
            "audit_keep_days", "%d", (int)keep_days,
            NULL
        );
        return -1;
    }

    json_t *jn_removed = json_array();
    uint64_t removed_bytes = 0;
    int removed = rotatory_remove_old_files(
        priv->audit_file,
        (unsigned)keep_days,
        jn_removed,
        &removed_bytes
    );
    if(removed > 0) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Old audit files removed",
            "audit_keep_days", "%d", (int)keep_days,
            "removed",      "%d", removed,
            "megas",        "%lu", (unsigned long)(removed_bytes/(1024*1024)),
            "current_file", "%s", rotatory_path(priv->audit_file),
            "files",        "%j", jn_removed,
            NULL
        );
    }
    JSON_DECREF(jn_removed);

    return removed; // on -1 error already logged
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int build_release_name(
    hgobj gobj, char *bf, int bfsize, json_t *hs_binary, json_t *hs_config
) {
    return build_yuno_release_name(
        gobj,
        bf,
        (size_t)bfsize,
        SDATA_GET_STR(hs_binary, "version"),
        SDATA_GET_STR(hs_config, "version")
    );
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
PRIVATE unsigned get_new_service_port(hgobj gobj)
{
    json_t *jn_range_ports = gobj_read_json_attr(gobj, "range_ports");
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
                "msgset",       "%s", MSGSET_INTERNAL,
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
                "msgset",       "%s", MSGSET_SERVICE,
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
 *  The public services of `yuno`, from its binary: created with a new port,
 *  or updated (description, schema, connector) keeping theirs. The write is
 *  saved like any other, so a snap shot afterwards holds it, and an
 *  activate-snap of an earlier snap brings back the earlier connector.
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

            unsigned port = 0;

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
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SERVICE,
                        "msg",          "%s", "Cannot create service",
                        "yuno_role",    "%s", yuno_role,
                        "yuno_name",    "%s", yuno_name,
                        "service",      "%s", service,
                        NULL
                    );
                    ret += -1;
                    continue;
                }
                port = get_new_service_port(gobj);
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
 *  The pids of a yuno just killed, each with its start time, read after
 *  the kill: a pid alive later with another one is another process (a pid
 *  reused), and the yuno is gone. One gone already is not kept.
 ***************************************************************************/
PRIVATE void note_killed(hgobj gobj, json_t *jn_killed, const char *yuno_id, json_t *jn_pids)
{
    if(json_array_size(jn_pids) == 0) {
        return;
    }
    json_t *jn_entries = json_array();
    int idx2; json_t *jn_pid;
    json_array_foreach(jn_pids, idx2, jn_pid) {
        pid_t pid = (pid_t)json_integer_value(jn_pid);
        uint64_t start_time = 0;
        char state;
        int r = read_process_stat(gobj, pid, &state, &start_time);
        if(r == -1) {
            continue;   // gone already: nothing to wait for
        }
        if(r < 0) {
            start_time = 0; // Error already logged: judged by its existence alone
        }
        json_array_append_new(jn_entries, json_pack("[I, I]",
            (json_int_t)pid, (json_int_t)start_time
        ));
    }
    json_t *jn_prev = json_object_get(jn_killed, yuno_id);
    if(json_array_size(jn_entries) == 0) {
        // all gone already
    } else if(jn_prev) {
        json_array_extend(jn_prev, jn_entries);   // another release of the same id
    } else {
        json_object_set(jn_killed, yuno_id, jn_entries);
    }
    JSON_DECREF(jn_entries)
}

/***************************************************************************
 *  SIGKILL a yuno that runs, from its record (`yuno`, not owned: the row
 *  of before a reload of the treedb does), and note its pids
 ***************************************************************************/
PRIVATE void kill_running_yuno(hgobj gobj, json_t *yuno, json_t *jn_killed)
{
    json_t *jn_pids = json_array();
    pid_t pids[2] = {
        (pid_t)kw_get_int(gobj, yuno, "yuno_pid", 0, KW_REQUIRED),
        (pid_t)kw_get_int(gobj, yuno, "watcher_pid", 0, 0)
    };
    for(int i = 0; i < 2; i++) {
        if(pids[i] > 0) {
            json_array_append_new(jn_pids, json_integer(pids[i]));
        }
    }
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
    if(channel_gobj) {
        gobj_write_user_data( // HACK release yuno info connection
            channel_gobj,
            "__yuno__",
            json_string("")
        );
    }
    kill_yuno(gobj, yuno);
    note_killed(gobj, jn_killed, SDATA_GET_ID(yuno), jn_pids);
    JSON_DECREF(jn_pids)
}

/***************************************************************************
 *  Restart the node: activate-snap and deactivate-snap, and the promotion
 *  of the newest release of each yuno (yunetas upgrade-yunos).
 *
 *  The agent's treedb is reloaded, and a reload loses what the agent knows
 *  of a running yuno (in memory only: running, its pids, its channel). With
 *  `all` every yuno is SIGKILLed and the enabled ones launched again, as up
 *  to 7.28.10. Without it, only the yunos whose launch changed go: each
 *  running yuno's launch signature (restart_plan.c: its row, binary,
 *  configuration, realm, public services) as run_yuno() wrote it at its
 *  launch, in its bin/ (note_yuno_launch()), is compared with the one
 *  after the reload; the same one keeps its runtime and stays alive,
 *  another one is killed and launched again (or not, if it is gone or
 *  disabled). A running yuno with no launch noted (launched by an agent up
 *  to 7.29.1) is launched again: what it runs is not known. A yuno that does not run is launched afterwards, if
 *  enabled, as before. Up to 7.29.1 the signature of before was taken from
 *  the treedb when the restart began, where a change that waits for the
 *  next launch (update-config of the version in use) already was: the yuno
 *  was kept on what it ran.
 ***************************************************************************/
PRIVATE int restart_nodes(hgobj gobj, BOOL all)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    json_t *iter = gobj_list_nodes(
        priv->resource,
        "yunos",
        0, // filter
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    // Force kill
    int prev_signal2kill = (int)gobj_read_integer_attr(gobj, "signal2kill");
    gobj_write_integer_attr(gobj, "signal2kill", SIGKILL);

    json_t *jn_killed = json_object();  // {yuno_id: [[pid, start_time]]}
    json_t *jn_before = json_object();  // {yuno_id: {signature, runtime, record}}, decided after the reload
    int idx; json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        /*
         *  Kill yuno
         */
        json_t *jn_pids = json_array();
        BOOL running = kw_get_bool(gobj, yuno, "yuno_running", 0, KW_REQUIRED);
        if(running && !all) {
            json_t *jn_b = json_pack("{s:o, s:O}",
                "runtime", yuno_runtime_fields(gobj, priv->resource, yuno),
                "record", yuno
            );
            char bin_path[PATH_MAX];
            char role_plus_name[NAME_MAX];
            if(build_role_plus_name(gobj, role_plus_name, sizeof(role_plus_name), yuno) == 0 &&
                    build_yuno_bin_path(gobj, yuno, bin_path, sizeof(bin_path), FALSE)) {
                // Without a signature the yuno counts as changed: restarted
                json_t *jn_launched = yuno_launched_signature(gobj, bin_path, role_plus_name);
                if(jn_launched) {
                    json_object_set_new(jn_b, "signature", jn_launched);
                }
            }
            json_object_set_new(jn_before, SDATA_GET_ID(yuno), jn_b);
        } else if(running) {
            pid_t pids[2] = {
                (pid_t)kw_get_int(gobj, yuno, "yuno_pid", 0, KW_REQUIRED),
                (pid_t)kw_get_int(gobj, yuno, "watcher_pid", 0, 0)
            };
            for(int i = 0; i < 2; i++) {
                if(pids[i] > 0) {
                    json_array_append_new(jn_pids, json_integer(pids[i]));
                }
            }
            hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, yuno, "_channel_gobj", 0, KW_REQUIRED);
            if(channel_gobj) {
                gobj_write_user_data( // HACK release yuno info connection
                    channel_gobj,
                    "__yuno__",
                    json_string("")
                );
            }
            kill_yuno(gobj, yuno);
        } else {
            kill_yuno_unregistered(gobj, yuno, jn_pids);
        }
        note_killed(gobj, jn_killed, SDATA_GET_ID(yuno), jn_pids);
        JSON_DECREF(jn_pids)
    }
    JSON_DECREF(iter)

    /*----------------------------------------*
     *  Promote the highest yuno_release per id BEFORE the reload, so the
     *  rebuilt primary index is the newest version (not just the highest
     *  rowid left by lifecycle writes). Must precede the gobj_stop/start.
     *----------------------------------------*/
    promote_highest_release_yunos(gobj);

    /*----------------------------*
     *  Restart treedb
     *----------------------------*/
    gobj_stop(priv->resource);
    gobj_start(priv->resource);

    /*----------------------------------------*
     *  The running yunos, after the reload:
     *  the same launch keeps its runtime and
     *  lives on, another one is killed
     *----------------------------------------*/
    if(!all) {
        json_t *jn_kept = json_array();
        json_t *jn_restarted = json_array();
        json_t *jn_stopped = json_array();
        json_t *jn_unknown = json_array();  // restarted: not launched by this agent
        const char *before_id; json_t *jn_b;
        json_object_foreach(jn_before, before_id, jn_b) {
            json_t *yuno_after = gobj_get_node(
                priv->resource,
                "yunos",
                json_pack("{s:s}", "id", before_id),
                json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
                gobj
            );
            json_t *jn_signature_after = yuno_after?
                yuno_launch_signature(gobj, priv->resource, yuno_after) : NULL;
            const char *decision = restart_decision(
                json_object_get(jn_b, "signature"),
                jn_signature_after
            );
            JSON_DECREF(jn_signature_after)

            if(strcmp(decision, "keep")==0) {
                json_object_update(yuno_after, json_object_get(jn_b, "runtime"));
                json_decref(gobj_update_node(
                    priv->resource,
                    "yunos",
                    yuno_after,     // owned
                    json_pack("{s:b}", "volatil", 1),
                    gobj
                ));
                yuno_after = NULL;
                json_array_append_new(jn_kept, json_string(before_id));
            } else {
                kill_running_yuno(gobj, json_object_get(jn_b, "record"), jn_killed);
                json_array_append_new(
                    strcmp(decision, "stop")==0? jn_stopped : jn_restarted,
                    json_string(before_id)
                );
                if(strcmp(decision, "restart")==0 && !json_object_get(jn_b, "signature")) {
                    json_array_append_new(jn_unknown, json_string(before_id));
                }
            }
            JSON_DECREF(yuno_after)
        }

        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Node restart: only the yunos whose launch changed",
            "kept",         "%j", jn_kept,
            "restarted",    "%j", jn_restarted,
            "stopped",      "%j", jn_stopped,
            "launch_not_known", "%j", jn_unknown,
            NULL
        );
        JSON_DECREF(jn_kept)
        JSON_DECREF(jn_restarted)
        JSON_DECREF(jn_stopped)
        JSON_DECREF(jn_unknown)
    }
    JSON_DECREF(jn_before)

    // Restore kill
    gobj_write_integer_attr(gobj, "signal2kill", prev_signal2kill);

    /*
     *  The relaunch waits for the yunos killed above to be gone: a SIGKILL
     *  is delivered, not done, and a process in an uninterruptible wait (a
     *  disk) lives on a while. Run at once, a new instance met the old
     *  one's exclusive resources, or the launch found it still alive and
     *  did not launch it (up to 7.25.21). Looked at every 100 ms, for 10 s
     *  at most: then the relaunch goes on, and says who was left.
     */
    if(!priv->restart_wait) {
        priv->restart_wait = json_object();
    }
    const char *yuno_id; json_t *jn_pids;
    json_object_foreach(jn_killed, yuno_id, jn_pids) {
        json_t *jn_prev = json_object_get(priv->restart_wait, yuno_id);
        if(jn_prev) {
            json_array_extend(jn_prev, jn_pids);    // a restart while the previous one waits
        } else {
            json_object_set(priv->restart_wait, yuno_id, jn_pids);
        }
    }
    JSON_DECREF(jn_killed)
    priv->restart_wait_until = start_msectimer(10*1000);
    priv->restart_sparing = FALSE;
    if(priv->restart_held) {
        json_object_clear(priv->restart_held);
    }
    clear_timeout(priv->restart_wait_timer);    // a previous restart's, maybe at its slower pace
    restart_wait_tick(gobj);    // none to wait for: the relaunch at once

    return ret;
}

/***************************************************************************
 *  The state of a process and its start time (clock ticks since boot),
 *  from /proc/<pid>/stat. 0 read; -1 the process does not exist (ENOENT,
 *  ESRCH); -2 it cannot be told (another errno -- EMFILE, a hidepid mount
 *  -- or a content that does not parse), logged at the transition only:
 *  the restart asks every 100 ms.
 ***************************************************************************/
PRIVATE BOOL proc_stat_failing_said = FALSE;

PRIVATE int read_process_stat(hgobj gobj, pid_t pid, char *state, uint64_t *start_time)
{
    char path[PATH_MAX];
    char bf[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    int fd = open(path, O_RDONLY|O_CLOEXEC);
    ssize_t n = -1;
    int err = 0;
    if(fd >= 0) {
        n = read(fd, bf, sizeof(bf) - 1);
        err = errno;
        close(fd);
    } else {
        err = errno;
    }
    if(fd < 0 || n < 0) {
        if(err == ENOENT || err == ESRCH) {
            return -1;
        }
        if(!proc_stat_failing_said) {
            proc_stat_failing_said = TRUE;
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot read the stat of a process: taken as alive (said once, until a read works)",
                "path",         "%s", path,
                "errno",        "%d", err,
                "serrno",       "%s", strerror(err),
                NULL
            );
        }
        return -2;
    }
    bf[n] = 0;
    /*
     *  After the command, which may hold spaces and ')': its state (field
     *  3) and, 19 fields later, its start time (field 22)
     */
    char *p = strrchr(bf, ')');
    if(p) {
        char *save = NULL;
        char *tok = strtok_r(p + 1, " ", &save);
        for(int field = 3; tok; field++, tok = strtok_r(NULL, " ", &save)) {
            if(field == 3) {
                *state = tok[0];
            } else if(field == 22) {
                *start_time = strtoull(tok, NULL, 10);
                proc_stat_failing_said = FALSE;
                return 0;
            }
        }
    }
    if(!proc_stat_failing_said) {
        proc_stat_failing_said = TRUE;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "The stat of a process does not parse: taken as alive (said once, until a read works)",
            "path",         "%s", path,
            NULL
        );
    }
    return -2;
}

/***************************************************************************
 *  A pid the restart killed is gone when it does not exist, is a zombie
 *  (dead, only waiting for its parent to reap it), or runs with another
 *  start time than the one recorded: another process that took the pid.
 *  Not gone: a task in D state, whatever its cmdline says (it loses it at
 *  exit_mm(), while it still holds its files); nor one whose stat cannot be
 *  read -- taken for gone in the first wait, it emptied the map, and the
 *  relaunch without sparing gave it a second instance. Up to cb3d6c70a a
 *  reused pid was told apart by the scan alone at the window's end, which
 *  took that task for gone too.
 ***************************************************************************/
PRIVATE BOOL process_is_gone(hgobj gobj, pid_t pid, uint64_t start_time)
{
    char state = 0;
    uint64_t now_start = 0;
    int r = read_process_stat(gobj, pid, &state, &now_start);
    if(r == -1) {
        return TRUE;
    }
    if(r < 0) {
        return FALSE;   // cannot be told (Error already logged): alive
    }
    if(state == 'Z') {
        return TRUE;
    }
    if(start_time && now_start != start_time) {
        return TRUE;    // another process with the pid
    }
    return FALSE;
}

/***************************************************************************
 *  restart_nodes(): the relaunch, once every yuno it killed is gone, or
 *  when the wait is over.
 *  Those still alive after the wait (a SIGKILL waits for the end of an
 *  uninterruptible wait, a disk) are not launched beside themselves: they
 *  are SPARED, and each one is launched when its processes are gone,
 *  looked at every second for RESTART_SPARE_MS (restart_spare_tick()). Up
 *  to 7.25.22 such a yuno was relaunched at 10 s anyway, beside itself; a
 *  first form of the sparing left it down until a run-yuno. A deliberate
 *  poll: the death of a process that is not our child gives no event here.
 ***************************************************************************/
#define RESTART_SPARE_MS    (5*60*1000)

PRIVATE void restart_wait_tick(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->restart_sparing) {
        restart_spare_tick(gobj);
        return;
    }

    const char *yuno_id; json_t *jn_pids; void *n;
    json_object_foreach_safe(priv->restart_wait, n, yuno_id, jn_pids) {
        for(int idx = (int)json_array_size(jn_pids) - 1; idx >= 0; idx--) {
            json_t *jn_entry = json_array_get(jn_pids, (size_t)idx);
            pid_t pid = (pid_t)json_integer_value(json_array_get(jn_entry, 0));
            uint64_t start_time = (uint64_t)json_integer_value(json_array_get(jn_entry, 1));
            if(process_is_gone(gobj, pid, start_time)) {
                json_array_remove(jn_pids, (size_t)idx);
            }
        }
        if(json_array_size(jn_pids) == 0) {
            json_object_del(priv->restart_wait, yuno_id);
        }
    }

    if(json_object_size(priv->restart_wait) == 0) {
        clear_timeout(priv->restart_wait_timer);
        run_enabled_yunos(gobj, FALSE);    // all gone: none is left alive to spare (the held are not run)
        if(priv->restart_held) {
            json_object_clear(priv->restart_held);
        }
        return;
    }

    if(!test_msectimer(priv->restart_wait_until)) {
        if(!gobj_is_running(priv->restart_wait_timer)) {
            set_timeout_periodic(priv->restart_wait_timer, 100);
        }
        return;
    }

    /*
     *  The ones the operator stopped, disabled or launched while the
     *  restart waited are not for the window: taken out first, so the
     *  warning names only the spared, and no window opens for none
     */
    const char *held_id; json_t *jn_v;
    json_object_foreach(priv->restart_held, held_id, jn_v) {
        json_object_del(priv->restart_wait, held_id);
    }
    if(json_object_size(priv->restart_wait) == 0) {
        clear_timeout(priv->restart_wait_timer);
        run_enabled_yunos(gobj, TRUE);    // the held are not run
        if(priv->restart_held) {
            json_object_clear(priv->restart_held);
        }
        return;
    }

    /*
     *  Not launched beside the one still alive (run_enabled_yunos() skips
     *  the spared ids): up to 7.25.22 they were relaunched anyway, and a
     *  yuno stuck in a disk wait got a second instance
     */
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_OPERATIONAL,
        "msg",          "%s", "yunos killed for the restart still alive after 10 s: each one launched when it is gone (looked at every second, for 5 minutes)",
        "yunos",        "%j", priv->restart_wait,
        NULL
    );
    priv->restart_sparing = TRUE;
    priv->restart_wait_until = start_msectimer(RESTART_SPARE_MS);
    clear_timeout(priv->restart_wait_timer);
    set_timeout_periodic(priv->restart_wait_timer, 1000);
    run_enabled_yunos(gobj, TRUE);
    if(priv->restart_held) {
        json_object_clear(priv->restart_held);
    }
}

/***************************************************************************
 *  The yunos spared by the restart: ONLY those, each one launched when it
 *  is gone. One the operator stops, disables or launches meanwhile leaves
 *  the window (forget_spared_yuno()): a first form of this relaunched a
 *  spared yuno the operator had stopped with kill-yuno. And the window
 *  launches nothing else: an earlier one ran run_enabled_yunos() at each
 *  death, which launched every enabled yuno not running.
 ***************************************************************************/
PRIVATE void restart_spare_tick(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *key; json_t *jn_pids; void *n;
    json_object_foreach_safe(priv->restart_wait, n, key, jn_pids) {
        char yuno_id[NAME_MAX];
        snprintf(yuno_id, sizeof(yuno_id), "%s", key);  // the launch takes the key out
        if(launch_spared_yuno(gobj, yuno_id, jn_pids)) {
            forget_spared_yuno(gobj, yuno_id);
        }
    }

    if(json_object_size(priv->restart_wait) == 0) {
        priv->restart_sparing = FALSE;
        clear_timeout(priv->restart_wait_timer);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL,
            "msg",          "%s", "yunos spared by the restart: none left to wait for",
            NULL
        );
        return;
    }

    if(test_msectimer(priv->restart_wait_until)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL,
            "msg",          "%s", "yunos killed for the restart still alive after 5 minutes: not launched, run-yuno once they are gone",
            "yunos",        "%j", priv->restart_wait,
            NULL
        );
        json_object_clear(priv->restart_wait);
        priv->restart_sparing = FALSE;
        clear_timeout(priv->restart_wait_timer);
    }
}

/***************************************************************************
 *  A spared yuno: TRUE when it is gone (and then launched, if it is enabled
 *  and not running), FALSE while a process of it lives, or when /proc
 *  cannot tell (logged).
 *  Gone is both: every pid the restart killed is gone (process_is_gone(),
 *  /proc/<pid>/stat: a task in D state is alive; a pid reused by another
 *  process reads gone by its start time), AND no process runs the yuno
 *  (find_living_yuno_pids(), its role and configuration). The scan alone
 *  is fooled by a task that lost its cmdline (exit_mm()) and still holds
 *  its files -- a SIGKILLed yuno stuck while closing, the one the window is
 *  for: a first form of this launched a second instance beside it. The
 *  scan, a read of every cmdline, runs only once the pids are gone.
 ***************************************************************************/
PRIVATE BOOL launch_spared_yuno(hgobj gobj, const char *yuno_id, json_t *jn_pids)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int idx; json_t *jn_entry;
    json_array_foreach(jn_pids, idx, jn_entry) {
        pid_t pid = (pid_t)json_integer_value(json_array_get(jn_entry, 0));
        uint64_t start_time = (uint64_t)json_integer_value(json_array_get(jn_entry, 1));
        if(!process_is_gone(gobj, pid, start_time)) {
            return FALSE;
        }
    }

    json_t *iter = gobj_list_nodes(
        priv->resource,
        "yunos",
        json_pack("{s:s}", "id", yuno_id),
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );

    BOOL gone = TRUE;
    json_t *yuno;
    json_array_foreach(iter, idx, yuno) {
        json_t *jn_living = find_living_yuno_pids(gobj, yuno);
        if(!jn_living || json_array_size(jn_living) > 0) {
            gone = FALSE;   // alive, or /proc cannot tell (Error already logged)
        }
        JSON_DECREF(jn_living)
    }
    if(gone) {
        json_array_foreach(iter, idx, yuno) {
            launch_enabled_yuno(gobj, yuno, FALSE);   // just seen gone: none to spare
        }
    }
    JSON_DECREF(iter)
    return gone;
}

/***************************************************************************
 *  A yuno leaves the restart's window: it is gone (launched or not), or the
 *  operator stopped, disabled or launched it. The window closes at its
 *  next tick when none is left
 ***************************************************************************/
PRIVATE void forget_spared_yuno(hgobj gobj, const char *yuno_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Only in the window: in the first wait the map holds the pids still
     *  to be gone, and an id taken out there emptied it early -- the
     *  relaunch without sparing then gave a living yuno a second instance
     */
    if(priv->restart_sparing && priv->restart_wait && !empty_string(yuno_id)) {
        json_object_del(priv->restart_wait, yuno_id);
    }
}

/***************************************************************************
 *  The operator stopped (kill-yuno) or disabled a yuno while a restart
 *  runs, or it was launched: in the window it leaves it; in the first wait
 *  its pids are still waited for, and it is held apart, not run by the
 *  relaunch nor spared. A launch holds only an id the wait still has
 *  (`only_waited`): the relaunch's own launches come after the wait.
 ***************************************************************************/
PRIVATE void hold_restarted_yuno(hgobj gobj, const char *yuno_id, BOOL only_waited, const char *why)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(empty_string(yuno_id) || !priv->restart_wait) {
        return;
    }
    if(only_waited && !json_object_get(priv->restart_wait, yuno_id)) {
        return;
    }
    if(priv->restart_sparing) {
        forget_spared_yuno(gobj, yuno_id);
        return;
    }
    if(json_object_size(priv->restart_wait) == 0) {
        return;     // no restart waiting
    }
    if(!priv->restart_held) {
        priv->restart_held = json_object();
    }
    /*
     *  Each act is kept: enable-yuno takes back a disable only, not a stop
     *  or a launch of the same yuno
     */
    json_t *jn_why = json_object_get(priv->restart_held, yuno_id);
    if(!jn_why) {
        jn_why = json_object();
        json_object_set_new(priv->restart_held, yuno_id, jn_why);
    }
    json_object_set_new(jn_why, why, json_true());
}

/***************************************************************************
 *  enable-yuno takes back the `why` (a disable) of a held yuno: it is the
 *  restart's again when no other act of the operator holds it
 ***************************************************************************/
PRIVATE void unhold_restarted_yuno(hgobj gobj, const char *yuno_id, const char *why)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->restart_held || empty_string(yuno_id)) {
        return;
    }
    json_t *jn_why = json_object_get(priv->restart_held, yuno_id);
    if(!jn_why) {
        return;
    }
    json_object_del(jn_why, why);
    if(json_object_size(jn_why) == 0) {
        json_object_del(priv->restart_held, yuno_id);
    }
}

/***************************************************************************
 *  Cert auto-sync helpers.
 *
 *  The yuneta user cannot read /etc/letsencrypt/live/ (0700 root), so the
 *  agent delegates the privileged copy to copy-certs.sh via sudo (yuneta
 *  has NOPASSWD:ALL). To detect whether the copy actually changed anything
 *  we snapshot the size+mtime of every .crt file in the store directory
 *  before and after the copy; a diff triggers the reload broadcast.
 ***************************************************************************/

/* Build a {filename: "size:mtime"} snapshot of *.crt files in dir. */
PRIVATE json_t *cert_sync_snapshot_dir(const char *dir)
{
    json_t *snap = json_object();
    DIR *d = opendir(dir);
    if(!d) {
        return snap;
    }
    struct dirent *ent;
    while((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
        if(len < 5) {
            continue;
        }
        /* only .crt files (ignore .chain, .key, and dotfiles) */
        if(strcmp(name + len - 4, ".crt") != 0) {
            continue;
        }
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        struct stat st;
        if(stat(path, &st) != 0) {
            continue;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%lld:%lld",
            (long long)st.st_size, (long long)st.st_mtime);
        json_object_set_new(snap, name, json_string(buf));
    }
    closedir(d);
    return snap;
}

PRIVATE BOOL cert_sync_snapshots_differ(json_t *a, json_t *b)
{
    if(!a || !b) {
        return TRUE;
    }
    if(json_object_size(a) != json_object_size(b)) {
        return TRUE;
    }
    const char *key; json_t *val;
    json_object_foreach(a, key, val) {
        json_t *other = json_object_get(b, key);
        if(!other) {
            return TRUE;
        }
        const char *sa = json_string_value(val);
        const char *sb = json_string_value(other);
        if(!sa || !sb || strcmp(sa, sb) != 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 *  Broadcast the yuno-level 'reload-certs' command to every running yuno
 *  plus the local agent (in case it has TLS listeners of its own).
 *  We piggy-back on cmd_command_yuno() to reuse its iteration + dispatch
 *  for the remote yunos.
 */
PRIVATE int cert_sync_broadcast_reload(hgobj gobj)
{
    /* Remote yunos (the yunos table does not include the agent itself). */
    json_t *kw_remote = json_pack("{s:s, s:s}",
        "command", "reload-certs",
        "service", "__yuno__"
    );
    json_t *resp_remote = cmd_command_yuno(gobj, "command-yuno", kw_remote, gobj);
    JSON_DECREF(resp_remote);
    /* kw_remote was consumed by cmd_command_yuno. */

    /* Agent itself — in case it ever exposes a TLS listener. */
    json_t *resp_local = gobj_command(gobj_yuno(), "reload-certs", json_object(), gobj);
    JSON_DECREF(resp_local);

    return 0;
}

PRIVATE int cert_sync_tick(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->cert_sync_enabled) {
        return 0;
    }

    const char *store_dir = gobj_read_str_attr(gobj, "cert_sync_store_dir");
    const char *copy_cmd  = gobj_read_str_attr(gobj, "cert_sync_copy_cmd");

    gobj_write_integer_attr(gobj, "cert_sync_last_check", (json_int_t)time(NULL));

    /* 1) snapshot store dir BEFORE the copy */
    json_t *before = cert_sync_snapshot_dir(store_dir);

    /* 2) run copy-certs.sh via sudo. A non-zero return means the script
     *    failed entirely (e.g. sudo denied) — we bump failures but still
     *    compare the snapshots, because the previous run may have copied
     *    successfully and the store could be out of sync with the yunos.
     */
    BOOL copy_ok = TRUE;
    if(!empty_string(copy_cmd)) {
        char *const argv[] = {"sh", "-c", (char *)copy_cmd, NULL};
        int rc = run_process2("/bin/sh", argv);    // not system(): see exec_startup_command()
        if(rc != 0) {
            copy_ok = FALSE;
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "cert-sync copy command FAILED",
                "cmd",          "%s", copy_cmd,
                "rc",           "%d", rc,
                "exit_code",    "%d", (rc > 0 && WIFEXITED(rc))? WEXITSTATUS(rc): -1,
                NULL
            );
        }
    }

    /* 3) snapshot AFTER; compare against the stored snapshot from last tick
     *    (or the pre-copy one if it's the first). A change in either means
     *    new cert material, so we reload every yuno.
     */
    json_t *after = cert_sync_snapshot_dir(store_dir);

    BOOL changed = cert_sync_snapshots_differ(priv->cert_sync_state, after)
                || cert_sync_snapshots_differ(before, after);

    if(changed) {
        cert_sync_broadcast_reload(gobj);
        gobj_write_integer_attr(gobj, "cert_sync_last_action", (json_int_t)time(NULL));
        gobj_write_str_attr(gobj, "cert_sync_last_result",
            copy_ok ? "reloaded" : "reloaded (copy script reported error)");
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "cert-sync detected cert change, broadcasted reload-certs",
            "store_dir",    "%s", store_dir,
            NULL
        );
    } else if(!copy_ok) {
        gobj_write_str_attr(gobj, "cert_sync_last_result", "copy-script-failed");
        gobj_write_integer_attr(gobj, "cert_sync_failures",
            gobj_read_integer_attr(gobj, "cert_sync_failures") + 1);
    } else {
        gobj_write_str_attr(gobj, "cert_sync_last_result", "no-change");
    }

    JSON_DECREF(priv->cert_sync_state);
    priv->cert_sync_state = after;   // becomes the baseline for next tick
    JSON_DECREF(before);
    return changed ? 1 : 0;
}

PRIVATE int cert_sync_arm_timer(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->cert_sync_timer) {
        return -1;
    }
    if(!priv->cert_sync_enabled) {
        clear_timeout(priv->cert_sync_timer);
        return 0;
    }

    int32_t secs = priv->cert_sync_interval_sec;
    if(secs < 30) {
        secs = 30;  // guardrail — don't hammer sudo + disk
    }

    /* Take the initial snapshot so the first tick's "changed?" check has
     * a sane baseline. Without this, the very first tick would always see
     * "different" vs the empty dict and broadcast a reload unnecessarily.
     */
    const char *store_dir = gobj_read_str_attr(gobj, "cert_sync_store_dir");
    JSON_DECREF(priv->cert_sync_state);
    priv->cert_sync_state = cert_sync_snapshot_dir(store_dir);

    set_timeout_periodic(priv->cert_sync_timer, ((json_int_t)secs) * 1000);
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "cert-sync timer armed",
        "interval_sec", "%d", (int)secs,
        "store_dir",    "%s", store_dir,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Is the requester still there to take its answer? A client that left
 *  before it (a ycommand that timed out, a closed tab) leaves its channel
 *  of __input_side__ CLOSED, and a controlcenter link that dropped leaves
 *  its C_IEVENT_CLI out of session: neither takes EV_SEND_IEV. The answer
 *  has nowhere to go, and that is the client's leaving, not an error here.
 ***************************************************************************/
PRIVATE BOOL requester_is_listening(hgobj gobj, hgobj requester, gobj_event_t event)
{
    BOOL listening;
    if(gobj_has_attr(requester, "opened")) {
        listening = gobj_read_bool_attr(requester, "opened");
    } else {
        listening = gobj_in_this_state(requester, ST_SESSION);
    }
    if(!listening) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "requester gone before its answer, answer dropped",
            "requester",    "%s", gobj_short_name(requester),
            "event",        "%s", event,
            NULL
        );
    }
    return listening;
}





            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_edit_config(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
                json_sprintf("%s: 'id' required", gobj_yuno_role_plus_name()),
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
    int found = (int)json_array_size(iter);
    if(found != 1) {
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: %s",
                    gobj_yuno_role_plus_name(),
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
PRIVATE int ac_view_config(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
                json_sprintf("%s: 'id' required", gobj_yuno_role_plus_name()),
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
                json_sprintf("%s: %s",
                    gobj_yuno_role_plus_name(),
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
PRIVATE int ac_edit_yuno_config(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
                json_sprintf("%s: Yuno not found", gobj_yuno_role_plus_name()),
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
                json_sprintf("%s: Select only one yuno please", gobj_yuno_role_plus_name()),
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
                json_sprintf("%s: Yuno without configuration", gobj_yuno_role_plus_name()),
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
                json_sprintf("%s: Yuno with too much configurations. Not supported",
                    gobj_yuno_role_plus_name()),
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
PRIVATE int ac_view_yuno_config(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
                json_sprintf("%s: Yuno not found", gobj_yuno_role_plus_name()),
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
                json_sprintf("%s: Select only one yuno please", gobj_yuno_role_plus_name()),
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
                json_sprintf("%s: Yuno without configuration", gobj_yuno_role_plus_name()),
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
                json_sprintf("%s: Yuno with too much configurations. Not supported",
                    gobj_yuno_role_plus_name()),
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
 *  Confine a caller-supplied read path to the yuneta tree (the same roots the
 *  dir-* browse commands expose: yuneta_root_dir() and "/yuneta"). realpath()
 *  canonicalizes "." / ".." and resolves symlinks, so it blocks traversal and
 *  symlink escapes and requires the file to exist. Returns 0 and fills
 *  `resolved` on success, -1 if the path is empty, missing, or escapes the
 *  allowed roots. Closes the arbitrary-file-read surface (e.g. /etc/shadow,
 *  ~/.ssh) while preserving every legitimate read under the yuneta install.
 ***************************************************************************/
PRIVATE BOOL path_is_under(const char *rp, const char *root)
{
    if(empty_string(root)) {
        return FALSE;
    }
    size_t n = strlen(root);
    while(n > 1 && root[n-1] == '/') {
        n--;  // ignore trailing slashes on the root
    }
    if(strncmp(rp, root, n) != 0) {
        return FALSE;
    }
    return rp[n] == '/' || rp[n] == 0;  // exact root or a child, not a sibling prefix
}

PRIVATE int safe_read_path(const char *filename, char *resolved, size_t resolved_sz)
{
    if(empty_string(filename)) {
        return -1;
    }
    char rp[PATH_MAX];
    if(realpath(filename, rp) == NULL) {
        return -1;  // missing/unreadable path (also rejects non-existent files)
    }
    if(!path_is_under(rp, yuneta_root_dir()) && !path_is_under(rp, "/yuneta")) {
        return -1;
    }
    if(snprintf(resolved, resolved_sz, "%s", rp) >= (int)resolved_sz) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_json(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *filename = kw_get_str(gobj, kw, "filename", 0, 0);
    if(!filename) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: filename required", gobj_yuno_role_plus_name()),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    char resolved[PATH_MAX];
    if(safe_read_path(filename, resolved, sizeof(resolved)) < 0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: File '%s' not found or not allowed",
                    gobj_yuno_role_plus_name(), filename),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    char why[256];
    size_t readed = 0;
    char *s = read_file_content(gobj, resolved, &readed, why, sizeof(why));
    if(!s || readed == 0) {
        json_t *jn_comment = json_sprintf("%s: cannot read '%s': %s",
            gobj_yuno_role_plus_name(), filename, s? "it is empty" : why);
        GBMEM_FREE(s);
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                jn_comment,
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    const char *p = strrchr(filename, '/');
    if(!p) {
        p = filename;
    } else {
        p++;
    }
    json_error_t jn_error;
    json_t *jn_s = json_loadb(s, (size_t)readed, JSON_DECODE_ANY, &jn_error);
    if(!jn_s) {
        // The operator's file, not ours: where it breaks, never its content
        char error[JSON_ERROR_TEXT_LENGTH];
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "File is not json",
            "filename",     "%s", filename,
            "error",        "%s", json_error_for_log(&jn_error, error, sizeof(error)),
            "line",         "%d", jn_error.line,
            "position",     "%d", jn_error.position,
            NULL
        );
    }
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
PRIVATE int ac_read_file(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *filename = kw_get_str(gobj, kw, "filename", 0, 0);
    if(!filename) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: filename required", gobj_yuno_role_plus_name()),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    char resolved[PATH_MAX];
    if(safe_read_path(filename, resolved, sizeof(resolved)) < 0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: File '%s' not found or not allowed",
                    gobj_yuno_role_plus_name(), filename),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    char why[256];
    size_t readed = 0;
    char *s = read_file_content(gobj, resolved, &readed, why, sizeof(why));
    if(!s) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: cannot read '%s': %s",
                    gobj_yuno_role_plus_name(), filename, why),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

    const char *p = strrchr(filename, '/');
    if(!p) {
        p = filename;
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
PRIVATE int ac_read_binary_file(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *filename = kw_get_str(gobj, kw, "filename", 0, 0);
    if(!filename) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -200,
                json_sprintf("%s: filename required", gobj_yuno_role_plus_name()),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    char resolved[PATH_MAX];
    if(safe_read_path(filename, resolved, sizeof(resolved)) < 0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -201,
                json_sprintf("%s: File '%s' not found or not allowed",
                    gobj_yuno_role_plus_name(), filename),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    size_t max_size = gbmem_get_maximum_block();
    uint64_t size = filesize(resolved);
    if(size > max_size) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -205,
                json_sprintf("%s: File '%s' too large. Maximum supported size is %zu",
                    gobj_yuno_role_plus_name(),
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

    int fp = open(resolved, O_RDONLY|O_CLOEXEC);
    if(fp<0) {
        return gobj_send_event(
            src,
            event,
            msg_iev_build_response(gobj,
                -202,
                json_sprintf("%s: Cannot open '%s', %s",
                    gobj_yuno_role_plus_name(), filename, strerror(errno)),
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
                json_sprintf("%s: No memory", gobj_yuno_role_plus_name()),
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
                json_sprintf("%s: Error with file '%s': %s",
                    gobj_yuno_role_plus_name(), filename, strerror(err)),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    close(fp);

    const char *p = strrchr(filename, '/');
    if(!p) {
        p = filename;
    } else {
        p++;
    }

    gbuffer_t *gbuf_base64 = gbuffer_binary_to_base64(s, size);

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
PRIVATE int ac_read_running_keys(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
            EV_READ_FILE,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: Yuno not found", gobj_yuno_role_plus_name()),
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
            EV_READ_FILE,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: Select only one yuno please", gobj_yuno_role_plus_name()),
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
    char bfbinary[PATH_MAX];
    bfbinary[0] = '\0';
    gbuffer_t *gbuf_sh = gbuffer_create(4*1024, 32*1024);
    if(!build_yuno_running_script(gobj, gbuf_sh, yuno, bfbinary, sizeof(bfbinary), TRUE)) {
        // Error already logged
        gbuffer_decref(gbuf_sh);
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            EV_READ_FILE,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: build_yuno_running_script() FAILED",
                    gobj_yuno_role_plus_name()),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }
    char *s = gbuffer_cur_rd_pointer(gbuf_sh);
    if(!s) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "running script is empty",
            NULL
        );
        gbuffer_decref(gbuf_sh);
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            EV_READ_FILE,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: running script is empty", gobj_yuno_role_plus_name()),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

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
        EV_READ_FILE,
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
PRIVATE int ac_read_running_bin(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
            EV_READ_FILE,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: Yuno not found", gobj_yuno_role_plus_name()),
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
            EV_READ_FILE,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: Select only one yuno please", gobj_yuno_role_plus_name()),
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
    char bfbinary[PATH_MAX];
    bfbinary[0] = '\0';
    gbuffer_t *gbuf_sh = gbuffer_create(4*1024, 32*1024);
    if(!build_yuno_running_script(gobj, gbuf_sh, yuno, bfbinary, sizeof(bfbinary), FALSE)) {
        // Error already logged
        gbuffer_decref(gbuf_sh);
        JSON_DECREF(iter)
        return gobj_send_event(
            src,
            EV_READ_FILE,
            msg_iev_build_response(gobj,
                -1,
                json_sprintf("%s: build_yuno_running_script() FAILED",
                    gobj_yuno_role_plus_name()),
                0,
                0,
                kw  // owned
            ),
            gobj
        );
    }

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
        EV_READ_FILE,
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
PRIVATE int ac_play_yuno_ack(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "hs_yuno NULL",
            NULL
        );
        KW_DECREF(kw);
        return 0;
    }

    int action_return = kw_get_int(gobj, kw, "result", -1, 0);
    if(action_return == 0) {
        json_object_set_new(yuno, "yuno_playing", json_true());

        // volatil: memory only -- saved, a lifecycle write would put this release over a newer one
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

        /*
         *  Not published: the counter of play-yuno counts every ACK as a
         *  yuno played, so its requester gets the failure by timeout.
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SERVICE,
            "msg",          "%s", "The yuno refused to play",
            "yuno_id",      "%s", __yuno__,
            "result",       "%d", action_return,
            "comment",      "%s", kw_get_str(gobj, kw, "comment", "", 0),
            NULL
        );
        KW_DECREF(kw);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_pause_yuno_ack(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "hs_yuno NULL",
                NULL
            );
            KW_DECREF(kw);
            return 0;
        }
        json_object_set_new(yuno, "yuno_playing", json_false());
        // volatil: memory only -- saved, a lifecycle write would put this release over a newer one
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
        /*
         *  Not published: the counter of pause-yuno counts every ACK as a
         *  yuno paused, so its requester gets the failure by timeout.
         */
        hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
        json_t *jn_yuno_id = channel_gobj? gobj_read_user_data(channel_gobj, "__yuno__") : NULL;
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SERVICE,
            "msg",          "%s", "The yuno refused to pause",
            "yuno_id",      "%s", json_is_string(jn_yuno_id)? json_string_value(jn_yuno_id) : "",
            "result",       "%d", action_return,
            "comment",      "%s", kw_get_str(gobj, kw, "comment", "", 0),
            NULL
        );
        KW_DECREF(kw);
    }
    return 0;
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_stats_yuno_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);

    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
    if(strcmp(dst_service, gobj_name(gobj))==0) {
        /*
         *  Stats asked by the agent itself: a reading of a watch-yuno-stats
         *  goes to its requesters, anything else has nowhere to go.
         */
        json_t *jn_watch_md = kw_get_dict(gobj, kw, "__md_iev__`agent_watch", 0, 0);
        if(jn_watch_md) {
            watch_answer(gobj, jn_watch_md, kw);
        }
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_input_side,
        dst_service
    );
    if(!gobj_requester) {
        /*
         *  Command cascaded from a controlcenter: it arrived via the outbound
         *  "controlcenter" C_IEVENT_CLI service, not an __input_side__ child.
         *  Send EV_SEND_IEV to that C_IEVENT_CLI: it is the ievent serializer on
         *  its own stack (its channel sits above a raw C_PROT_TCP4H, not a
         *  C_IEVENT_SRV), so its ac_send_iev unwraps and serializes the inner
         *  iev (EV_MT_COMMAND_ANSWER / EV_MT_STATS_ANSWER) up the link — uniform
         *  with the input-side C_CHANNEL path.
         */
        gobj_requester = gobj_find_service(dst_service, FALSE);
    }
    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "child not found",
            "child",        "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_ievent_id);
    if(!requester_is_listening(gobj, gobj_requester, event)) {
        KW_DECREF(kw);
        return 0;
    }

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *  HACK intermediate node
 ***************************************************************************/
PRIVATE int ac_command_yuno_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);

    const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
    if(strcmp(dst_service, gobj_name(gobj))==0) {
        /*
         *  Command requested by the agent itself: nothing to forward. The
         *  answer to a publish-stats says whether the yuno publishes.
         */
        json_t *jn_publish_md = kw_get_dict(gobj, kw, "__md_iev__`agent_publish", 0, 0);
        if(jn_publish_md) {
            publication_answer(gobj, jn_publish_md, kw);
        }
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }

    hgobj gobj_requester = gobj_child_by_name(
        priv->gobj_input_side,
        dst_service
    );
    if(!gobj_requester) {
        /*
         *  Command cascaded from a controlcenter: it arrived via the outbound
         *  "controlcenter" C_IEVENT_CLI service, not an __input_side__ child.
         *  Send EV_SEND_IEV to that C_IEVENT_CLI: it is the ievent serializer on
         *  its own stack (its channel sits above a raw C_PROT_TCP4H, not a
         *  C_IEVENT_SRV), so its ac_send_iev unwraps and serializes the inner
         *  iev (EV_MT_COMMAND_ANSWER / EV_MT_STATS_ANSWER) up the link — uniform
         *  with the input-side C_CHANNEL path.
         */
        gobj_requester = gobj_find_service(dst_service, FALSE);
    }
    if(!gobj_requester) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "child not found",
            "child",        "%s", dst_service,
            NULL
        );
        JSON_DECREF(jn_ievent_id);
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_ievent_id);
    if(!requester_is_listening(gobj, gobj_requester, event)) {
        KW_DECREF(kw);
        return 0;
    }

    KW_INCREF(kw);
    json_t *kw_redirect = msg_iev_set_back_metadata(gobj, kw, kw, TRUE);

    json_t *iev = iev_create(
        gobj,
        event,
        kw_redirect    // owned
    );

    return gobj_send_event(
        gobj_requester,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *client_yuno_role = kw_get_str(gobj, kw, "client_yuno_role", "", 0);
    if(strcasecmp(client_yuno_role, "yuneta_cli")==0 ||
        strcasecmp(client_yuno_role, "yuneta_agent")==0 ||
        strcasecmp(client_yuno_role, "ycli")==0 ||
        strcasecmp(client_yuno_role, "ybatch")==0 ||
        strcasecmp(client_yuno_role, "ystats")==0 ||
        strcasecmp(client_yuno_role, "ycommand")==0 ||
        strcasecmp(client_yuno_role, "ytests")==0 ||
        strcasecmp(client_yuno_role, "gui_agent")==0 ||
        strcasecmp(client_yuno_role, "gui_treedb")==0
    ) {
        // let it.

        KW_DECREF(kw);
        return 0;
    }

    const char *yuno_id = kw_get_str(gobj, kw, "identity_card`yuno_id", 0, KW_REQUIRED);
    pid_t pid = (pid_t)kw_get_int(gobj, kw, "identity_card`pid", 0, KW_REQUIRED);
    pid_t watcher_pid = (pid_t)kw_get_int(gobj, kw, "identity_card`watcher_pid", 0, 0);
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


    int found = (int)json_array_size(iter_yunos);
    if(found==0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
    pid_t _pid = SDATA_GET_INT(yuno, "yuno_pid");
    if(_pid && getpgid(_pid) >= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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

    /*
     *  It's open: yuno_running below is now the authoritative mark.
     */
    json_object_del(priv->launching_yunos, yuno_id);

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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "channel_gobj NULL",
            NULL
        );
    }

    // volatil: memory only -- saved, a lifecycle write would put this release over a newer one
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
        /*
         *  run-yuno play=0 launched this process and asked us NOT to auto-play it.
         *  Honour it once and consume the transient marker; a watcher crash relaunch
         *  reuses the same launch_id but the marker is already gone, so must_play
         *  reconciliation stays intact for it.
         */
        BOOL skip_auto_play = FALSE;
        json_int_t launch_id = kw_get_int(gobj, kw, "identity_card`launch_id", 0, 0);
        if(launch_id) {
            char s_launch_id[32];
            snprintf(s_launch_id, sizeof(s_launch_id), "%lld", (long long)launch_id);
            if(kw_has_key(priv->no_play_launches, s_launch_id)) {
                skip_auto_play = TRUE;
                json_object_del(priv->no_play_launches, s_launch_id);
            }
        }

        BOOL must_play = SDATA_GET_BOOL(yuno, "must_play");
        if(must_play && !skip_auto_play) {
            json_t *kw_play = json_pack("{s:s, s:s}",
                "realm_id", realm_id,
                "id", yuno_id
            );

            // The requester run-yuno kept in the row (cmd_run_yuno, do_play): the ACK of this play goes back to it
            const char *requester = kw_get_str(gobj, yuno, "_requester", "", 0);
            if(!empty_string(requester)) {
                json_t *requester_md_iev = kw_get_dict(gobj, yuno, "_requester_md_iev", 0, 0);
                json_t *kw_ext = json_pack("{s:O}",
                    "__md_iev__", requester_md_iev
                );
                json_object_update_new(kw_play, kw_ext);
                json_object_set_new(yuno, "_requester", json_string(""));
            }
            cmd_play_yuno(gobj, "play-yuno", kw_play, gobj);
        }
        // volatil: memory only -- saved, a lifecycle write would put this release over a newer one

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
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "channel_gobj NULL",
            NULL
        );
        KW_DECREF(kw);
        return 0;
    }

    json_t *yuno = NULL;
    const char *__yuno__ = json_string_value(gobj_read_user_data(channel_gobj, "__yuno__"));
    if(!empty_string(__yuno__)) {
        yuno = gobj_get_node(
            priv->resource,
            "yunos",
            json_pack("{s:s}", "id", __yuno__),
            json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
            src
        );
    }

    if(!yuno) {
        // Must be yuneta_cli or a yuno refused!.
        delete_consoles_on_disconnection(gobj, kw, src);
        delete_watch_of_channel(gobj, channel_gobj);
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

    // volatil: memory only -- saved, a lifecycle write would put this release over a newer one
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
PRIVATE int ac_final_count(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *info = kw_get_str(gobj, kw, "info", "", 0);
    int max_count = (int)kw_get_int(gobj, kw, "max_count", 0, 0);
    int cur_count = (int)kw_get_int(gobj, kw, "cur_count", 0, 0);

    /*
     *  The end of the answer of run-yuno, kill-yuno, play-yuno and pause-yuno:
     *  every yuno ACKed, or the counter's expiration_timeout ran out (then
     *  cur_count < max_count); the requester the command noted gets it
     */
    json_t *iter_yunos = gobj_kw_get_user_data(src, "iter", 0, KW_EXTRACT|KW_REQUIRED);
    json_t *kw_answer = gobj_kw_get_user_data(src, "kw_answer", 0, KW_EXTRACT|KW_REQUIRED);

    const char *requester = kw_get_str(gobj, kw, "requester", 0, 0);
    if(empty_string(requester)) {
        // Can be empty if it comes from auto-play
        JSON_DECREF(iter_yunos);
        KW_DECREF(kw_answer);
        KW_DECREF(kw);
        return -1;
    }

    hgobj gobj_requester_channel = gobj_child_by_name(
        priv->gobj_input_side,
        requester
    );
    if(!gobj_requester_channel) {
        /*
         *  Command cascaded from a controlcenter: it arrived via the outbound
         *  "controlcenter" C_IEVENT_CLI service, not an __input_side__ child,
         *  so the requester names a top-level service. Same fallback as
         *  ac_command_yuno_answer / ac_stats_yuno_answer: send EV_SEND_IEV to
         *  that C_IEVENT_CLI, which serializes the inner iev up the link.
         *
         *  Without it EVERY counter-driven answer — kill-yuno, run-yuno,
         *  play-yuno, pause-yuno — was dropped here for any client behind a
         *  controlcenter, which is every SPA. The work was done; only the
         *  answer was lost, so the caller waits forever.
         */
        gobj_requester_channel = gobj_find_service(requester, FALSE);
    }
    if(!gobj_requester_channel) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "requester channel child not found",
            "chanel child", "%s", requester,
            NULL
        );
        JSON_DECREF(iter_yunos);
        KW_DECREF(kw_answer);
        KW_DECREF(kw);
        return 0;
    }
    if(!requester_is_listening(gobj, gobj_requester_channel, EV_MT_COMMAND_ANSWER)) {
        JSON_DECREF(iter_yunos);
        KW_DECREF(kw_answer);
        KW_DECREF(kw);
        return 0;
    }

    BOOL ok = (max_count>0 && max_count==cur_count);

    json_t *jn_comment = json_sprintf("%s: %s%s (%d raised, %d reached)\n",
        gobj_yuno_role_plus_name(),
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

    KW_DECREF(kw);

    json_t *iev = iev_create(
        gobj,
        EV_MT_COMMAND_ANSWER,
        webix    // owned
    );

    return gobj_send_event(
        gobj_requester_channel,
        EV_SEND_IEV,
        iev,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_console = named_dict(gobj, priv->list_consoles, gobj_name(src), TRUE);
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
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "no route child found",
                    "service",      "%s", route_service,
                    "child",        "%s", route_child,
                    NULL
                );
                continue;
            }

            gobj_send_event(
                gobj_input_gate,
                EV_TTY_OPEN,
                msg_iev_build_response(gobj,
                    0,  // result
                    0,  // comment
                    0,  // schema
                    json_incref(kw), // owned
                    kw_incref(jn_route)  // owned
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
PRIVATE int ac_tty_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_shutdowning()) {
        KW_DECREF(kw);
        return 0;
    }

    json_t *jn_console = named_dict_take(priv->list_consoles, gobj_name(src));
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
                    "msgset",       "%s", MSGSET_INTERNAL,
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
                EV_TTY_CLOSE,
                msg_iev_build_response(gobj,
                    0,  // result
                    0,  // comment
                    0,  // schema
                    json_incref(kw), // owned
                    kw_incref(jn_route)  // owned
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
PRIVATE int ac_tty_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_console = named_dict(gobj, priv->list_consoles, gobj_name(src), TRUE);
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
                    "msgset",       "%s", MSGSET_INTERNAL,
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
                    kw_incref(jn_route)  // owned
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
PRIVATE int ac_write_tty(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *name= kw_get_str(gobj, kw, "name", 0, 0);
    const char *content64 = kw_get_str(gobj, kw, "content64", 0, 0);
    if(empty_string(content64)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "content64 required",
            "name",         "%s", name,
            NULL
        );
        /*  Malformed write-tty frame: log and drop THIS message, not the
         *  shared controlcenter_cli link. */
        KW_DECREF(kw);
        return 0;
    }

    hgobj gobj_console = gobj_find_service(name, FALSE);
    if(!gobj_console) {
        /*
         *  A write-tty for a console that isn't here — usually a benign race:
         *  the client (e.g. gui_agent) sent a keystroke in the window between
         *  the console closing on this node and its EV_TTY_CLOSE reaching the
         *  client. Do NOT EV_DROP src: over the shared controlcenter_cli link
         *  that would tear down the whole control channel (every panel + every
         *  other console) for one stray tty write. Log and ignore.
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "console not found",
            "name",         "%s", name,
            NULL
        );
        /*
         *  Tell the requester the console is gone so its Terminal tab can
         *  close/cleanup: the original EV_TTY_CLOSE may have been lost while
         *  its link flapped. src is the gate this write-tty came through —
         *  the same path ac_tty_close() uses to reach the client.
         */
        gobj_send_event(
            src,
            EV_TTY_CLOSE,
            msg_iev_build_response(gobj,
                0,  // result
                0,  // comment
                0,  // schema
                json_pack("{s:s}", "name", name?name:""),   // owned
                json_incref(kw)  // owned, echoes __md_iev__ back
            ),
            gobj
        );
        KW_DECREF(kw);
        return 0;
    }

    gbuffer_t *gbuf = gbuffer_base64_to_binary(content64, strlen(content64));
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Bad data",
            "name",         "%s", name,
            NULL
        );
        /*  Undecodable base64: log and drop THIS message, not the shared
         *  controlcenter_cli link. */
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
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->util_yunos_running) {
        priv->util_yunos_running = 1;
        run_util_yunos(gobj);
        set_timeout(priv->timer, priv->timerStBoot);
    } else if(!priv->enabled_yunos_running) {
        priv->enabled_yunos_running = 1;
        run_enabled_yunos(gobj, TRUE);
        exec_startup_command(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Periodic cert-sync tick.
 ***************************************************************************/
PRIVATE int ac_timeout_periodic(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->watch_timer) {
        watch_tick(gobj, NULL);
    } else if(src == priv->restart_wait_timer) {
        restart_wait_tick(gobj);
    } else if(src == priv->cert_sync_timer) {
        cert_sync_tick(gobj);
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "periodic timeout of an unknown timer",
            "src",          "%s", gobj_short_name(src),
            NULL
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  The first readings of a watch-yuno-stats, posted by the command so
 *  that they follow its answer.
 ***************************************************************************/
PRIVATE int ac_watch_reading(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *route = kw_get_str(gobj, kw, "route", "", KW_REQUIRED);
    if(kw_has_key(priv->watches, route)) {    // stop=1 or its channel closed in between: none
        watch_tick(gobj, route);
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
    .mt_create      = mt_create,
    .mt_destroy     = mt_destroy,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_play        = mt_play,
    .mt_pause       = mt_pause,
    .mt_writing     = mt_writing,
    .mt_trace_on    = mt_trace_on,
    .mt_trace_off   = mt_trace_off,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_AGENT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_READ_RUNNING_KEYS);
GOBJ_DEFINE_EVENT(EV_READ_RUNNING_BIN);
GOBJ_DEFINE_EVENT(EV_WATCH_READING);

/***************************************************************************
 *  Authz service notifications.  In mt_start the agent subscribes to ALL of
 *  its authz service's output events; the agent has nothing to do on
 *  EV_AUTHZ_USER_LOGIN/LOGOUT/NEW, so it only accepts-and-ignores them.
 *  Without this they reach the FSM as "Event NOT DEFINED in state" on every
 *  login/logout.
 ***************************************************************************/
PRIVATE int ac_authz_user_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
        {EV_AUTHZ_USER_LOGIN,     ac_authz_user_event,    0},
        {EV_AUTHZ_USER_LOGOUT,    ac_authz_user_event,    0},
        {EV_AUTHZ_USER_NEW,       ac_authz_user_event,    0},
        {EV_FINAL_COUNT,          ac_final_count,         0},
        {EV_TTY_DATA,             ac_tty_data,            0},
        {EV_TTY_OPEN,             ac_tty_open,            0},
        {EV_TTY_CLOSE,            ac_tty_close,           0},
        {EV_WRITE_TTY,            ac_write_tty,           0},
        {EV_TIMEOUT,              ac_timeout,             0},
        {EV_TIMEOUT_PERIODIC,     ac_timeout_periodic,    0},
        {EV_WATCH_READING,        ac_watch_reading,       0},
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
        {EV_AUTHZ_USER_LOGIN,     0},
        {EV_AUTHZ_USER_LOGOUT,    0},
        {EV_AUTHZ_USER_NEW,       0},
        {EV_FINAL_COUNT,          0},
        {EV_TIMEOUT,              0},
        {EV_TIMEOUT_PERIODIC,     0},
        {EV_WATCH_READING,        0},
        {EV_STOPPED,              0},

        /* Publications (outputs) */
        {EV_PLAY_YUNO_ACK,        EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_PAUSE_YUNO_ACK,       EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_MT_STATS_ANSWER,      EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},
        {EV_MT_COMMAND_ANSWER,    EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},

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
        attrs_table,
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
    return create_gclass(C_AGENT);
}
