/****************************************************************************
 *          Yuneta Agent22 Main
 *
 *          A second backdoor for yuneta agents being controlled by controlcenter
 *
 *          Copyright (c) 2022 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include <c_pty.h>
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
        'realm_owner': '',                                          \n\
        'work_dir': '/yuneta',                                      \n\
        'domain_dir': 'realms/agent/agent2'                         \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['yuneta', 'core']                                  \n\
    }                                                               \n\
}                                                                   \n\
";

/*
 *  The link to the controlcenter (`controlcenter_cli`) trusts ONLY the
 *  controlcenter's own certificate, `certs/controlcenter.crt`, whose key never
 *  leaves the controlcenter's machine, on its own port (`__output_url__`).
 *  Up to 7.30.3 it trusted `yuneta_agent.crt`, the certificate every agent
 *  serves, whose key every node holds. The old port stays served for the
 *  agents of before.
 *
 *  And the agent shows the controlcenter who it is with its OWN pair,
 *  `certs/yuneta_agent.{crt,key}` (made on each node, yuneta-agent-cert),
 *  presented as the client certificate of the link (mutual TLS, 7.30.10+):
 *  the controlcenter pins the node uuid to that key (accept-node).
 *
 *  A top-level `__json_config_variables__` has the scope of its own json
 *  text: these are replaced in THIS text when it is loaded, before the
 *  agent's json file is merged, so the input urls leave fixed -- no json
 *  file can open 1991 to the network or move 1993.
 *
 *  `__output_url__` is in `"global"`, as
 *  `controlcenter.__json_config_variables__`: that is replaced when the
 *  `controlcenter` service is created, AFTER the json file is merged (a deep
 *  merge, the file wins), so a node can point its agent at another
 *  controlcenter with the same key in its json file:
 *
 *      "global": {
 *          "controlcenter.__json_config_variables__": {
 *              "__output_url__": "yunetacontrol.ovh:1989"
 *          }
 *      }
 *
 *  A top-level `__json_config_variables__` in the json file reaches only
 *  that file: it changes nothing here.
 *
 *  The operator of a command the controlcenter forwards (7.30.9+): the
 *  agent does not take the `__username__` the controlcenter says, it
 *  verifies the operator assertion that comes with the command -- a token
 *  the controlcenter signs for THIS node (aud = its node uuid), with the key
 *  whose public half is `Authz.assertion_jwks` below -- and takes its `sub`.
 *  A command with none (a controlcenter without the key, a local user) is
 *  of `service_principal`, 'controlcenter'; a bad one is refused. The
 *  assertion's time is checked with a day of leeway: a box whose clock
 *  drifts (no NTP) must still take its controlcenter's commands; what binds
 *  an assertion is its audience. To take the controlcenter's word again:
 *  `"global": {"controlcenter.verify_operator": false}` in the json file.
 */
