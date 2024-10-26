/****************************************************************************
 *          MAIN_PEPON.C
 *          pepon main
 *
 *          Pepon, yuno server de pruebas
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yuneta_tls.h>
#include "c_pepon.h"
#include "yuno_pepon.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        ROLE_PEPONS
#define APP_DOC         "Pepon, yuno server de pruebas"

#define APP_VERSION     __yuneta_version__
#define APP_SUPPORT     "<niyamaka@yuneta.io>"
#define APP_DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"ROLE_PEPONS"',                               \n\
        'tags': ['app', 'test']                                     \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'use_system_memory': true,                                  \n\
        'log_gbmem_info': true,                                     \n\
        'MEM_MIN_BLOCK': 32,                                        \n\
        'MEM_MAX_BLOCK': 65536,             #^^ 64*K                \n\
        'MEM_SUPERBLOCK': 131072,           #^^ 128*K               \n\
        'MEM_MAX_SYSTEM_MEMORY': 1048576,   #^^ 1*M                 \n\
        'console_log_handlers': {                                   \n\
            'to_stdout': {                                          \n\
                'handler_type': 'stdout',                           \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
            'to_file': {                                            \n\
                'handler_type': 'file',                             \n\
                'filename_mask': 'pepon-W.log',                 \n\
                'handler_options': 255                              \n\
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
        'public_services': [],                                      \n\
        'service_descriptor': {                                     \n\
        },                                                          \n\
        'trace_levels': {                                           \n\
            'Tcp0': ['connections'],                                \n\
            'TcpS0': ['listen', 'not-accepted', 'accepted'],        \n\
            'Tcp1': ['connections'],                                \n\
            'TcpS1': ['listen', 'not-accepted', 'accepted']         \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        '__input_side__.__json_config_variables__': {               \n\
            '__input_url__': 'tcps://0.0.0.0:7778',                 \n\
            '__input_host__': '0.0.0.0',                            \n\
            '__input_port__': '7778'                                \n\
        }                                                           \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'pepon',                                        \n\
            'gclass': 'Pepon',                                      \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'zchilds': [                                            \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'IOGate',                                     \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'zchilds': [                                            \n\
                {                                                   \n\
                    'name': 'server_port',                          \n\
                    'gclass': 'TcpS1',                              \n\
                    'kw': {                                         \n\
                        'crypto': {                                 \n\
                            'library': 'openssl',                   \n\
'ssl_certificate': '/yuneta/development/yuneta/^yuneta/tests-y/certs/yuneta.crt',\n\
'ssl_certificate_key': '/yuneta/development/yuneta/^yuneta/tests-y/certs/yuneta.key',\n\
                            'trace': false                           \n\
                        },                                          \n\
                        'url': '(^^__input_url__^^)',               \n\
                        'child_tree_filter': {                      \n\
                            'op': 'find',                           \n\
                            'kw': {                                 \n\
                                '__prefix_gobj_name__': '(^^__input_port__^^)-', \n\
                                '__gclass_name__': 'Channel',       \n\
                                '__disabled__': false,              \n\
                                'connected': false,                 \n\
                                'lHost': '(^^__input_host__^^)',    \n\
                                'lPort': '(^^__input_port__^^)'     \n\
                            }                                       \n\
                        }                                           \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^zchilds^^]': {                                      \n\
                '__range__': [[1,1]],                               \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': '(^^__input_port__^^)-(^^__range__^^)', \n\
                    'gclass': 'Channel',                            \n\
                    'kw': {                                         \n\
                        'lHost': '(^^__input_host__^^)',            \n\
                        'lPort': '(^^__input_port__^^)'             \n\
                    },                                              \n\
                    'zchilds': [                                    \n\
                        {                                           \n\
                            'name': '(^^__input_port__^^)-(^^__range__^^)', \n\
                            'gclass': 'Prot_header4',               \n\
                            'kw': {                                 \n\
                            }                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *                      Register
 ***************************************************************************/
static void register_yuno_and_more(void)
{
    /*------------------------*
     *  Register yuneta-tls
     *------------------------*/
    yuneta_register_c_tls();

    /*-------------------*
     *  Register yuno
     *-------------------*/
    register_yuno_pepon();

    /*--------------------*
     *  Register service
     *--------------------*/
    gobj_register_gclass(GCLASS_PEPON);
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------------------------*
     *  To trace memory
     *------------------------------------------------*/
#ifdef DEBUG
    static uint32_t mem_list[] = {0};
    gbmem_trace_alloc_free(0, mem_list);
#endif

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(GCLASS_TIMER, "machine", TRUE);  // Avoid timer trace, too much information

// Samples
//     gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "ievents", TRUE);
//     gobj_set_gclass_trace(GCLASS_IEVENT_SRV, "ievents", TRUE);
//     gobj_set_gclass_trace(GCLASS_PEPON, "messages", TRUE);

//     gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
//     gobj_set_gobj_trace(0, "create_delete2", TRUE, 0);
//     gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
//     gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
//     gobj_set_gobj_trace(0, "machine", TRUE, 0);
//     gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
//     gobj_set_gobj_trace(0, "libuv", TRUE, 0);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_set_gobj_startup_functions(
        db_load_persistent_attrs,   // dbsimple.c
        db_save_persistent_attrs,   // dbsimple.c
        db_remove_persistent_attrs, // dbsimple.c
        db_list_persistent_attrs,   // dbsimple.
        command_parser,             // command_parser.c
        stats_parser                // stats_parser.c
    );
    return yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more
    );
}
