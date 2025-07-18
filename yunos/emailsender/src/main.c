/****************************************************************************
 *          MAIN_EMAILSENDER.C
 *          emailsender main
 *
 *          Email sender
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yuneta.h>
#include "c_emailsender.h"
#include "yuno_emailsender.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "emailsender"
#define APP_DOC         "Email sender"

#define APP_VERSION     YUNETA_VERSION
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<support at artgins.com>"

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': 'emailsender',                                 \n\
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
static void register_yuno_and_more(void)
{
    /*-------------------*
     *  Register yuno
     *-------------------*/
    register_yuno_emailsender();

    /*--------------------*
     *  Register service
     *--------------------*/
    gobj_register_gclass(GCLASS_CURL);
    gobj_register_gclass(GCLASS_EMAILSENDER);
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
    static uint32_t mem_list[] = {0, 0};
    gbmem_trace_alloc_free(0, mem_list);
#endif

//     gobj_set_gobj_trace(0, "machine", TRUE, 0);
//     gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
//     gobj_set_gobj_trace(0, "uv", TRUE, 0);

//     gobj_set_gclass_trace(GCLASS_TCP0, "traffic", TRUE);
//
//     gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "ievents2", TRUE);
//     gobj_set_gclass_trace(GCLASS_IEVENT_SRV, "ievents2", TRUE);
//     gobj_set_gclass_no_trace(GCLASS_TIMER, "machine", TRUE);

//     gobj_set_gobj_trace(0, "machine", TRUE, 0);
//     gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
//     gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
//     gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
//     gobj_set_gobj_trace(0, "create_delete2", TRUE, 0);
//     gobj_set_gobj_trace(0, "start_stop", TRUE, 0);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        dbattrs_startup,
        dbattrs_end,
        dbattrs_load_persistent,
        dbattrs_save_persistent,
        dbattrs_remove_persistent,
        dbattrs_list_persistent,
        0,
        0,
        0,
        0
    );
    return yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more
    );
}
