/****************************************************************************
 *          MAIN_YUNETA_AGENT22.C
 *          yuneta_agent22 main
 *
 *          Copyright (c) 2022 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_agent22.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "yuneta_agent22"
#define APP_DOC         "Yuneta Agent22"

#define APP_VERSION     YUNETA_VERSION
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<support at artgins.com>"

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           512
#define MEM_MAX_BLOCK           (200*1024*1024L)     // 200*M
#define MEM_SUPERBLOCK          (200*1024*1024L)     // 200*M
#define MEM_MAX_SYSTEM_MEMORY   (2*1024*1024*1024L)    // 2*G

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'realm_owner': 'agent22',                                   \n\
        'work_dir': '/yuneta',                                      \n\
        'domain_dir': 'realms/agent/agent2'                         \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['yuneta', 'core']                                  \n\
    }                                                               \n\
}                                                                   \n\
";

PRIVATE char variable_config[]= "\
{                                                                   \n\
    '__json_config_variables__': {                                  \n\
        '__realm_id__': 'agent.yunetacontrol.com',                  \n\
        '__output_url__': 'yunetacontrol.ovh:1994'                  \n\
    },                                                              \n\
    'environment': {                                                \n\
        'realm_id': '(^^__realm_id__^^)',                           \n\
        'console_log_handlers': {                                   \n\
            'to_stdout': {                                          \n\
                'handler_type': 'stdout',                           \n\
                'handler_options': 255                              \n\
            },                                                      \n\
            'to_udp': {                                             \n\
                'handler_type': 'udp',                              \n\
                'url': 'udp://127.0.0.1:1992',                      \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
            'to_file': {                                            \n\
                'handler_type': 'file',                             \n\
                'handler_options': 255,                             \n\
                'filename_mask': 'yuneta_agent22-W.log'             \n\
            },                                                      \n\
            'to_udp': {                                             \n\
                'handler_type': 'udp',                              \n\
                'url': 'udp://127.0.0.1:1992',                      \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'yuno_name': '(^^__hostname__^^)',                          \n\
        'trace_levels': {                                           \n\
            'Tcp0': ['connections'],                                \n\
            'TcpS0': ['listen', 'not-accepted', 'accepted'],        \n\
            'Tcp1': ['connections'],                                \n\
            'TcpS1': ['listen', 'not-accepted', 'accepted']         \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        'Authz.max_sessions_per_user': 4,                           \n\
        'Authz.initial_load': {                                     \n\
            'roles': [                                              \n\
                {                                                   \n\
                    'id': 'root',                                   \n\
                    'disabled': false,                              \n\
                    'description': 'Super-Owner of system',         \n\
                    'realm_id': '*',                                \n\
                    'parent_role_id': '',                           \n\
                    'service': '*',                                 \n\
                    'permission': '*'                               \n\
                },                                                  \n\
                {                                                   \n\
                    'id': 'owner',                                  \n\
                    'disabled': false,                              \n\
                    'description': 'Owner of system',               \n\
                    'realm_id': '(^^__realm_id__^^)',               \n\
                    'parent_role_id': '',                           \n\
                    'service': '*',                                 \n\
                    'permission': '*'                               \n\
                },                                                  \n\
                {                                                   \n\
                    'id': 'manage-authzs',                          \n\
                    'disabled': false,                              \n\
                    'description': 'Management of Authz',           \n\
                    'realm_id': '(^^__realm_id__^^)',               \n\
                    'parent_role_id': '',                           \n\
                    'service': 'treedb_authzs',                     \n\
                    'permission': '*'                               \n\
                },                                                  \n\
                {                                                   \n\
                    'id': 'write-authzs',                           \n\
                    'disabled': false,                              \n\
                    'description': 'Can write authz topics',        \n\
                    'realm_id': '(^^__realm_id__^^)',               \n\
                    'parent_role_id': 'roles^manage-authzs^roles',  \n\
                    'service': 'treedb_authzs',                     \n\
                    'permission': 'write'                           \n\
                },                                                  \n\
                {                                                   \n\
                    'id': 'read-authzs',                            \n\
                    'disabled': false,                              \n\
                    'description': 'Can read authz topics',         \n\
                    'realm_id': '(^^__realm_id__^^)',               \n\
                    'parent_role_id': 'roles^manage-authzs^roles',  \n\
                    'service': 'treedb_authzs',                     \n\
                    'permission': 'read'                            \n\
                },                                                  \n\
                {                                                   \n\
                    'id': 'manage-yuneta-agent22',                    \n\
                    'disabled': false,                              \n\
                    'description': 'Management of Yuneta Agent22',    \n\
                    'realm_id': '(^^__realm_id__^^)',               \n\
                    'parent_role_id': '',                           \n\
                    'service': 'treedb_yuneta_agent22',               \n\
                    'permission': '*'                               \n\
                }                                                   \n\
            ],                                                      \n\
            'users': [                                              \n\
                {                                                   \n\
                    'id': 'yuneta',                                 \n\
                    'roles': [                                      \n\
                        'roles^root^users',                         \n\
                        'roles^owner^users'                         \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        }                                                           \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                               \n\
            'name': 'agent22',                          \n\
            'gclass': 'Agent22',                        \n\
            'default_service': true,                    \n\
            'autostart': true,                          \n\
            'autoplay': false                           \n\
        },                                              \n\
        {                                               \n\
            'name': 'controlcenter',                    \n\
            'gclass': 'IEvent_cli',                     \n\
            'autostart': true,                          \n\
            'autoplay': true,                           \n\
            'kw': {                                     \n\
                'remote_yuno_name': '',                 \n\
                'remote_yuno_role': 'controlcenter',    \n\
                'remote_yuno_service': 'controlcenter'  \n\
            },                                          \n\
            'children': [                                \n\
                {                                               \n\
                    'name': '__controlcenter__',                \n\
                    'gclass': 'C_IOGATE',                         \n\
                    'as_service': true,                         \n\
                    'kw': {                                     \n\
                        'persistent_channels': false            \n\
                    },                                          \n\
                    'children': [                                \n\
                        {                                               \n\
                            'name': '__controlcenter__',                \n\
                            'gclass': 'C_CHANNEL',                        \n\
                            'kw': {                                     \n\
                            },                                          \n\
                            'children': [                                \n\
                                {                                       \n\
                                    'name': '__controlcenter__',        \n\
                                    'gclass': 'C_PROT_TCP4H',           \n\
                                    'children': [                                \n\
                                        {                                       \n\
                                            'name': '__connex_controlcenter__', \n\
                                            'gclass': 'Connexs',                \n\
                                            'kw': {                             \n\
                                                'timeout_between_connections': 10000, \n\
                                                'crypto': {                     \n\
                                                    'library': 'openssl',       \n\
                                                    'trace': false              \n\
                                                },                              \n\
                                                'urls':[                        \n\
        'tcps://(^^__sys_machine__^^).(^^__node_owner__^^).(^^__output_url__^^)'    \n\
                                                ]                               \n\
                                            }                                   \n\
                                        }                                       \n\
                                    ]                                           \n\
                                }                                       \n\
                            ]                                           \n\
                        }                                               \n\
                    ]                                           \n\
                }                                              \n\
            ]                                           \n\
        },                                              \n\
        {                                                           \n\
            'name': 'authz',                                        \n\
            'gclass': 'C_AUTHZ',                                      \n\
            'default_service': false,                               \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                            \n\
            ]                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *                      Register
 ***************************************************************************/
