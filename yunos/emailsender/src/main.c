/****************************************************************************
 *          MAIN_EMAILSENDER.C
 *          emailsender main
 *
 *          Email sender
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_emailsender.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "emailsender"
#define APP_DOC         "Email sender"

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
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['yuneta', 'utils']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
// HACK los hijos en el arbol del servicio emailsender deben llamarse emailsender
// porque las __json_config_variables__ que pone el agente no se insertan en el servicio
// sino en global, como emailsender.__json_config_variables__,
// y como son los hijos quienes reciben las variables, pues se tiene que llamar tb emailsender
// En el futuro debería insertar la variables en el propio tree del servicio.
// Sería más fácil si los servicios estuviesen en un dictionary en vez de un array.
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'console_log_handlers': {                                   \n\
            'to_stdout': {                                          \n\
                'handler_type': 'stdout',                           \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
            'to_file': {                                            \n\
                'handler_type': 'file',                             \n\
                'handler_options': 255,                             \n\
                'filename_mask': 'emailsender-W.log'            \n\
            },                                                      \n\
            'to_udp': {                                             \n\
                'handler_type': 'udp',                              \n\
                'url': 'udp://127.0.0.1:1992',                      \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'required_services': [],                                    \n\
        'public_services': ['emailsender'],                         \n\
        'service_descriptor': {                                         \n\
            'emailsender': {                                            \n\
                'description' : 'Email sender',                         \n\
                'schema' : 'ws',                                        \n\
                'connector' : {                                         \n\
                    'name': 'emailsender',                              \n\
                    'gclass': 'IEvent_cli',                             \n\
                    'autostart': true,                                  \n\
                    'kw': {                                             \n\
                        'remote_yuno_name': '(^^__yuno_name__^^)',      \n\
                        'remote_yuno_role': 'emailsender',              \n\
                        'remote_yuno_service': 'emailsender'            \n\
                    },                                                  \n\
                    'zchilds': [                                        \n\
                        {                                               \n\
                            'name': 'emailsender',                      \n\
                            'gclass': 'IOGate',                         \n\
                            'kw': {                                     \n\
                            },                                          \n\
                            'zchilds': [                                \n\
                                {                                       \n\
                                    'name': 'emailsender',              \n\
                                    'gclass': 'Channel',                \n\
                                    'kw': {                             \n\
                                    },                                  \n\
                                    'zchilds': [                        \n\
                                        {                               \n\
                                            'name': 'emailsender',      \n\
                                            'gclass': 'GWebSocket',     \n\
                                            'kw': {                     \n\
                                                'kw_connex': {          \n\
                                                    'urls':[            \n\
                                                        '(^^__url__^^)' \n\
                                                    ]                   \n\
                                                }                       \n\
                                            }                           \n\
                                        }                               \n\
                                    ]                                   \n\
                                }                                       \n\
                            ]                                           \n\
                        }                                               \n\
                    ]                                                   \n\
                }                                                       \n\
            }                                                           \n\
        },                                                              \n\
        'trace_levels': {                                           \n\
            'Tcp0': ['connections']                                 \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'emailsender',                                  \n\
            'gclass': 'Emailsender',                                \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'zchilds': [                                            \n\
                {                                                   \n\
                    'name': '__input_side__',                       \n\
                    'gclass': 'IOGate',                             \n\
                    'as_service': true,                             \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'zchilds': [                                        \n\
                        {                                               \n\
                            'name': 'emailsender',                      \n\
                            'gclass': 'TcpS0',                          \n\
                            'kw': {                                     \n\
                                'url': '(^^__url__^^)',                 \n\
                                'child_tree_filter': {                  \n\
                                    'op': 'find',                       \n\
                                    'kw': {                                 \n\
                                        '__prefix_gobj_name__': 'wss',      \n\
                                        '__gclass_name__': 'IEvent_srv',    \n\
                                        '__disabled__': false,              \n\
                                        'connected': false                  \n\
                                    }                                       \n\
                                }                                       \n\
                            }                                           \n\
                        }                                               \n\
                    ],                                                  \n\
                    '[^^zchilds^^]': {                                  \n\
                        '__range__': [[0,300]], #^^ max 300 users     \n\
                        '__vars__': {                                   \n\
                        },                                              \n\
                        '__content__': {                                \n\
                            'name': 'wss-(^^__range__^^)',                  \n\
                            'gclass': 'IEvent_srv',                         \n\
                            'kw': {                                         \n\
                            },                                              \n\
                            'zchilds': [                                     \n\
                                {                                               \n\
                                    'name': 'wss-(^^__range__^^)',              \n\
                                    'gclass': 'Channel',                        \n\
                                    'kw': {                                         \n\
                                        'lHost': '(^^__ip__^^)',                    \n\
                                        'lPort': '(^^__port__^^)'                   \n\
                                    },                                              \n\
                                    'zchilds': [                                     \n\
                                        {                                               \n\
                                            'name': 'wss-(^^__range__^^)',              \n\
                                            'gclass': 'GWebSocket',                     \n\
                                            'kw': {                                     \n\
                                                'iamServer': true                       \n\
                                            }                                           \n\
                                        }                                               \n\
                                    ]                                               \n\
                                }                                               \n\
                            ]                                               \n\
                        }                                               \n\
                    }                                                   \n\
                }                                               \n\
            ]                                           \n\
        }                                               \n\
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
    register_c_emailsender();

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
