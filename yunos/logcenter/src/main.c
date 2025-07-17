/****************************************************************************
 *          MAIN_LOGCENTER.C
 *          logcenter main
 *
 *          Log Center
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yuneta.h>
#include "c_logcenter.h"
#include "yuno_logcenter.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "logcenter"
#define APP_DOC         "Log Center"

#define APP_VERSION     __yuneta_version__
#define APP_DATETIME    __DATE__ " " __TIME__
#define APP_SUPPORT     "<niyamaka at yuneta.io>"

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': 'logcenter',                                   \n\
        'tags': ['yuneta', 'utils']                                 \n\
    },                                                              \n\
    'environment': {                                                \n\
        'work_dir': '/yuneta',                                      \n\
        'domain_dir': 'realms/agent/logcenter',                     \n\
        'use_system_memory': true,                                  \n\
        'log_gbmem_info': true,                                     \n\
        'MEM_MIN_BLOCK': 64,                                        \n\
        'MEM_MAX_BLOCK': 52428800,              #^^  50*M           \n\
        'MEM_SUPERBLOCK': 52428800,             #^^  50*M           \n\
        'MEM_MAX_SYSTEM_MEMORY': 2147483648,     #^^ 2*G            \n\
        'console_log_handlers': {                                   \n\
            'to_stdout': {                                          \n\
                'handler_type': 'stdout',                           \n\
                'handler_options': 255                              \n\
            }                                                       \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
        }                                                           \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'required_services': ['emailsender'],                       \n\
        'trace_levels': {                                           \n\
            'Tcp0': ['connections']                                 \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'logcenter',                                    \n\
            'gclass': 'Logcenter',                                  \n\
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
    register_yuno_logcenter();

    /*--------------------*
     *  Register service
     *--------------------*/
    gobj_register_gclass(GCLASS_LOGCENTER);
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

//     gobj_set_gclass_trace(GCLASS_TCP0, "traffic", TRUE);

//     gobj_set_gclass_trace(GCLASS_IEVENT_CLI, "ievents2", TRUE);
//     gobj_set_gclass_trace(GCLASS_IEVENT_SRV, "ievents2", TRUE);
//     gobj_set_gclass_no_trace(GCLASS_TIMER, "machine", TRUE);

//      gobj_set_gclass_trace(GCLASS_TCP0, "traffic", TRUE);

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
