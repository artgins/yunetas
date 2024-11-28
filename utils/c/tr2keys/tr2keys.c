/****************************************************************************
 *          tr2keys.C
 *
 *          List messages of tranger database
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <argp.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>

#include <gobj.h>
#include <testing.h>
#include <helpers.h>
#include <kwid.h>
#include <timeranger2.h>
#include <stacktrace_with_backtrace.h>
#include <cpu.h>
#include <yev_loop.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "tr2keys"
#define DOC         "List keys of a topic"

#define VERSION     "1.0" // __ghelpers_version__
#define SUPPORT     "<niyamaka at yuneta.io>"
#define DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *              Structures
 ***************************************************************************/

/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 1
#define MAX_ARGS 1
struct arguments {
    char *args[MAX_ARGS+1];     /* positional args */

    char *path;
    char *database;
    char *topic;
    int recursive;
    char *mode;
    char *fields;
    int verbose;

    char *from_t;
    char *to_t;

    char *from_rowid;
    char *to_rowid;

    char *user_flag_mask_set;
    char *user_flag_mask_notset;

    char *system_flag_mask_set;
    char *system_flag_mask_notset;

    char *key;
    char *notkey;

    char *from_tm;
    char *to_tm;

    char *rkey;
    char *filter;

    int list_databases;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PUBLIC void yuno_catch_signals(void);
static error_t parse_opt (int key, char *arg, struct argp_state *state);
PRIVATE int list_topics(const char *path);

/***************************************************************************
 *      Data
 ***************************************************************************/
const char *argp_program_version = NAME " " VERSION;
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
//{"topic",               'c',    "TOPIC",            0,      "Topic name.",      2},
{"recursive",           'r',    0,                  0,      "List recursively.",  2},

{0,                     0,      0,                  0,      "Presentation",     3},
//{"verbose",             'l',    "LEVEL",            0,      "Verbose level (empty=total, 0=metadata, 1=metadata, 2=metadata+path, 3=metadata+record)", 3},
//{"mode",                'm',    "MODE",             0,      "Mode: form or table", 3},
//{"fields",              'f',    "FIELDS",           0,      "Print only this fields", 3},
//
//{0,                     0,      0,                  0,      "Search conditions", 4},
//{"from-t",              1,      "TIME",             0,      "From time.",       4},
//{"to-t",                2,      "TIME",             0,      "To time.",         4},
//{"from-rowid",          4,      "TIME",             0,      "From rowid.",      5},
//{"to-rowid",            5,      "TIME",             0,      "To rowid.",        5},
//
//{"user-flag-set",       9,      "MASK",             0,      "Mask of User Flag set.",   6},
//{"user-flag-not-set",   10,     "MASK",             0,      "Mask of User Flag not set.",6},
//
//{"system-flag-set",     13,     "MASK",             0,      "Mask of System Flag set.",   7},
//{"system-flag-not-set", 14,     "MASK",             0,      "Mask of System Flag not set.",7},
//
//{"key",                 15,     "KEY",              0,      "Key.",             9},
//{"not-key",             16,     "KEY",              0,      "Not key.",         9},
//
//{"from-tm",             17,     "TIME",             0,      "From msg time.",       10},
//{"to-tm",               18,     "TIME",             0,      "To msg time.",         10},
//
//{"rkey",                19,     "RKEY",             0,      "Regular expression of Key.", 11},
//{"filter",              20,     "FILTER",           0,      "Filter of fields in json dict string", 11},

{0,                     0,      0,                  0,      "Print", 12},
{"list-databases",      21,     0,                  0,      "List databases.",  12},

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

yev_loop_t *yev_loop;
int time2exit = 10;
struct arguments arguments;
int total_counter = 0;
int partial_counter = 0;
json_t *match_cond = 0;
yev_loop_t *yev_loop;

/***************************************************************************
 *  Parse a single option
 ***************************************************************************/
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    /*
     *  Get the input argument from argp_parse,
     *  which we know is a pointer to our arguments structure.
     */
    struct arguments *arguments_ = state->input;

