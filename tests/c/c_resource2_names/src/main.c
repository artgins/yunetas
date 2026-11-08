/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_resource2_names: a persistent C_RESOURCE2 takes
 *          a resource name only when it is one file name
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test_resource2_names.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_resource2_names"
#define APP_DOC         "Test the resource names a persistent C_RESOURCE2 takes"

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
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'console_log_handlers': {                                   \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'timeout_periodic': 100,                                    \n\
        'service_descriptor': {                                     \n\
        },                                                          \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'test_resource2_names',                         \n\
            'gclass': 'C_TEST_RESOURCE2_NAMES',                     \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                           \n\
            ]                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

time_measure_t time_measure;

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
int result = 0;

static int register_yuno_and_more(void)
{
    int result = 0;

    /*--------------------------------------*
     *  The store of the resources, inside
     *  the directory the driver wipes
     *--------------------------------------*/
    char root_dir[PATH_MAX];
    build_path(root_dir, sizeof(root_dir), getenv("HOME"), "tests_yuneta", NULL);
    register_yuneta_environment(root_dir, "c_resource2_names", 02770, 0660);

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_test_resource2_names();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // gobj_set_gclass_trace(gclass_find_by_name(C_RESOURCE2), "messages", TRUE);
    // gobj_set_gobj_trace(0, "machine", TRUE, 0);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]", // errors_list
            "msg", "Starting yuno",
            "msg", "Playing yuno",
            "msg", "Resource name refused: not one file name (a '/', a leading '.', '..', too long)",
            "msg", "Resource name refused: not one file name (a '/', a leading '.', '..', too long)",
            "msg", "Resource name refused: not one file name (a '/', a leading '.', '..', too long)",
            "msg", "Resource name refused: not one file name (a '/', a leading '.', '..', too long)",
            "msg", "Resource name refused: not one file name (a '/', a leading '.', '..', too long)",
            "msg", "TEST: a persistent C_RESOURCE2 takes only names that are one file name",
            "msg", "Exit to die",
            "msg", "Pausing yuno",
            "msg", "Yuno stopped, gobj end"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    MT_START_TIME(time_measure)

    return result;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment
 *  AFTER the yuno has stopped
 ***************************************************************************/
static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)
    result += test_json(NULL);  //  NULL: we want to check only the logs
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    glog_init();

    /*
     *  Add all handlers very early
     */
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *      To check
     *------------------------------------------------*/
    set_auto_kill_time(10);

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

    result += yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        cleaning
    );

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP_NAME);
    }
    return result<0?-1:0;
}
