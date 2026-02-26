/****************************************************************************
 *          TREEDB_LIST.C
 *
 *  List records of Tree Database over TimeRanger2
 *  Hierarchical tree of objects (nodes, records, messages)
 *  linked by child-parent relation.
 *
 *          Copyright (c) 2018 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <argp.h>
#include <time.h>
#include <errno.h>
#include <regex.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <gobj.h>
#include <helpers.h>
#include <kwid.h>
#include <timeranger2.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define APP_NAME    "treedb_list"
#define DOC         "List messages of Treedb database."

#define VERSION     __ghelpers_version__
#define SUPPORT     "<niyamaka at yuneta.io>"
#define DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    char path[256];
    char database[256];
    char topic[256];
    json_t *jn_filter;
    json_t *jn_options;
    int verbose;
} list_params_t;


/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 0
#define MAX_ARGS 0
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */

    char *path;
    char *database;
    char *topic;
    char *id;
    int recursive;
    char *mode;
    char *fields;
    int verbose;

    int print_tranger;
    int print_treedb;
};

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
const char *argp_program_version = APP_NAME " " VERSION;
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
{"database",            'b',    "DATABASE",         0,      "Database.",        2},
{"topic",               'c',    "TOPIC",            0,      "Topic.",           2},
{"ids",                 'i',    "ID",               0,      "Id or list of id's.",2},
{"recursive",           'r',    0,                  0,      "List recursively.",  2},

{0,                     0,      0,                  0,      "Print",            3},
{"print-tranger",       5,      0,                  0,      "Print tranger json", 3},
{"print-treedb",        6,      0,                  0,      "Print treedb json", 3},

{0,                     0,      0,                  0,      "TreeDb options",       30},
//{"expand",              30,     0,                  0,      "Expand nodes.",         30},

{0}
};

/* Our argp parser. */
static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc
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
    case 'b':
        arguments->database= arg;
        break;
    case 'c':
        arguments->topic= arg;
        break;
    case 'i':
        arguments->id= arg;
        break;
    case 'r':
        arguments->recursive = 1;
        break;
    case 'l':
        if(arg) {
            arguments->verbose = atoi(arg);
        }
        break;

    case 5:
        arguments->print_tranger = 1;
        break;

    case 6:
        arguments->print_treedb = 1;
        break;

    case 30:
        //arguments->expand_nodes = 1;
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

/***************************************************************************
 *
 ***************************************************************************/
