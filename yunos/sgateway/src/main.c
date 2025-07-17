/****************************************************************************
 *          MAIN_SGATEWAY.C
 *          sgateway main
 *
 *          Simple Gateway
 *
 *  {"command": "-install-binary sgateway id=11 content64=$$(sgateway)"}
 *  {"command": "create-config sgateway.gw1 version=1 id=11"}
 *  {"command": "create-yuno id=11 realm_name=utils yuno_role=sgateway yuno_name=gw1 yuno_tag=utils"}
 *
 *  {"command": "-update-binary id=11 content64=$$(sgateway)"}
 *
 *  command-yuno id=11 service=__root__ command=write-str attribute=input_url value=tcp://0.0.0.0:2011
 *  command-yuno id=11 service=__root__ command=write-str attribute=output_url value=tcp://x.x.x.x:yy
 *
 *  command-yuno id=11 service=__root__ command=view-attrs
 *
 *  command-yuno id=11 service=__yuno__ command=set-gclass-trace gclass_name=Tcp0 level=traffic set=1
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yuneta.h>
#include "c_sgateway.h"
#include "yuno_sgateway.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        ROLE_SGATEWAY
#define APP_DOC         "Simple Gateway"

#define APP_VERSION     __yuneta_version__
#define APP_SUPPORT     "<niyamaka at yuneta.io>"
#define APP_DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"ROLE_SGATEWAY"',                             \n\
        'tags': ['yuneta', 'utils']                                 \n\
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
                'filename_mask': 'sgateway-W.log',              \n\
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
            'TcpS0': ['listen', 'not-accepted', 'accepted']         \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'sgateway',                                     \n\
            'gclass': 'Sgateway',                                   \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
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
static void register_yuno_and_more(void)
{
    /*-------------------*
     *  Register yuno
     *-------------------*/
    register_yuno_sgateway();

    /*--------------------*
     *  Register service
     *--------------------*/
    gobj_register_gclass(GCLASS_SGATEWAY);
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
//     gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "ievents2", TRUE);
//     gobj_set_gclass_trace(GCLASS_IEVENT_SRV, "ievents2", TRUE);
//     gobj_set_gclass_trace(GCLASS_SGATEWAY, "messages", TRUE);

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
