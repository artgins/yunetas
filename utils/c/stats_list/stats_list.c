/****************************************************************************
 *          STATS_LIST.C
 *
 *          List stats
 *
 *          Copyright (c) 2018 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <regex.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>

#include <yunetas.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "stats_list"
#define DOC         "List statistics."

#define VERSION     YUNETA_VERSION
#define SUPPORT     "<support at artgins.com>"
#define DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *              Structures
 ***************************************************************************/
/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 0
#define MAX_ARGS 0
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */

    char *path;
    char *group;
    int recursive;
    int raw;
    int verbose;
    int limits;

    char *from_t;
    char *to_t;

    char *variable;
    char *metric;
    char *units;
};

typedef struct {
    char path_simple_stats[PATH_MAX];
    struct arguments *arguments;
    json_t *match_cond;
} list_params_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);
PRIVATE void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
struct arguments arguments;
int total_counter = 0;
int partial_counter = 0;
const char *argp_program_version = NAME " " VERSION;
const char *argp_program_bug_address = SUPPORT;
yev_loop_h yev_loop;
int time2exit = 10;

/* Program documentation. */
static char doc[] = DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-----------------key-----arg-----------------flags---doc-----------------group */
{0,                     0,      0,                  0,      "Database",         2},
{"path",                'a',    "PATH",             0,      "Path.",            2},
{"group",               'b',    "GROUP",            0,      "Group.",           2},
{"recursive",           'r',    0,                  0,      "List recursively.",2},
{"raw",                 10,     0,                  0,      "List rawly.",      2},

{0,                     0,      0,                  0,      "Presentation",     3},
{"verbose",             'l',    0,                  0,      "Verbose",          3},

{0,                     0,      0,                  0,      "Search conditions", 4},
{"from-t",              1,      "TIME",             0,      "From time.",       4},
{"to-t",                2,      "TIME",             0,      "To time.",         4},

{"limits",              3,      0,                  0,      "Show limits",      5},

{"variable",            21,     "VARIABLE",         0,      "Variable.",        9},
{"metric",              22,     "METRIC",           0,      "Metric.",          10},
{"units",               23,     "UNITS",            0,      "Units (SEC, MIN, HOUR, MDAY, MON, YEAR, WDAY, YDAY, CENT).",          11},

{0}
};

/* Our argp parser. */
static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc,
    0,
    0,
    0
};