static inline double ts_diff2 (struct timespec start, struct timespec end)
{
    uint64_t s, e;
    s = ((uint64_t)start.tv_sec)*1000000 + ((uint64_t)start.tv_nsec)/1000;
    e = ((uint64_t)end.tv_sec)*1000000 + ((uint64_t)end.tv_nsec)/1000;
    return ((double)(e-s))/1000000;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_db_cb(
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,   // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
)
{
    printf("  directory: %s\n", directory);
    printf("  treedb   : %s\n", name);
    return TRUE; // to continue
}

PRIVATE int list_databases(const char *path)
{
    printf("Databases found:\n");
    walk_dir_tree(
        path,
        ".*\\.treedb_schema\\.json",
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
PRIVATE int _list_messages(
    char *path, // must contains the full path of tranger database
    char *treedb_name, // must contains the treedb name
    char *topic,
    json_t *jn_filter,
    json_t *jn_options,
    int verbose)
{
    /*-------------------------------*
     *  Startup TimeRanger
     *-------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:b, s:i}",
        "path", path,
        "master", 0,
        "on_critical_error", 0
    );
    json_t * tranger = tranger_startup(jn_tranger);
    if(!tranger) {
        fprintf(stderr, "Can't startup tranger %s\n\n", path);
        exit(-1);
    }

    /*-------------------------------*
     *  Open treedb
     *-------------------------------*/
    json_t *treedb = treedb_open_db(
        tranger,
        treedb_name,
        0, // jn_schema_sample
        "persistent"
    );
    treedb_set_callback(
        tranger,
        treedb_name,
        0,
        0
    );


    if(arguments.print_tranger) {
        print_json(tranger);
    } else if(arguments.print_treedb) {
        print_json(kw_get_dict(tranger, "treedbs", 0, KW_REQUIRED));
    } else {
        const char *topic_name; json_t *topic_data;
        json_object_foreach(treedb, topic_name, topic_data) {
            if(!json_is_object(topic_data)) {
                // Can be __schema_version__ or other attribute
                continue;
            }
            if(!empty_string(topic)) {
                if(strcmp(topic, topic_name)!=0) {
                    continue;
                }
            }
            JSON_INCREF(jn_filter);

            json_t *iter = treedb_list_nodes( // Return MUST be decref
                tranger,
                treedb_name,
                topic_name,
                jn_filter,
                0  // match_fn
            );

            json_t *node_list = json_array();
            int idx; json_t *node;
            json_array_foreach(iter, idx, node) {
                json_array_append_new(
                    node_list,
                    node_collapsed_view(
                        tranger,
                        node,
                        json_incref(jn_options)
                    )
                );
            }
            json_decref(iter);

            print_json2(topic_name, node_list);

            total_counter += json_array_size(node_list);
            partial_counter += json_array_size(node_list);

            json_decref(node_list);
        }
    }

    /*-------------------------------*
     *  Free resources
     *-------------------------------*/
    treedb_close_db(tranger, treedb_name);
    tranger_shutdown(tranger);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_messages(
    char *path,
    char *database,
    char *topic,
    json_t *jn_filter,
    json_t *jn_options,
    int verbose)
{
    /*
     *  Check if path is a tranger directory
     */
    char path_tranger[PATH_MAX];
    snprintf(path_tranger, sizeof(path_tranger), "%s", path);
    if(!file_exists(path_tranger, "__timeranger__.json")) {
        database = pop_last_segment(path_tranger);
        if(!file_exists(path_tranger, "__timeranger__.json")) {
            fprintf(stderr, "What Database?\n\n");
            list_databases(path_tranger);
            exit(-1);
        }
    }

    if(!is_directory(path_tranger)) {
        fprintf(stderr, "Directory '%s' not found\n\n", path_tranger);
        exit(-1);
    }
    if(empty_string(database)) {
        fprintf(stderr, "What Database?\n\n");
        list_databases(path_tranger);
        exit(-1);
    }

    if(file_exists(path_tranger, database)) {
        char *p = strstr(database, ".treedb_schema.json");
        if(p) {
            *p = 0;
        }
        return _list_messages(
            path_tranger,
            database,
            topic,
            jn_filter,
            jn_options,
            verbose
        );
    }

    char database_name[NAME_MAX];
    snprintf(database_name, sizeof(database_name), "%s.treedb_schema.json", database);
    if(file_exists(path_tranger, database_name)) {
        char *p = strstr(database_name, ".treedb_schema.json");
        if(p) {
            *p = 0;
        }
        return _list_messages(
            path_tranger,
            database_name,
            topic,
            jn_filter,
            jn_options,
            verbose
        );
    }

    fprintf(stderr, "What Database?\n\n");
    list_databases(path_tranger);
    exit(-1);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_recursive_db_cb(
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
)
{
    list_params_t *list_params = user_data;
    list_params_t list_params2 = *list_params;

    char *p = strstr(name, ".treedb_schema.json");
    if(p) {
        *p = 0;
    }

    snprintf(list_params2.path, sizeof(list_params2.path), "%s", directory);
    snprintf(list_params2.database, sizeof(list_params2.database), "%s", name);

    partial_counter = 0;
    _list_messages(
        list_params2.path,
        list_params2.database,
        list_params2.topic,
        list_params2.jn_filter,
        list_params2.jn_options,
        list_params2.verbose
    );

    printf("====> %s: %d records\n\n", name, partial_counter);

    return TRUE; // to continue
}

PRIVATE int list_recursive_databases(list_params_t *list_params)
{
    walk_dir_tree(
        list_params->path,
        ".*\\.treedb_schema\\.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_recursive_db_cb,
        list_params
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_recursive_msg(
    char *path,
    char *database,
    char *topic,
    json_t *jn_filter,
    json_t *jn_options,
    int verbose)
{
    list_params_t list_params;
    memset(&list_params, 0, sizeof(list_params));

    snprintf(list_params.path, sizeof(list_params.path), "%s", path);
    if(!empty_string(database)) {
        snprintf(list_params.database, sizeof(list_params.database), "%s", database);
    }
    if(!empty_string(topic)) {
        snprintf(list_params.topic, sizeof(list_params.topic), "%s", topic);
    }
    list_params.jn_filter = jn_filter;
    list_params.jn_options = jn_options;

    list_params.verbose = verbose;

    return list_recursive_databases(&list_params);
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

    /*-------------------------------------*
     *  Your start code
     *-------------------------------------*/
    init_ghelpers_library(APP_NAME);
    log_startup(
        "test",             // application name
        "1.0.0",            // applicacion version
        "test_glogger"     // executable program, to can trace stack
    );

    /*------------------------------------------------*
     *          Setup memory
     *------------------------------------------------*/
    #define MEM_MIN_BLOCK   512
    uint64_t MEM_MAX_SYSTEM_MEMORY = free_ram_in_kb() * 1024LL;
    MEM_MAX_SYSTEM_MEMORY /= 100LL;
    MEM_MAX_SYSTEM_MEMORY *= 90LL;  // Coge el 90% de la memoria

    uint64_t MEM_MAX_BLOCK = (MEM_MAX_SYSTEM_MEMORY / sizeof(md_record_t)) * sizeof(md_record_t);
    MEM_MAX_BLOCK = MIN(1*1024*1024*1024LL, MEM_MAX_BLOCK);  // 1*G max

    uint64_t MEM_SUPERBLOCK = MEM_MAX_BLOCK;

    static uint32_t mem_list[] = {2037, 0};
    gbmem_trace_alloc_free(0, mem_list);

    if(1) {
        gbmem_startup(
            MEM_MIN_BLOCK,
            MEM_MAX_BLOCK,
            MEM_SUPERBLOCK,
            MEM_MAX_SYSTEM_MEMORY,
            NULL,
            0
        );
    } else {
        gbmem_startup_system(
            MEM_MAX_BLOCK,
            MEM_MAX_SYSTEM_MEMORY
        );
    }
    json_set_alloc_funcs(
        gbmem_malloc,
        gbmem_free
    );
    uv_replace_allocator(
        gbmem_malloc,
        gbmem_realloc,
        gbmem_calloc,
        gbmem_free
    );

    log_startup(
        APP_NAME,       // application name
        VERSION,        // applicacion version
        APP_NAME        // executable program, to can trace stack
    );
    log_add_handler(APP_NAME, "stdout", LOG_OPT_LOGGER, 0);


    /*----------------------------------*
     *  Ids
     *----------------------------------*/
    json_t *jn_filter = json_array();
    if(arguments.id) {
        int list_size;
        const char **ss = split2(arguments.id, ", ", &list_size);
        for(int i=0; i<list_size; i++) {
            json_array_append_new(jn_filter, json_string(ss[i]));
        }
        split_free2(ss);
    }

    /*----------------------------------*
     *  Options
     *----------------------------------*/
    json_t *jn_options = 0; //json_object();
//     if(!arguments.expand_nodes) {
//         json_object_set_new(
//             jn_options,
//             "collapsed",
//             json_true()
//         );
//     }

    /*------------------------*
     *      Do your work
     *------------------------*/
    struct timespec st, et;
    double dt;

    clock_gettime (CLOCK_MONOTONIC, &st);

    if(empty_string(arguments.path)) {
        fprintf(stderr, "What TimeRanger path?\n");
        exit(-1);
    }
    if(arguments.recursive) {
        list_recursive_msg(
            arguments.path,
            arguments.database,
            arguments.topic,
            jn_filter,
            jn_options,
            arguments.verbose
        );
    } else {
        list_messages(
            arguments.path,
            arguments.database,
            arguments.topic,
            jn_filter,
            jn_options,
            arguments.verbose
        );
    }
    JSON_DECREF(jn_filter);
    JSON_DECREF(jn_options);

    clock_gettime (CLOCK_MONOTONIC, &et);

    /*-------------------------------------*
     *  Print times
     *-------------------------------------*/
    dt = ts_diff2(st, et);

    setlocale(LC_ALL, "");
    printf("====> Total: %'d records; %'f seconds; %'lu op/sec\n\n",
        total_counter,
        dt,
        (unsigned long)(((double)total_counter)/dt)
    );

    gbmem_shutdown();
    return 0;
}
