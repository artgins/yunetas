/****************************************************************************
 *          MAIN_YUNETA_CLI.C
 *          yuneta_cli main
 *
 *          Copyright (c) 2014,2015 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_cli.h"
#include "c_wn_stdscr.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "yuneta"
#define APP_DOC         "Yuneta Command Line Interface"

#define APP_VERSION     YUNETA_VERSION
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<support at artgins.com>"

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'realm_owner': 'agent',                                     \n\
        'work_dir': '/yuneta',                                      \n\
        'domain_dir': 'realms/agent/cli'                            \n\
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
    },                                                              \n\
    'environment': {                                                \n\
        'use_system_memory': true,                                  \n\
        'log_gbmem_info': true,                                     \n\
        'MEM_MIN_BLOCK': 512,                                       \n\
        'MEM_MAX_BLOCK': 209715200,             #^^  200*M          \n\
        'MEM_SUPERBLOCK': 209715200,            #^^  200*M          \n\
        'MEM_MAX_SYSTEM_MEMORY': 2147483648,     #^^ 2*G            \n\
        'console_log_handlers': {                                   \n\
#^^            'to_stdout': {                                       \n\
#^^                'handler_type': 'stdout'                         \n\
#^^            },                                                   \n\
            'to_udp': {                                             \n\
                'handler_type': 'udp',                              \n\
                'url': 'udp://127.0.0.1:1992',                      \n\
                'handler_options': 255                              \n\
            },                                                      \n\
            'to_file': {                                            \n\
                'handler_type': 'file',                             \n\
                'filename_mask': 'yuneta_cli-W.log',            \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'timeout': 200,                                             \n\
        'trace_levels': {                                           \n\
            'Tcp0': ['connections']                                 \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        'Cli.shortkeys': {                                                      \n\
            's': 'stats-yuno yuno_role=logcenter',                              \n\
            'ss': 'command-yuno yuno_role=logcenter command=display-summary',   \n\
            'r': 'command-yuno yuno_role=logcenter command=reset-counters',     \n\
            'tt': 't yuno_running=1',                                           \n\
            'error': 'command-yuno yuno_role=logcenter command=search text=\\\"$1\\\"' \n\
        }                                                                       \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'cli',                                          \n\
            'gclass': 'Cli',                                        \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            }                                                       \n\
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
    register_c_cli();

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

    // gobj_set_gclass_trace(gclass_find_by_name(C_TEST4), "messages", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_TEST4), "machine", TRUE);
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
    static uint32_t mem_list[] = {0, 0};
    gbmem_trace_alloc_free(0, mem_list);
#endif

    /*
     *  Estas trazas siempre en el cli.
     */
    gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "identity-card", TRUE);
    gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "identity-card", TRUE);

    if(argv[1]) {
        if(strcmp(argv[1], "verbose2")==0) {
            gobj_set_gobj_trace(0, "machine", TRUE, 0);
            gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "ievents2", TRUE);
            gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
            gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
            argc = 1;
        } else if(strcmp(argv[1], "verbose3")==0) {
            gobj_set_gobj_trace(0, "machine", TRUE, 0);
            gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
            gobj_set_gobj_trace(0, "create_delete2", TRUE, 0);
            gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
            gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
            gobj_set_gobj_trace(0, "libuv", TRUE, 0);
            gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
            gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "ievents2", TRUE);
            gobj_set_gclass_trace(GCLASS_CLI, "trace-kb", TRUE);
            argc = 1;
        } else if(strcmp(argv[1], "verbose")==0) {
            gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "ievents2", TRUE);
            gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
            argc = 1;
        }
    }

//     gobj_set_gclass_trace(GCLASS_CLI, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_CLI, "ev_kw", TRUE);
//     gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "machine", TRUE);
//     gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "ev_kw", TRUE);
//     gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "ievents", TRUE);
//     gobj_set_gclass_trace(GCLASS_IOGATE, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_IOGATE, "ev_kw", TRUE);
//     gobj_set_gclass_trace(GCLASS_CHANNEL, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_CHANNEL, "ev_kw", TRUE);
//     gobj_set_gclass_trace(GCLASS_GWEBSOCKET, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_GWEBSOCKET, "ev_kw", TRUE);
//     gobj_set_gclass_trace(GCLASS_CONNEX, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_CONNEX, "ev_kw", TRUE);

//     gobj_set_gclass_trace(GCLASS_WN_LIST, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_WN_LIST, "ev_kw", TRUE);

//     gobj_set_gclass_trace(GCLASS_WN_STSLINE, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_WN_STSLINE, "ev_kw", TRUE);

//     gobj_set_gclass_trace(GCLASS_WN_EDITLINE, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_WN_EDITLINE, "ev_kw", TRUE);

//     gobj_set_gclass_trace(GCLASS_ROUTER, "machine", TRUE);
//     gobj_set_gclass_trace(GCLASS_ROUTER, "ev_kw", TRUE);

//     gobj_set_gobj_trace(0, "machine", TRUE, 0);
//     gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
//     gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
//     gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
//     gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);

//     gobj_set_gclass_trace(GCLASS_CLI, "trace-kb", TRUE);
//     gobj_set_gclass_trace(GCLASS_PTY, "traffic", TRUE);

    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);

    //set_auto_kill_time(5);

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