/***************************************************************************
 *  Parse a single option
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    /*
     *  Get the input argument from argp_parse,
     *  which we know is a pointer to our arguments structure.
     */
    struct arguments *arguments = state->input;

    switch (key) {
    case 'a':
        arguments->path= arg;
        break;
        break;
    case 'b':
        arguments->group= arg;
        break;
    case 'r':
        arguments->recursive = 1;
        break;
    case 10:
        arguments->raw = 1;
        break;
    case 'l':
        arguments->verbose = 1;
        break;

    case 1: // from_t
        arguments->from_t = arg;
        break;
    case 2: // to_t
        arguments->to_t = arg;
        break;

    case 3:
        arguments->limits = 1;
        break;

    case 21:
        arguments->variable = arg;
        break;
    case 22:
        arguments->metric = arg;
        break;
    case 23:
        arguments->units = arg;
        break;

    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            /* Too many arguments. */
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;

    case ARGP_KEY_END:
        if (state->arg_num < MIN_ARGS) {
            /* Not enough arguments. */
            argp_usage (state);
        }
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// TODO review what this functions do
// /***************************************************************************
//  *
//  ***************************************************************************/
// PRIVATE void print_summary(json_t *metrics)
// {
//     const char *var;
//     json_t *jn_var;
//     json_object_foreach(metrics, var, jn_var) {
//         printf("Variable: %s\n", var);
//
//         const char *metric;
//         json_t *jn_metric;
//         json_object_foreach(jn_var, metric, jn_metric) {
//             printf("    Metric-Id: %s\n", metric);
//             printf("    Period: %s\n", kw_get_str(0, jn_metric, "period", "", KW_REQUIRED));
//             printf("    Units: %s\n", kw_get_str(0, jn_metric, "units", "", KW_REQUIRED));
//             printf("    Compute: %s\n", kw_get_str(0, jn_metric, "compute", "", KW_REQUIRED));
//             json_t *jn_data = kw_get_list(0, jn_metric, "data", 0, KW_REQUIRED);
//             if(json_array_size(jn_data)) {
//                 int items = json_array_size(jn_data);
//                 json_t *jn_first = json_array_get(jn_data, 0);
//                 json_t *jn_last = json_array_get(jn_data, items-1);
//                 printf("        From %s\n",
//                     kw_get_str(0, jn_first, "fr_d", "", KW_REQUIRED)
//                 );
//                 printf("        To   %s\n",
//                     kw_get_str(0, jn_last, "to_d", "", KW_REQUIRED)
//                 );
//             }
//         }
//     }
// }
//
// /***************************************************************************
//  *
//  ***************************************************************************/
// PRIVATE void print_keys(json_t *metrics)
// {
//     const char *key;
//     json_t *jn_value;
//     json_object_foreach(metrics, key, jn_value) {
//         printf("        %s\n", key);
//     }
// }
//
// /***************************************************************************
//  *
//  ***************************************************************************/
// PRIVATE void print_units(json_t *variable)
// {
//     const char *key;
//     json_t *jn_value;
//     json_object_foreach(variable, key, jn_value) {
//         printf("        %s\n", json_string_value(json_object_get(jn_value, "units")));
//     }
//
// }
//
// /***************************************************************************
//  *
//  ***************************************************************************/
// PRIVATE void print_limits(json_t *jn_metric)
// {
//     json_t *jn_data = kw_get_list(0, jn_metric, "data", 0, KW_REQUIRED);
//     if(json_array_size(jn_data)) {
//         int items = json_array_size(jn_data);
//         json_t *jn_first = json_array_get(jn_data, 0);
//         json_t *jn_last = json_array_get(jn_data, items-1);
//         printf("        From %s\n",
//             kw_get_str(0, jn_first, "fr_d", "", KW_REQUIRED)
//         );
//         printf("        To   %s\n",
//             kw_get_str(0, jn_last, "to_d", "", KW_REQUIRED)
//         );
//     }
// }

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _list_stats(
    char *path,
    char *group_name,
    char *metric_name_,
    json_t *match_cond,
    int verbose
)
{
    // TODO: rstats API not yet ported to Yunetas V7
    fprintf(stderr, "rstats API not yet available in V7\n");
    return -1;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_metric_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    pop_last_segment(fullpath); // remove __simple_stats__.json
    char *metric = pop_last_segment(fullpath);
    if(level>2) {
        char *group = pop_last_segment(fullpath);
        printf("  Group  ==> '%s'\n", group);
    }
    printf("  Metric ==> '%s'\n", metric);

    return TRUE; // to continue
}

PRIVATE int list_metrics(const char *path)
{
    walk_dir_tree(
        0,
        path,
        "__metric__\\.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_metric_cb,
        0
    );
    printf("\n");
    return 0;
}

PRIVATE BOOL list_db_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    pop_last_segment(fullpath); // remove __simple_stats__.json
    printf("Stats database ==> '%s'\n", fullpath);
    list_metrics(fullpath);

    return TRUE; // to continue
}

