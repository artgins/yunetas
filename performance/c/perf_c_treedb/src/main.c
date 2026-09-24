/****************************************************************************
 *          MAIN.C
 *
 *          Main of perf_c_treedb
 *          Benchmark of the open of a dynamic-schema treedb by C_TREEDB,
 *          in a store of many treedbs (see c_perf_treedb_open.c)
 *
 *          Every result is one line of JSON on stdout:
 *            {"bench": "perf_c_treedb", "case": "<phase>",
 *             "seconds": <s>, "ops": <opens>, "us_per_op": <us>, ...}
 *          The first line says the version and the build. Another size,
 *          through a config file (small.json is the one ctest runs):
 *            perf_c_treedb --config-file=small.json
 *          An ERROR in the log (an open that failed) makes it exit -1.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include <yuneta_version.h>
#include <yuneta_config.h>
#include "c_perf_treedb_open.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "perf_c_treedb"
#define APP_DOC         "Benchmark of the open of a treedb by C_TREEDB"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define DEBUG_MEMORY            FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   (2LL*1024*1024*1024)    // the store of 40 treedbs is in memory

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['performance', 'yunetas']                          \n\
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
        'service_descriptor': {                                     \n\
        },                                                          \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'perf_treedb_open',                             \n\
            'gclass': 'C_PERF_TREEDB_OPEN',                         \n\
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

/***************************************************************************
 *  The benchmark drives C_TREEDB through its commands, and those are
 *  guarded by authzs. Without an authz service the default checker denies
 *  them, so the benchmark supplies its own: everyone may.
 ***************************************************************************/
PRIVATE BOOL perf_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
}

/***************************************************************************
 *  The ERRORs of the run. Not the watcher of a store that inotify cannot
 *  give (its limit of instances): the open goes on without it.
 ***************************************************************************/
PRIVATE int errors_logged = 0;

PRIVATE int counting_log_write(void *h, int priority, const char *bf, size_t len)
{
    if(strstr(bf, "\"inotify_init1() FAILED\"") ||
            strstr(bf, "\"fs_create_watcher_event FAILED\"")) {
        return 0;
    }
    if(priority <= LOG_ERR) {
        errors_logged++;
    }
    return 0;
}

/***************************************************************************
 *                      Register
 ***************************************************************************/
static int register_yuno_and_more(void)
{
    int result = 0;

    /*--------------------------------------*
     *  A realm for the persistent attrs,
     *  beside the store of the benchmark
     *--------------------------------------*/
    char root_dir[PATH_MAX];
    build_path(root_dir, sizeof(root_dir), getenv("HOME"), "tests_yuneta", NULL);
    register_yuneta_environment(root_dir, APP_NAME, 02770, 0660);

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_perf_treedb_open();

#ifdef CONFIG_DEBUG_TRACK_MEMORY
    int track_memory = 1;
#else
    int track_memory = 0;
#endif
    printf("{\"bench\": \"%s\", \"yuneta_version\": \"%s\", \"track_memory\": %d}\n",
        APP_NAME, YUNETA_VERSION, track_memory
    );
    fflush(stdout);

    return result;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);
    gobj_log_register_handler("counting", 0, counting_log_write, 0);
    gobj_log_add_handler("perf_counting", "counting", LOG_OPT_UP_ERROR, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs
        NULL,       // command_parser
        NULL,       // stats_parser
        perf_authz_checker,
        NULL,       // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    int result = yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        NULL
    );

    if(get_cur_system_memory() != 0) {
        printf("{\"bench\": \"%s\", \"error\": \"system memory not free\"}\n", APP_NAME);
        print_track_mem();
        result = -1;
    }
    if(errors_logged > 0) {
        printf("{\"bench\": \"%s\", \"error\": \"%d errors logged\"}\n", APP_NAME, errors_logged);
        result = -1;
    }
    if(result < 0) {
        printf("{\"bench\": \"%s\", \"result\": \"FAILED\"}\n", APP_NAME);
    }
    return result < 0? -1 : 0;
}