    switch (key) {
    case 'c':
        arguments_->topic= arg;
        break;
    case 'r':
        arguments_->recursive = 1;
        break;
    case 'l':
        if(arg) {
            arguments_->verbose = atoi(arg);
        }
        break;
    case 'm':
        arguments_->mode = arg;
        break;
    case 'f':
        arguments_->fields = arg;
        break;

    case 1: // from_t
        arguments_->from_t = arg;
        break;
    case 2: // to_t
        arguments_->to_t = arg;
        break;

    case 4: // from_rowid
        arguments_->from_rowid = arg;
        break;
    case 5: // to_rowid
        arguments_->to_rowid = arg;
        break;

    case 9:
        arguments_->user_flag_mask_set = arg;
        break;
    case 10:
        arguments_->user_flag_mask_notset = arg;
        break;

    case 13:
        arguments_->system_flag_mask_set = arg;
        break;
    case 14:
        arguments_->system_flag_mask_notset = arg;
        break;

    case 15:
        arguments_->key = arg;
        break;
    case 16:
        arguments_->notkey = arg;
        break;

    case 17: // from_tm
        arguments_->from_tm = arg;
        break;
    case 18: // to_tm
        arguments_->to_tm = arg;
        break;

    case 19: // to_tm
        arguments_->rkey = arg;
        break;

    case 20: // filter
        arguments_->filter= arg;
        break;

    case 21:
        arguments_->list_databases = 1;
        break;

    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            /* Too many arguments_. */
            argp_usage (state);
        }
        arguments_->args[state->arg_num] = arg;
        break;

