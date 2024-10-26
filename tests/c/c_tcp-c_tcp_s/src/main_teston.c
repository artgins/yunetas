/****************************************************************************
 *          MAIN_TESTON.C
 *          teston main
 *
 *          Teston, yuno client de pruebas
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yuneta_tls.h>
#include "c_teston.h"
#include "yuno_teston.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        ROLE_TESTONS
#define APP_DOC         "Teston, yuno client de pruebas"

#define APP_VERSION     __yuneta_version__
#define APP_SUPPORT     "<niyamaka@yuneta.io>"
#define APP_DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"ROLE_TESTONS"',                              \n\
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
                'filename_mask': 'teston-W.log',                \n\
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
            'Tcp1': ['connections'],                     \n\
            'TcpS1': ['listen', 'not-accepted', 'accepted']         \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'teston',                                       \n\
            'gclass': 'Teston',                                     \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'zchilds': [                                            \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__output_side__',                              \n\
            'gclass': 'IOGate',                                     \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'zchilds': [                                            \n\
                {                                                   \n\
                    'name': 'output',                               \n\
                    'gclass': 'Channel',                            \n\
                    'zchilds': [                                    \n\
                        {                                           \n\
                            'name': 'output',                       \n\
                            'gclass': 'Prot_header4',               \n\
                            'zchilds': [                            \n\
                                {                                   \n\
                                    'name': 'output',               \n\
                                    'gclass': 'Connexs',            \n\
                                    'kw': {                         \n\
                                        'crypto': {                 \n\
                                            'library': 'openssl',   \n\
                                            'trace': false           \n\
                                        },                          \n\
                                        'urls':[                    \n\
                                            'tcps://127.0.0.1:7778' \n\
                                        ]                           \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
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
    register_yuno_teston();

    /*--------------------*
     *  Register service
     *--------------------*/
    gobj_register_gclass(GCLASS_TESTON);
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
//     gobj_set_gclass_trace(GCLASS_TESTON, "messages", TRUE);

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
