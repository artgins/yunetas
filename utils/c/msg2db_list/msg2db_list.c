/****************************************************************************
 *          MSG2DB_LIST.C
 *
 *  List Messages (ordered by pkey,pkey2: active and their instances) with TimeRanger2
 *
 *  Double dict of messages (message: active and instances)
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
#define APP_NAME    "msg2db_list"
#define DOC         "List messages of Dict/dict database."

#define VERSION     YUNETA_VERSION
#define SUPPORT     "<support at artgins.com>"
#define DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    char path[256];
    char database[256];
    char topic[256];
    json_t *jn_ids;
    json_t *jn_filter;
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
    int print_msg2db;
    int expand_nodes;
};

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
const char *argp_program_version = APP_NAME " " VERSION;
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
{"database",            'b',    "DATABASE",         0,      "Database.",        2},
{"topic",               'c',    "TOPIC",            0,      "Topic.",           2},
{"ids",                 'i',    "ID",               0,      "Id or list of id's.",2},
{"recursive",           'r',    0,                  0,      "List recursively.",  2},

{0,                     0,      0,                  0,      "Print",            3},
{"print-tranger",       5,      0,                  0,      "Print tranger json", 3},
{"print-msg2db",        6,      0,                  0,      "Print msg2db json", 3},

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
        arguments->print_msg2db = 1;
        break;

    case 30:
        arguments->expand_nodes = 1;
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
PRIVATE BOOL list_db_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[256]  filename
    int level,              // level of tree where the file is found
    wd_option opt           // option parameter
)
{
    printf("  directory: %s\n", directory);
    printf("  msg2db   : %s\n", name);
    return TRUE; // to continue
}

PRIVATE int list_databases(const char *path)
{
    printf("Databases found:\n");
    walk_dir_tree(
        0,
        path,
        ".*\\.msg2db_schema\\.json",
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
    char *msg2db_name, // must contains the msg2db name
    char *topic,
    json_t *jn_ids,
    json_t *jn_filter,
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
    json_t * tranger = tranger2_startup(0, jn_tranger, 0);
    if(!tranger) {
        fprintf(stderr, "Can't startup tranger %s\n\n", path);
        exit(-1);
    }

    /*-------------------------------*
     *  Open msg2db
     *-------------------------------*/
    json_t *msg2db = msg2db_open_db(
        tranger,
        msg2db_name,
        0, // jn_schema_sample
        "persistent"
    );

    if(arguments.print_tranger) {
        print_json("tranger", tranger);
    } else if(arguments.print_msg2db) {
        print_json("msg2dbs", kw_get_dict(0, tranger, "msg2dbs", 0, KW_REQUIRED));
    } else {
        const char *topic_name; json_t *topic_data;
        json_object_foreach(msg2db, topic_name, topic_data) {
            if(!empty_string(topic)) {
                if(strcmp(topic, topic_name)!=0) {
                    continue;
                }
            }
            JSON_INCREF(jn_ids);
            JSON_INCREF(jn_filter);

            json_t *node_list = msg2db_list_messages( // Return MUST be decref
                tranger,
                msg2db_name,
                topic_name,
                jn_ids,
                jn_filter,
                0  // match_fn
            );

            print_json(topic_name, node_list);

            total_counter += json_array_size(node_list);
            partial_counter += json_array_size(node_list);

            json_decref(node_list);
        }
    }

    /*-------------------------------*
     *  Free resources
     *-------------------------------*/
    msg2db_close_db(tranger, msg2db_name);
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
    json_t *jn_ids,
    json_t *jn_filter,
    int verbose)
{
    /*
     *  Check if path is a tranger directory
     */
    char path_tranger[PATH_MAX];
    snprintf(path_tranger, sizeof(path_tranger), "%s", path);
    if(!file_exists(path_tranger, "__timeranger2__.json")) {
        database = pop_last_segment(path_tranger);
        if(!file_exists(path_tranger, "__timeranger2__.json")) {
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
        return _list_messages(
            path_tranger,
            database,
            topic,
            jn_ids,
            jn_filter,
            verbose
        );
    }

    char database_name[NAME_MAX];
    snprintf(database_name, sizeof(database_name), "%s.msg2db_schema.json", database);
    if(file_exists(path_tranger, database_name)) {
        return _list_messages(
            path_tranger,
            database_name,
            topic,
            jn_ids,
            jn_filter,
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

    char *p = strstr(name, ".msg2db_schema.json");
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
        list_params2.jn_ids,
        list_params2.jn_filter,
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
        ".*\\.msg2db_schema\\.json",
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
    json_t *jn_ids,
    json_t *jn_filter,
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
    list_params.jn_ids = jn_ids;
    list_params.jn_filter = jn_filter;

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
    json_t *jn_ids = json_array();
    if(arguments.id) {
        int list_size;
        const char **ss = split2(arguments.id, ", ", &list_size);
        for(int i=0; i<list_size; i++) {
            json_array_append_new(jn_ids, json_string(ss[i]));
        }
        split_free2(ss);
    }

    /*----------------------------------*
     *  Filter
     *----------------------------------*/
    json_t *jn_filter = json_object();

    /*------------------------*
     *      Do your work
     *------------------------*/
    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    if(empty_string(arguments.path)) {
        fprintf(stderr, "What TimeRanger path?\n");
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

    if(arguments.recursive) {
        list_recursive_msg(
            arguments.path,
            arguments.database,
            arguments.topic,
            jn_ids,
            jn_filter,
            arguments.verbose
        );
    } else {
        list_messages(
            arguments.path,
            arguments.database,
            arguments.topic,
            jn_ids,
            jn_filter,
            arguments.verbose
        );
    }
    JSON_DECREF(jn_ids);
    JSON_DECREF(jn_filter);

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