    case ARGP_KEY_END:
        if (state->arg_num < MIN_ARGS) {
            /* Not enough arguments_. */
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
    char *name,             // dname[255]
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    char *p = strrchr(fullpath, '/');
    if(p) {
        *p = 0;
    }
    char topic_path[PATH_MAX];
    snprintf(topic_path, sizeof(topic_path), "%s", fullpath);
    printf("TimeRanger ==> '%s'\n", fullpath);
    list_topics(topic_path);

    return TRUE; // to continue
}

PRIVATE int list_databases(const char *path)
{
    walk_dir_tree(
        0,
        path,
        "__timeranger2__.json",
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
PRIVATE BOOL list_topic_cb(
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
    char *p = strrchr(directory, '/');
    if(p) {
        printf("        %s\n", p+1);
    } else {
        printf("        %s\n", directory);
    }
    return TRUE; // to continue
}

PRIVATE int list_topics(const char *path)
{
    printf("    Topics:\n");
    walk_dir_tree(
        0,
        path,
        "topic_desc.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_topic_cb,
        0
    );
    printf("\n");
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_messages(void)
{
    if(empty_string(arguments.topic)) {
        arguments.topic = pop_last_segment(arguments.path);
        if(empty_string(arguments.database)) {
            arguments.database = pop_last_segment(arguments.path);
        }
    }

    /*-------------------------------*
     *      Startup TimeRanger
     *-------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:s}",
        "path", arguments.path,
        "database", arguments.database
    );

    json_t * tranger = tranger2_startup(0, jn_tranger, 0);
    if(!tranger) {
        fprintf(stderr, "Can't startup tranger %s/%s\n\n", arguments.path, arguments.database);
        exit(-1);
    }

    /*-------------------------------*
     *  Open topic
     *-------------------------------*/
    json_t *topic = tranger2_open_topic(
        tranger,
        arguments.topic,
        FALSE
    );
    if(!topic) {
        fprintf(stderr, "Can't open topic %s\n\n", arguments.topic);
        exit(-1);
    }
    printf("Topic ===> '%s'\n", kw_get_str(0, topic, "directory", "", KW_REQUIRED));

    json_t *jn_keys = tranger2_list_keys( // return is yours
        tranger,
        arguments.topic,
        arguments.rkey
    );
    print_json2("Keys", jn_keys);
    JSON_DECREF(jn_keys)
    /*-------------------------------*
     *  Free resources
     *-------------------------------*/
    tranger2_close_topic(tranger, arguments.topic);
    tranger2_shutdown(tranger);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_topic_messages(void)
{
    char path_topic[PATH_MAX];

    build_path(path_topic, sizeof(path_topic),
        arguments.path,
        arguments.database,
        arguments.topic,
        NULL
    );

    if(!file_exists(path_topic, "topic_desc.json")) {
        if(!is_directory(path_topic)) {
            fprintf(stderr, "Path not found: '%s'\n\n", path_topic);
            exit(-1);
        }
        fprintf(stderr, "What Topic? Found:\n\n");
        list_databases(arguments.path);
        exit(-1);
    }
    return list_messages();
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_recursive_topic_cb(
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
    partial_counter = 0;

    pop_last_segment(fullpath);
    arguments.topic = pop_last_segment(fullpath);
    arguments.database = pop_last_segment(fullpath);
    arguments.path = fullpath;

    list_messages();

    if(partial_counter > 0) {
        printf("====> %s %s: %d records\n\n",
            arguments.database,
            arguments.topic,
            partial_counter
        );
    }

    return TRUE; // to continue
}

PRIVATE int list_recursive_topics(void)
{
    walk_dir_tree(
        0,
        arguments.path,
        "topic_desc.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_recursive_topic_cb,
        0
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL search_topic_cb(
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
    partial_counter = 0;
    pop_last_segment(fullpath);
    arguments.topic = pop_last_segment(fullpath);
    arguments.database = pop_last_segment(fullpath);
    arguments.path = fullpath;

    if(!arguments.topic || strcmp(arguments.topic, arguments.topic)==0) {
        list_messages();
        if(arguments.recursive) {
            if(partial_counter > 0) {
                printf("====> %s %s: %d records\n\n",
                    arguments.database,
                    arguments.topic,
                    partial_counter
                );
            }
        } else {
            printf("====> %s %s: %d records\n\n",
                arguments.database,
                arguments.topic,
                partial_counter
            );
        }
    }

    return TRUE; // to continue
}

PRIVATE int search_topics(void)
{
    walk_dir_tree(
        0,
        arguments.path,
        "topic_desc.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        search_topic_cb,
        0
    );
    return 0;
}

PRIVATE BOOL search_by_databases_cb(
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
    arguments.path = (char *)directory;
    arguments.database = 0;
    arguments.topic = arguments.topic;

    search_topics();

    return TRUE; // to continue
}

PRIVATE int search_by_databases()
{
    walk_dir_tree(
        0,
        arguments.path,
        "__timeranger2__.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        search_by_databases_cb,
        0
    );
    //printf("\n");
    return 0;
}

PRIVATE BOOL search_by_paths_cb(
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
    arguments.path = (char *)fullpath;
    arguments.database = 0;
    arguments.topic = arguments.topic;

    search_by_databases();

    return TRUE; // to continue
}

PRIVATE int search_by_paths(void)
{
    if(empty_string(arguments.database)) {
        arguments.database = pop_last_segment(arguments.path);
    }

    walk_dir_tree(
        0,
        arguments.path,
        arguments.database,
        WD_MATCH_DIRECTORY,
        search_by_paths_cb,
        0
    );
    //printf("\n");
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_recursive_topic_messages(void)
{
    char path_tranger[PATH_MAX];

    build_path(path_tranger, sizeof(path_tranger),
        arguments.path,
        arguments.database,
        NULL
    );

    if(!file_exists(path_tranger, "__timeranger2__.json")) {
        if(file_exists(path_tranger, "topic_desc.json")) {
            arguments.topic = pop_last_segment(path_tranger);
            arguments.database = pop_last_segment(path_tranger);
            arguments.path = path_tranger;
            return list_messages();
        }

        search_by_paths();
        exit(-1);
    }

    arguments.topic = "";
    arguments.database = "";
    arguments.path = path_tranger;
    return list_recursive_topics();
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
    /*
     *  Default values
     */
    memset(&arguments, 0, sizeof(arguments));
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

    gobj_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

#ifdef DEBUG
    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);
#endif

    uint64_t MEM_MAX_SYSTEM_MEMORY = free_ram_in_kb() * 1024LL;
    MEM_MAX_SYSTEM_MEMORY /= 100LL;
    MEM_MAX_SYSTEM_MEMORY *= 90LL;  // Coge el 90% de la memoria
    uint64_t MEM_MAX_BLOCK = (MEM_MAX_SYSTEM_MEMORY / sizeof(md2_record_ex_t)) * sizeof(md2_record_ex_t);
    MEM_MAX_BLOCK = MIN(1*1024*1024*1024LL, MEM_MAX_BLOCK);  // 1*G max

    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // startup_persistent_attrs
        NULL, // end_persistent_attrs
        0,  // load_persistent_attrs
        0,  // save_persistent_attrs
        0,  // remove_persistent_attrs
        0,  // list_persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL, // global_authenticate_parser
        MEM_MAX_BLOCK,  // max_block, largest memory block
        MEM_MAX_SYSTEM_MEMORY // max_system_memory, maximum system memory
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

    /*----------------------------------*
     *  Match conditions
     *----------------------------------*/
    match_cond = json_object();

    if(arguments.from_t) {
        json_object_set_new(match_cond, "from_t", json_string(arguments.from_t));
    }
    if(arguments.to_t) {
        json_object_set_new(match_cond, "to_t", json_string(arguments.to_t));
    }
    if(arguments.from_tm) {
        json_object_set_new(match_cond, "from_tm", json_string(arguments.from_tm));
    }
    if(arguments.to_tm) {
        json_object_set_new(match_cond, "to_tm", json_string(arguments.to_tm));
    }

    if(arguments.from_rowid) {
        json_object_set_new(match_cond, "from_rowid", json_integer(atoll(arguments.from_rowid)));
    }
    if(arguments.to_rowid) {
        json_object_set_new(match_cond, "to_rowid", json_integer(atoll(arguments.to_rowid)));
    }

    if(arguments.user_flag_mask_set) {
        json_object_set_new(
            match_cond,
            "user_flag_mask_set",
            json_integer(atol(arguments.user_flag_mask_set))
        );
    }
    if(arguments.user_flag_mask_notset) {
        json_object_set_new(
            match_cond,
            "user_flag_mask_notset",
            json_integer(atol(arguments.user_flag_mask_notset))
        );
    }
    if(arguments.system_flag_mask_set) {
        json_object_set_new(
            match_cond,
            "system_flag_mask_set",
            json_integer(atol(arguments.system_flag_mask_set))
        );
    }
    if(arguments.system_flag_mask_notset) {
        json_object_set_new(
            match_cond,
            "system_flag_mask_notset",
            json_integer(atol(arguments.system_flag_mask_notset))
        );
    }
    if(arguments.key) {
        json_object_set_new(
            match_cond,
            "key",
            json_string(arguments.key)
        );
    }
    if(arguments.notkey) {
        json_object_set_new(
            match_cond,
            "notkey",
            json_string(arguments.notkey)
        );
    }
    if(arguments.rkey) {
        json_object_set_new(
            match_cond,
            "rkey",
            json_string(arguments.rkey)
        );
    }
    if(arguments.filter) {
        json_object_set_new(
            match_cond,
            "filter",
            legalstring2json(arguments.filter, TRUE)
        );
    }

//    if(json_object_size(match_cond)>0) {
//        json_object_set_new(match_cond, "only_md", json_true());
//    } else {
//        JSON_DECREF(match_cond)
//    }

    /*
     *  Do your work
     */
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
    if(arguments.list_databases) {
        list_databases(arguments.path);
    } else if(arguments.recursive) {
        list_recursive_topic_messages();
    } else {
        list_topic_messages();
    }

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    /*-------------------------------------*
     *  Print times
     *-------------------------------------*/
    MT_INCREMENT_COUNT(time_measure, total_counter)
    MT_PRINT_TIME(time_measure, "list keys")

    JSON_DECREF(match_cond)
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
    yev_loop->running = 0;
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