PRIVATE int list_databases(const char *path)
{
    walk_dir_tree(
        0,
        path,
        "__simple_stats__\\.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_db_cb,
        0
    );
    printf("\n");
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_stats(list_params_t *list_params)
{
    if(!file_exists(list_params->arguments->path, "__simple_stats__.json")) {
        if(!is_directory(list_params->arguments->path)) {
            fprintf(stderr, "Path not found: '%s'\n\n", list_params->arguments->path);
            exit(-1);
        }
        fprintf(stderr, "What Stats Database?\n\n");
        list_databases(list_params->arguments->path);
        exit(-1);
    }

    snprintf(list_params->path_simple_stats, sizeof(list_params->path_simple_stats),
        "%s", list_params->arguments->path
    );
    pop_last_segment(list_params->path_simple_stats); // remove __simple_stats__.json

    char temp[PATH_MAX];
    build_path(temp, sizeof(temp), list_params->path_simple_stats, "", NULL);

    if(!empty_string(list_params->arguments->group)) {
        if(!subdir_exists(list_params->path_simple_stats, list_params->arguments->group)) {
            fprintf(stderr, "Group not found: '%s'\n\n", list_params->arguments->group);
            exit(-1);
        }
        build_path(temp, sizeof(temp), list_params->path_simple_stats, list_params->arguments->group, NULL);
    }

    return _list_stats(
        list_params->path_simple_stats,
        list_params->arguments->group,
        list_params->arguments->metric,
        list_params->match_cond,
        list_params->arguments->verbose
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_recursive_groups_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // name of type found
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    list_params_t *list_params = user_data;

    pop_last_segment(fullpath); // remove __metric__.json
    char *metric_name = pop_last_segment(fullpath);
    if(empty_string(list_params->arguments->group)) {
        if(level > 2) {
            list_params->arguments->group = pop_last_segment(fullpath);
        }
    }

    _list_stats(
        list_params->path_simple_stats,
        list_params->arguments->group,
        metric_name,
        list_params->match_cond,
        list_params->arguments->verbose
    );

    return TRUE; // to continue
}

PRIVATE int list_recursive_groups(list_params_t *list_params)
{
    walk_dir_tree(
        0,
        list_params->arguments->path,
        ".*\\__metric__\\.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_recursive_groups_cb,
        list_params
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_recursive_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // name of type found
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    list_params_t *list_params = user_data;

    pop_last_segment(fullpath); // remove __simple_stats__.json
    list_params->arguments->path = fullpath;
    snprintf(list_params->path_simple_stats, sizeof(list_params->path_simple_stats), "%s", fullpath);
    list_recursive_groups(list_params);

    return TRUE; // to continue
}

PRIVATE int list_recursive(list_params_t *list_params)
{
    walk_dir_tree(
        0,
        list_params->arguments->path,
        ".*\\__simple_stats__\\.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_recursive_cb,
        list_params
    );

    return 0;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*
     *  Default values
     */
    memset(&arguments, 0, sizeof(arguments));

    /*
     *  Parse arguments
     */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

#ifndef CONFIG_BUILD_TYPE_RELEASE
    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);
#endif

    uint64_t MEM_MAX_SYSTEM_MEMORY = free_ram_in_kb() * 1024LL;
    MEM_MAX_SYSTEM_MEMORY /= 100LL;
    MEM_MAX_SYSTEM_MEMORY *= 90LL;  // Coge el 90% de la memoria
    uint64_t MEM_MAX_BLOCK = (MEM_MAX_SYSTEM_MEMORY / sizeof(md2_record_ex_t)) * sizeof(md2_record_ex_t);
    MEM_MAX_BLOCK = MIN(1*1024*1024*1024LL, MEM_MAX_BLOCK);  // 1*G max

    gbmem_setup(
        MEM_MAX_BLOCK,  // max_block, largest memory block
        MEM_MAX_SYSTEM_MEMORY, // max_system_memory, maximum system memory
        FALSE,
        0,
        0
    );

    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL  // global_authentication_parser
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

    /*----------------------------------*
     *  Match conditions
     *----------------------------------*/
    json_t *match_cond = json_object();

    if(arguments.from_t) {
        timestamp_t timestamp;
        if(all_numbers(arguments.from_t)) {
            timestamp = atoll(arguments.from_t);
        } else {
            timestamp = approxidate(arguments.from_t);
        }
        json_object_set_new(match_cond, "from_t", json_integer(timestamp));
    }
    if(arguments.to_t) {
        timestamp_t timestamp;
        if(all_numbers(arguments.to_t)) {
            timestamp = atoll(arguments.to_t);
        } else {
            timestamp = approxidate(arguments.to_t);
        }
        json_object_set_new(match_cond, "to_t", json_integer(timestamp));
    }

    if(arguments.variable) {
        json_object_set_new(
            match_cond,
            "variable",
            json_string(arguments.variable)
        );
    }
    if(arguments.units) {
        json_object_set_new(
            match_cond,
            "units",
            json_string(arguments.units)
        );
    }
    if(arguments.limits) {
        json_object_set_new(
            match_cond,
            "show_limits",
            json_true()
        );
    }

    /*------------------------*
     *      Do your work
     *------------------------*/
    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    if(empty_string(arguments.path)) {
        fprintf(stderr, "What Statistics path?\n");
        fprintf(stderr, "You must supply --path (-a) option\n\n");
        exit(-1);
    }

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        if(rl.rlim_cur < 200000) {
            rl.rlim_cur = 200000;
            rl.rlim_max = 200000;
            setrlimit(RLIMIT_NOFILE, &rl);
        }
    }

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        NULL,
        2024,
        10,
        NULL,
        &yev_loop
    );

    list_params_t list_params;
    memset(&list_params, 0, sizeof(list_params));
    list_params.arguments = &arguments;
    list_params.match_cond = match_cond;

    if(arguments.raw) {
        _list_stats(
            arguments.path,
            arguments.group,
            arguments.metric,
            match_cond,
            arguments.verbose
        );
    } else if(arguments.recursive) {
        list_recursive(&list_params);
    } else {
        list_stats(&list_params);
    }

    JSON_DECREF(match_cond);

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    /*-------------------------------------*
     *  Print times
     *-------------------------------------*/
    MT_INCREMENT_COUNT(time_measure, total_counter)
    MT_PRINT_TIME(time_measure, "list stats")

    gobj_end();

    return gobj_get_exit_code();
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int times = 0;
    times++;
    yev_loop_reset_running(yev_loop);
    if(times > 1) {
        exit(-1);
    }
}

PUBLIC void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);   // to debug in kdevelop
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);    // ctrl+c

    alarm(time2exit);
}
