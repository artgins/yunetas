/****************************************************************************
 *          STATS_LIST.C
 *
 *          List stats
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <regex.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

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

/***************************************************************************
 *      Data
 ***************************************************************************/
struct arguments arguments;
int total_counter = 0;
int partial_counter = 0;
const char *argp_program_version = NAME " " VERSION;
const char *argp_program_bug_address = SUPPORT;

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

#ifdef PEPE_TODO

/***************************************************************************
 *
 ***************************************************************************/
void print_summary(json_t *metrics)
{
    const char *var;
    json_t *jn_var;
    json_object_foreach(metrics, var, jn_var) {
        printf("Variable: %s\n", var);

        const char *metric;
        json_t *jn_metric;
        json_object_foreach(jn_var, metric, jn_metric) {
            printf("    Metric-Id: %s\n", metric);
            printf("    Period: %s\n", kw_get_str(jn_metric, "period", "", KW_REQUIRED));
            printf("    Units: %s\n", kw_get_str(jn_metric, "units", "", KW_REQUIRED));
            printf("    Compute: %s\n", kw_get_str(jn_metric, "compute", "", KW_REQUIRED));
            json_t *jn_data = kw_get_list(jn_metric, "data", 0, KW_REQUIRED);
            if(json_array_size(jn_data)) {
                int items = json_array_size(jn_data);
                json_t *jn_first = json_array_get(jn_data, 0);
                json_t *jn_last = json_array_get(jn_data, items-1);
                printf("        From %s\n",
                    kw_get_str(jn_first, "fr_d", "", KW_REQUIRED)
                );
                printf("        To   %s\n",
                    kw_get_str(jn_last, "to_d", "", KW_REQUIRED)
                );
            }
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void print_keys(json_t *metrics)
{
    const char *key;
    json_t *jn_value;
    json_object_foreach(metrics, key, jn_value) {
        printf("        %s\n", key);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void print_units(json_t *variable)
{
    const char *key;
    json_t *jn_value;
    json_object_foreach(variable, key, jn_value) {
        printf("        %s\n", json_string_value(json_object_get(jn_value, "units")));
    }

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void print_limits(json_t *jn_metric)
{
    json_t *jn_data = kw_get_list(jn_metric, "data", 0, KW_REQUIRED);
    if(json_array_size(jn_data)) {
        int items = json_array_size(jn_data);
        json_t *jn_first = json_array_get(jn_data, 0);
        json_t *jn_last = json_array_get(jn_data, items-1);
        printf("        From %s\n",
            kw_get_str(jn_first, "fr_d", "", KW_REQUIRED)
        );
        printf("        To   %s\n",
            kw_get_str(jn_last, "to_d", "", KW_REQUIRED)
        );
    }
}

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
    /*-------------------------------*
     *  Open group
     *-------------------------------*/
    json_t *jn_stats = json_pack("{s:s, s:s}",
        "path", path,
        "groups", group_name?group_name:""
    );
    json_t * stats = rstats_open(jn_stats);
    if(!stats) {
        fprintf(stderr, "Can't open stats %s/%s\n\n", path, group_name);
        exit(-1);
    }
    if(verbose) {
        print_json(stats);
    }

    json_t *variables = rstats_variables(stats);
    if(verbose) {
        print_json(variables);
    }

    if(kw_get_bool(match_cond, "show_limits", 0, 0)) {
        const char *var;
        json_t *jn_v;
        json_object_foreach(variables, var, jn_v) {
            printf("   Variable: %s\n", var);
            const char *metr;
            json_t *jn_metr;
            json_object_foreach(jn_v, metr, jn_metr) {
                printf("   Metrica: %s, Units: %s\n", metr, kw_get_str(jn_metr, "units", "", 0));
                print_limits(jn_metr);
            }
        }
        /*
         *  Free resources
         */
        JSON_DECREF(variables);
        rstats_close(stats);
        return -1;
    }

    uint64_t from_t = kw_get_int(match_cond, "from_t", 0, KW_WILD_NUMBER);
    uint64_t to_t = kw_get_int(match_cond, "to_t", 0, KW_WILD_NUMBER);
    const char *variable = kw_get_str(match_cond, "variable", "", 0);
    const char *metric_name = kw_get_str(match_cond, "metric", metric_name_, 0);
    const char *units = kw_get_str(match_cond, "units", "", 0);

    if(empty_string(variable)) {
        printf("What Variable?\n");
        printf("    Available Variables:\n");
        print_keys(variables);

        /*
         *  Free resources
         */
        JSON_DECREF(variables);
        rstats_close(stats);
        return -1;
    }

    json_t *jn_variable = kw_get_dict(variables, variable, 0, 0);
    if(!jn_variable) {
        printf("Variable \"%s\" not found\n", variable);
        printf("    Available Variables:\n");
        print_keys(variables);

        /*
         *  Free resources
         */
        JSON_DECREF(variables);
        rstats_close(stats);
        return -1;
    }

    json_t *metric = 0;
    if(!empty_string(units)) {
        metric = find_metric_by_units(variables, variable, units, FALSE);
        if(!metric) {
            printf("What Units?\n");
            printf("    Available Units:\n");
            print_units(jn_variable);

            /*
             *  Free resources
             */
            JSON_DECREF(variables);
            rstats_close(stats);
            return -1;
        }
    }
    if(!metric) {
        if(empty_string(metric_name)) {
            printf("What Metric or Units?\n");
            printf("    Available Metrics:\n");
            print_keys(jn_variable);
            printf("    Available Units:\n");
            print_units(jn_variable);

            /*
            *  Free resources
            */
            JSON_DECREF(variables);
            rstats_close(stats);
            return -1;
        }

        metric = rstats_metric(variables, variable, metric_name, "", FALSE);
        if(!metric) {
            printf("What Metric?\n");
            printf("    Available Metrics:\n");
            print_keys(jn_variable);

            /*
             *  Free resources
             */
            JSON_DECREF(variables);
            rstats_close(stats);
            return -1;
        }
    }

    if(!from_t && !to_t) {
        printf("What Range?\n");
        printf("    Available Limits:\n");
        print_limits(metric);

        /*
         *  Free resources
         */
        JSON_DECREF(metric);
        JSON_DECREF(variables);
        rstats_close(stats);
        return -1;
    }

    if(!to_t) {
        //to_t = time((time_t *)&to_t);
        to_t = (uint64_t)-1;
    }
    if(!from_t) {
        from_t = 1;
    }

    json_t *jn_data = rstats_get_data(metric, from_t, to_t);
    if(jn_data) {
        print_json(jn_data);
        JSON_DECREF(jn_data);
    }

    /*
     *  Free resources
     */
    JSON_DECREF(metric);
    JSON_DECREF(variables);
    rstats_close(stats);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_metric_cb(
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
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
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
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
    build_path2(temp, sizeof(temp), list_params->path_simple_stats, "");

    if(!empty_string(list_params->arguments->group)) {
        if(!subdir_exists(list_params->path_simple_stats, list_params->arguments->group)) {
            fprintf(stderr, "Group not found: '%s'\n\n", list_params->arguments->group);
            exit(-1);
        }
        build_path2(temp, sizeof(temp), list_params->path_simple_stats, list_params->arguments->group);
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
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // name of type found
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
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
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // name of type found
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
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
        list_params->arguments->path,
        ".*\\__simple_stats__\\.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_recursive_cb,
        list_params
    );

    return 0;
}
#endif

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

    // TODO migrate to Yunetas
    // uint64_t MEM_MAX_SYSTEM_MEMORY = free_ram_in_kb() * 1024LL;
    // MEM_MAX_SYSTEM_MEMORY /= 100LL;
    // MEM_MAX_SYSTEM_MEMORY *= 90LL;  // Coge el 90% de la memoria
    //
    // uint64_t MEM_MAX_BLOCK = (MEM_MAX_SYSTEM_MEMORY / sizeof(md_record_t)) * sizeof(md_record_t);
    //
    // MEM_MAX_BLOCK = MIN(1*1024*1024*1024LL, MEM_MAX_BLOCK);  // 1*G max
    //
    // BOOL debug = 0;
    // if(debug) {
    //     #define MEM_MIN_BLOCK   512
    //     #define MEM_SUPERBLOCK  209715200   // 200*M
    //     static uint32_t mem_list[] = {
    //         1012,
    //         0
    //     };
    //     gbmem_trace_alloc_free(1, mem_list);
    //     gbmem_startup(
    //         MEM_MIN_BLOCK,
    //         MEM_MAX_BLOCK,
    //         MEM_SUPERBLOCK,
    //         MEM_MAX_SYSTEM_MEMORY,
    //         NULL,
    //         0
    //     );
    // } else {
    //     gbmem_startup_system(
    //         MEM_MAX_BLOCK,
    //         MEM_MAX_SYSTEM_MEMORY
    //     );
    // }
    //
    // gbmem_startup_system(
    //     MEM_MAX_BLOCK,
    //     MEM_MAX_SYSTEM_MEMORY
    // );
    // json_set_alloc_funcs(
    //     gbmem_malloc,
    //     gbmem_free
    // );
    //
    // log_startup(
    //     NAME,       // application name
    //     VERSION,    // applicacion version
    //     NAME        // executable program, to can trace stack
    // );
    // log_add_handler(NAME, "stdout", debug?LOG_OPT_ALL:LOG_OPT_UP_WARNING, 0);
    //
    // /*----------------------------------*
    //  *  Match conditions
    //  *----------------------------------*/
    // json_t *match_cond = json_object();
    //
    // if(arguments.from_t) {
    //     timestamp_t timestamp;
    //     if(all_numbers(arguments.from_t)) {
    //         timestamp = atoll(arguments.from_t);
    //     } else {
    //         timestamp = approxidate(arguments.from_t);
    //     }
    //     json_object_set_new(match_cond, "from_t", json_integer(timestamp));
    // }
    // if(arguments.to_t) {
    //     timestamp_t timestamp;
    //     if(all_numbers(arguments.to_t)) {
    //         timestamp = atoll(arguments.to_t);
    //     } else {
    //         timestamp = approxidate(arguments.to_t);
    //     }
    //     json_object_set_new(match_cond, "to_t", json_integer(timestamp));
    // }
    //
    // if(arguments.variable) {
    //     json_object_set_new(
    //         match_cond,
    //         "variable",
    //         json_string(arguments.variable)
    //     );
    // }
    // if(arguments.units) {
    //     json_object_set_new(
    //         match_cond,
    //         "units",
    //         json_string(arguments.units)
    //     );
    // }
    // if(arguments.limits) {
    //     json_object_set_new(
    //         match_cond,
    //         "show_limits",
    //         json_true()
    //     );
    // }
    //
    // if(json_object_size(match_cond)>0) {
    // } else {
    //     JSON_DECREF(match_cond);
    // }
    //
    // /*
    //  *  Do your work
    //  */
    // if(empty_string(arguments.path)) {
    //     fprintf(stderr, "What Statistics path?\n");
    //     exit(-1);
    // }
    //
    // list_params_t list_params;
    // memset(&list_params, 0, sizeof(list_params));
    // list_params.arguments = &arguments;
    // list_params.match_cond = match_cond;
    //
    // if(arguments.raw) {
    //     _list_stats(
    //         arguments.path,
    //         arguments.group,
    //         arguments.metric,
    //         match_cond,
    //         arguments.verbose
    //     );
    // } else if(arguments.recursive) {
    //     list_recursive(&list_params);
    // } else {
    //     list_stats(&list_params);
    // }
    //
    // JSON_DECREF(match_cond);
    //
    // gbmem_shutdown();
    // log_end();

    return 0;
}
