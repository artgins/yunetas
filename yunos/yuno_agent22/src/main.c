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
        'use_system_memory': true,                                  \n\
        'log_gbmem_info': true,                                     \n\
        'MEM_MIN_BLOCK': 512,                                       \n\
        'MEM_MAX_BLOCK': 209715200,             #^^  200*M          \n\
        'MEM_SUPERBLOCK': 209715200,            #^^  200*M          \n\
        'MEM_MAX_SYSTEM_MEMORY': 2147483648,    #^^ 2*G             \n\
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
            'zchilds': [                                \n\
                {                                               \n\
                    'name': '__controlcenter__',                \n\
                    'gclass': 'IOGate',                         \n\
                    'as_service': true,                         \n\
                    'kw': {                                     \n\
                        'persistent_channels': false            \n\
                    },                                          \n\
                    'zchilds': [                                \n\
                        {                                               \n\
                            'name': '__controlcenter__',                \n\
                            'gclass': 'Channel',                        \n\
                            'kw': {                                     \n\
                            },                                          \n\
                            'zchilds': [                                \n\
                                {                                       \n\
                                    'name': '__controlcenter__',        \n\
                                    'gclass': 'Prot_header4',           \n\
                                    'zchilds': [                                \n\
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
            'gclass': 'Authz',                                      \n\
            'default_service': false,                               \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'zchilds': [                                            \n\
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
     *  To trace memory
     *------------------------------------------------*/
#ifdef CONFIG_BUILD_TYPE_RELEASE
    static uint32_t mem_list[] = {0,0};
    gbmem_trace_alloc_free(1, mem_list);
#endif

    /*
     *  Estas trazas siempre en el agent22e.
     */
    gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "identity-card", TRUE);
    gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "identity-card", TRUE);

    if(argv[1]) {
        if(strcmp(argv[1], "verbose2")==0) {
            gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "ievents2", TRUE);
            argc = 1;
        } else if(strcmp(argv[1], "verbose3")==0) {
            gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "ievents2", TRUE);
            gobj_set_global_trace("machine", TRUE);
            argc = 1;
        } else if(strcmp(argv[1], "verbose4")==0) {
            gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "ievents2", TRUE);
            gobj_set_global_trace("machine", TRUE);
            gobj_set_global_trace("ev_kw", TRUE);
            argc = 1;
        } else if(strcmp(argv[1], "verbose5")==0) {
            gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "ievents2", TRUE);
            gobj_set_global_trace("", TRUE);
            gobj_set_global_trace("ev_kw", TRUE);
            argc = 1;
        } else if(strcmp(argv[1], "verbose")==0) {
            gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "ievents", TRUE);
            argc = 1;
        }
    }

//     gobj_set_gclass_trace(GCLASS_ROUTER, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_ROUTER, "ev_kw", TRUE);
//     gobj_set_gclass_trace(GCLASS_ROUTER, "routing", TRUE);
//     gobj_set_gclass_trace(GCLASS_IOGATE, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_IOGATE, "ev_kw", TRUE);
//     gobj_set_gclass_trace(GCLASS_CHANNEL, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_CHANNEL, "ev_kw", TRUE);
//     gobj_set_gclass_trace(GCLASS_COUNTER, "debug", TRUE);

//     gobj_set_gclass_trace(GCLASS_AGENT22, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_AGENT22, "ev_kw", TRUE);
//     gobj_set_gclass_trace(GCLASS_COUNTER, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_COUNTER, "ev_kw", TRUE);

//      gobj_set_gclass_trace(GCLASS_TCP0, "traffic", TRUE);

//     gobj_set_gobj_trace(0, "machine", TRUE, 0);
//     gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
//     gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
//     gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
//     gobj_set_gobj_trace(0, "start_stop", TRUE, 0);

    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);

//     set_auto_kill_time(7);
//gobj_set_deep_tracing(1); // TODO TEST

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        dbattrs_startup,            // dbsimple2
        dbattrs_end,                // dbsimple2
        dbattrs_load_persistent,    // dbsimple2
        dbattrs_save_persistent,    // dbsimple2
        dbattrs_remove_persistent,  // dbsimple2
        dbattrs_list_persistent,    // dbsimple2
        command_parser,
        stats_parser,
        authz_checker,              // Monoclass Authz
        authenticate_parser         // Monoclass Authz
    );
    return yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more
    );
}
