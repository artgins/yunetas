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
#include <signal.h>
#include <regex.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>

#include <gobj.h>
#include <testing.h>
#include <helpers.h>
#include <kwid.h>
#include <tr_treedb.h>
#include <yev_loop.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define APP         "treedb_list"
#define DOC         "List messages of Treedb database."

#define VERSION     YUNETA_VERSION
#define SUPPORT     "<support at artgins.com>"
#define DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    char path[PATH_MAX];
    char database[PATH_MAX];
    char topic[NAME_MAX];
    json_t *jn_filter;
    json_t *jn_options;
    int verbose;
} list_params_t;

/***************************************************************************
 *              Arguments
 ***************************************************************************/
/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 1
#define MAX_ARGS 1
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

const char *argp_program_version = APP " " VERSION;
const char *argp_program_bug_address = SUPPORT;

/* Program documentation. */
static char doc[] = DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "PATH";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-----------------key-----arg-----------------flags---doc-----------------group */
{0,                     0,      0,                  0,      "Database",         2},
{"ids",                 'i',    "ID",               0,      "Id or list of id's.",2},
{"recursive",           'r',    0,                  0,      "List recursively.",  2},

{0,                     0,      0,                  0,      "Print",            3},
{"print-tranger",       5,      0,                  0,      "Print tranger json", 3},
{"print-treedb",        6,      0,                  0,      "Print treedb json", 3},

{0,                     0,      0,                  0,      "TreeDb options",       30},
//{"expand",              30,     0,                  0,      "Expand nodes.",         30},

{0}
};

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
struct arguments arguments;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
yev_loop_h yev_loop;
int time2exit = 10;
int total_counter = 0;
int partial_counter = 0;

/***************************************************************************
 *
 ***************************************************************************/
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
    printf("  directory: %s\n", directory);
    printf("  treedb   : %s\n", name);
    return TRUE; // to continue
}

PRIVATE int list_databases(const char *path)
{
    printf("Databases found:\n");
    walk_dir_tree(
        0,
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
    hgobj gobj = 0;
    /*-------------------------------*
     *  Startup TimeRanger
     *-------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:b, s:i}",
        "path", path,
        "master", 0,
        "on_critical_error", 0
    );
    json_t * tranger = tranger2_startup(0, jn_tranger, 0);
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
        print_json("tranger", tranger);
    } else if(arguments.print_treedb) {
        print_json("treedb", kw_get_dict(gobj, tranger, "treedbs", 0, KW_REQUIRED));
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

            print_json(topic_name, node_list);

            total_counter += (int)json_array_size(node_list);
            partial_counter += (int)json_array_size(node_list);

            json_decref(node_list);
        }
    }

    /*-------------------------------*
     *  Free resources
     *-------------------------------*/
    treedb_close_db(tranger, treedb_name);
    tranger2_shutdown(tranger);

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
        0,
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
 *
 ***************************************************************************/
PRIVATE void delete_right_slash(char *s)
{
    int l;
    char c;

    /*---------------------------------*
     *  Elimina blancos a la derecha
     *---------------------------------*/
    l = (int)strlen(s);
    if(l==0)
        return;
    while(--l>=0) {
        c= *(s+l);
        if(c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='/')
            *(s+l)='\0';
        else
            break;
    }
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*---------------------------------*
     *      Arguments
     *---------------------------------*/
    memset(&arguments, 0, sizeof(arguments));
    /*
     *  Default values
     */
    arguments.verbose = -1;

    /*
     *  Parse arguments
     */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    arguments.path = arguments.args[0];

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
    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    if(empty_string(arguments.path)) {
        fprintf(stderr, "What TimeRanger path?\n");
        fprintf(stderr, "You must supply --path option\n\n");
        exit(-1);
    }

    struct rlimit rl;
    // Get current limit
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        if(rl.rlim_cur < 200000) {
            // Set new limit
            rl.rlim_cur = 200000;  // Set soft limit
            rl.rlim_max = 200000;  // Set hard limit
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

    delete_right_slash(arguments.path);

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

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    /*-------------------------------------*
     *  Print times
     *-------------------------------------*/
    MT_INCREMENT_COUNT(time_measure, total_counter)
    MT_PRINT_TIME(time_measure, "list records")

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