PRIVATE char variable_config[]= "\
{                                                                   \n\
    '__json_config_variables__': {                                  \n\
        '__realm_id__': '/yuneta_agent.trdb'                        \n\
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
                'max_megas_rotatoryfile_size': 600,                 \n\
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
            'C_TCP': ['connections'],                               \n\
            'C_TCP_S': ['listen', 'not-accepted', 'accepted']       \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        'controlcenter.__json_config_variables__': {               \n\
            '__output_url__': 'yunetacontrol.ovh:1989'                  \n\
        },                                                          \n\
        'Authz.master': false,                                      \n\
        'Authz.authz_service': 'agent',                             \n\
        'Authz.assertion_time_leeway': 86400,                       \n\
        'Authz.assertion_jwks': [                                   \n\
            {                                                       \n\
                'kid': 'controlcenter',                             \n\
                'kty': 'EC',                                        \n\
                'crv': 'P-256',                                     \n\
                'alg': 'ES256',                                     \n\
                'use': 'sig',                                       \n\
                'x': 'pfcUj6lfWF5NlMBi47QQt0NPtGs570TPZews4VJLtlE', \n\
                'y': 'e5jGmxU2VE8If70nC1AbP2rVL18R6Y3tFSDSEb7-E0Q'  \n\
            }                                                       \n\
        ],                                                          \n\
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
                }                                                   \n\
            ],                                                      \n\
            'users': [                                              \n\
                {                                                   \n\
                    'id': 'yuneta',                                 \n\
                    'roles': [                                      \n\
                        'roles^root^users'                          \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        }                                                           \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'authz',                                        \n\
            'gclass': 'C_AUTHZ',                                    \n\
            'priority': 0,                                          \n\
            'autostart': true,                                      \n\
            'autoplay': true                                        \n\
        },                                              \n\
        {                                               \n\
            'name': 'agent22',                          \n\
            'gclass': 'C_AGENT22',                      \n\
            'default_service': true,                    \n\
            'autostart': true,                          \n\
            'autoplay': false                           \n\
        },                                              \n\
        {                                               \n\
            'name': 'controlcenter',                    \n\
            'gclass': 'C_IEVENT_CLI',                   \n\
            'autostart': false,                         \n\
            'autoplay': false,                          \n\
            'kw': {                                     \n\
                'remote_yuno_name': '',                 \n\
                'remote_yuno_role': 'controlcenter',    \n\
                'remote_yuno_service': 'controlcenter', \n\
                'verify_operator': true,                \n\
                'service_principal': 'controlcenter'    \n\
            },                                          \n\
            'children': [                                \n\
                {                                               \n\
                    'name': 'controlcenter_cli',                \n\
                    'gclass': 'C_IOGATE',                       \n\
                    'as_service': true,                         \n\
                    'kw': {                                     \n\
                    },                                          \n\
                    'children': [                               \n\
                        {                                               \n\
                            'name': 'controlcenter_cli',                \n\
                            'gclass': 'C_CHANNEL',                      \n\
                            'kw': {                                     \n\
                            },                                          \n\
                            'children': [                               \n\
                                {                                       \n\
                                    'name': 'controlcenter_cli',        \n\
                                    'gclass': 'C_PROT_TCP4H',           \n\
                                    'children': [                               \n\
                                        {                                       \n\
                                            'name': 'controlcenter_cli',        \n\
                                            'gclass': 'C_TCP',                  \n\
                                            'kw': {                             \n\
                                                'timeout_between_connections': 10000, \n\
                                                'crypto': {                     \n\
                                                'library': '" TLS_LIBRARY_NAME "',   \n\
'ssl_trusted_certificate': '/yuneta/agent/certs/controlcenter.crt',              \n\
'ssl_certificate': '/yuneta/agent/certs/yuneta_agent.crt',                      \n\
'ssl_certificate_key': '/yuneta/agent/certs/yuneta_agent.key',                  \n\
                                                    'ssl_server_name': 'yuneta_agent.yuneta.io', \n\
                                                    'trace': false              \n\
                                                },                              \n\
'url': 'tcps://(^^__sys_machine__^^).(^^__node_owner__^^).(^^__output_url__^^)' \n\
                                            }                                   \n\
                                        }                                       \n\
                                    ]                                           \n\
                                }                                       \n\
                            ]                                           \n\
                        }                                               \n\
                    ]                                   \n\
                }                                       \n\
            ]                                           \n\
        }                                               \n\
    ] \n\
} \n\
";

/***************************************************************************
 *                      Register
 ***************************************************************************/
static int register_yuno_and_more(void)
{
    int result = 0;

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_agent22();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // Samples of traces
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "identity-card", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "identity-card", TRUE);

    // gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_PTY), "traffic", TRUE);

    // Global traces
    // gobj_set_global_trace("create_delete", TRUE);
    // gobj_set_global_trace("machine", TRUE);
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

    return result;
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
        NULL,       // authentication_parser, default Monoclass C_AUTHZ
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
