/****************************************************************************
 *          MAIN_SANIKIDB.C
 *          sanikidb main
 *
 *          DBA Sanikidb
 *
 *          Saniki del aire
 *
 *          Copyright (c) 2024 by ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test_timer.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_timer"
#define APP_DOC         "Test C_TIMER"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define DEBUG_MEMORY            FALSE
#define MEM_MIN_BLOCK           0       // use default
#define MEM_MAX_BLOCK           0       // use default
#define MEM_SUPERBLOCK          0       // use default
#define MEM_MAX_SYSTEM_MEMORY   0       // use default

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['saniki', 'dbas']                                  \n\
    }                                                               \n\
}                                                                   \n\
";
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
                'filename_mask': 'saniki2db-W.log',                 \n\
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
        'i18n_dirname': '/yuneta/share/locale/',                    \n\
        'i18n_domain': 'sanikidb',                                  \n\
        'trace_levels': {                                           \n\
            'Tcp0': ['connections'],                                \n\
            'TcpS0': ['listen', 'not-accepted', 'accepted'],        \n\
            'Tcp1': ['connections'],                                \n\
            'TcpS1': ['listen', 'not-accepted', 'accepted']         \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'saniki2db',                                    \n\
            'gclass': 'C_SANIKI2DB',                                \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'zchilds': [                                            \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'authz',                                        \n\
            'gclass': 'C_AUTHZ',                                    \n\
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
static void register_yuno_and_more(void)
{
    /*--------------------*
     *  Register service
     *--------------------*/
    register_c_test_timer();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("periodic_timer", TRUE);
    gobj_set_global_no_trace("timer", TRUE);

    // Minimum trace: connections
    gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "connections", TRUE);
    gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", TRUE);

    // Minimum trace: login/logout
    gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "identity-card", TRUE);
    gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "identity-card", TRUE);

    // Samples of traces
    // gobj_set_gclass_trace(gclass_find_by_name(C_SANIKI2DB), "messages", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "ievents2", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "ievents2", TRUE);

    // Samples of global traces
    // gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
    // gobj_set_gobj_trace(0, "create_delete2", TRUE, 0);
    // gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
    // gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
    // gobj_set_gobj_trace(0, "machine", TRUE, 0);
    // gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
    // gobj_set_gobj_trace(0, "libuv", TRUE, 0);
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {2130,0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *      To check
     *------------------------------------------------*/
    // gobj_set_deep_tracing(1);
    // set_auto_kill_time(6);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // startup_persistent_attrs, default internal dbsimple
        NULL,       // end_persistent_attrs,        "
        NULL,       // load_persistent_attrs,       "
        NULL,       // save_persistent_attrs,       "
        NULL,       // remove_persistent_attrs,     "
        NULL,       // list_persistent_attrs,       "
        NULL,       // command_parser, default internal command_parser
        NULL,       // stats_parser, default internal stats_parser
        NULL,       // authz_checker, default Monoclass C_AUTHZ
        NULL,       // authenticate_parser, default Monoclass C_AUTHZ
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_MAX_BLOCK,
        MEM_SUPERBLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        DEBUG_MEMORY
    );
    return yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more
    );
}