static int register_yuno_and_more(void)
{
    /*--------------------*
     *  Register gclass
     *--------------------*/
    register_c_agent22();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_YUNO), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // Samples of traces
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "identity-card", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "identity-card", TRUE);

    // gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", TRUE);

    // Global traces
    // gobj_set_global_trace("create_delete", TRUE);
    // gobj_set_global_trace("machine", TRUE);
    // gobj_set_global_trace("create_delete", TRUE);
    // gobj_set_global_trace("create_delete2", TRUE);
    // gobj_set_global_trace("subscriptions", TRUE);
    // gobj_set_global_trace("start_stop", TRUE);
    // gobj_set_global_trace("monitor", TRUE);
    // gobj_set_global_trace("event_monitor", TRUE);
    // gobj_set_global_trace("liburing", TRUE);
    // gobj_set_global_trace("ev_kw", TRUE);
    // gobj_set_global_trace("authzs", TRUE);
    // gobj_set_global_trace("states", TRUE);
    // gobj_set_global_trace("gbuffers", TRUE);
    // gobj_set_global_trace("timer_periodic", TRUE);
    // gobj_set_global_trace("timer", TRUE);
    // gobj_set_global_trace("liburing_timer", TRUE);

    return 0;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *      To check
     *------------------------------------------------*/
    // gobj_set_deep_tracing(1);
    // set_auto_kill_time(6);
    // set_measure_times(-1 & ~YEV_TIMER_TYPE);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs, default internal dbsimple
        NULL,       // command_parser, default internal command_parser
        NULL,       // stats_parser, default internal stats_parser
        NULL,       // authz_checker, default Monoclass C_AUTHZ
        NULL,       // authenticate_parser, default Monoclass C_AUTHZ
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );
    return yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        NULL
    );
}
