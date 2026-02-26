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
{"database",            'b',    "DATABASE",         0,      "Database.",        2},
{"topic",               'c',    "TOPIC",            0,      "Topic.",           2},
{"ids",                 'i',    "ID",               0,      "Id or list of id's.",2},
{"recursive",           'r',    0,                  0,      "List recursively.",  2},

{0,                     0,      0,                  0,      "Presentation",     3},
{"mode",                'm',    "MODE",             0,      "Mode: form or table", 3},
{"fields",              'f',    "FIELDS",           0,      "Print only this fields", 3},

{0,                     0,      0,                  0,      "Print",            4},
{"print-tranger",       5,      0,                  0,      "Print tranger json", 4},
{"print-treedb",        6,      0,                  0,      "Print treedb json", 4},

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
    case 'm':
        arguments->mode = arg;
        break;
    case 'f':
        arguments->fields = arg;
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
json_t *jn_filter = 0;
json_t *jn_options = 0;
const char **list_fields = 0;
BOOL table_mode = FALSE;

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
    char treedb_name[NAME_MAX];
    snprintf(treedb_name, sizeof(treedb_name), "%s", name);
    char *p = strstr(treedb_name, ".treedb_schema.json");
    if(p) {
        *p = 0;
    }
    printf("  directory: %s\n", directory);
    printf("  treedb   : %s\n", treedb_name);
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
 *  Helper: callback for auto-discovering the treedb schema file
 ***************************************************************************/
typedef struct {
    char treedb_name[NAME_MAX];
    int count;
} find_schema_t;

PRIVATE BOOL find_schema_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *name,
    int level,
    wd_option opt
)
{
    find_schema_t *fs = (find_schema_t *)user_data;
    fs->count++;
    if(fs->count == 1) {
        snprintf(fs->treedb_name, sizeof(fs->treedb_name), "%s", name);
        char *p = strstr(fs->treedb_name, ".treedb_schema.json");
        if(p) {
            *p = 0;
        }
    }
    return TRUE; // continue to count all
}

/***************************************************************************
 *  Resolve the tranger path, treedb database name, and topic
 *  from the user-supplied program parameters.
 *
 *  path:     always filled (positional argument from user)
 *  database: can be empty (-b option)
 *  topic:    can be empty (-c option)
 *
 *  If database or topic is empty, deduce from path knowing that:
 *    - A topic directory contains topic_desc.json
 *    - A treedb root contains __timeranger2__.json
 *      and {treedb_name}.treedb_schema.json
 *
 *  On success (return 0):
 *    resolved_path     = tranger root directory
 *    resolved_database = treedb name without suffix
 *    resolved_topic    = topic name (may be empty)
 *
 *  Returns -1 on failure (with error printed to stderr).
 ***************************************************************************/
PRIVATE int resolve_treedb_path(
    const char *path,
    const char *database,
    const char *topic,
    char *resolved_path,        size_t resolved_path_size,
    char *resolved_database,    size_t resolved_database_size,
    char *resolved_topic,       size_t resolved_topic_size
)
{
    snprintf(resolved_path, resolved_path_size, "%s", path);
    resolved_database[0] = '\0';
    resolved_topic[0] = '\0';

    /*
     *  Copy provided values, normalizing database name
     */
    if(!empty_string(topic)) {
        snprintf(resolved_topic, resolved_topic_size, "%s", topic);
    }
    if(!empty_string(database)) {
        snprintf(resolved_database, resolved_database_size, "%s", database);
        char *p = strstr(resolved_database, ".treedb_schema.json");
        if(p) {
            *p = 0;
        }
    }

    /*
     *  Step 1: If topic is empty, check if path ends at a topic directory
     */
    if(empty_string(resolved_topic) && file_exists(resolved_path, "topic_desc.json")) {
        const char *segment = pop_last_segment(resolved_path);
        snprintf(resolved_topic, resolved_topic_size, "%s", segment);
    }

    /*
     *  Step 2: Find the tranger root (directory with __timeranger2__.json)
     */
    if(!file_exists(resolved_path, "__timeranger2__.json")) {
        /*
         *  Not at tranger root — try popping one segment
         *  (could be a database or schema name embedded in the path)
         */
        if(empty_string(resolved_database)) {
            const char *segment = pop_last_segment(resolved_path);
            if(!empty_string(segment)) {
                snprintf(resolved_database, resolved_database_size, "%s", segment);
                char *p = strstr(resolved_database, ".treedb_schema.json");
                if(p) {
                    *p = 0;
                }
            }
        }
        if(!file_exists(resolved_path, "__timeranger2__.json")) {
            fprintf(stderr, "Cannot find timeranger2 database in '%s'\n\n", path);
            list_databases(resolved_path);
            return -1;
        }
    }

    if(!is_directory(resolved_path)) {
        fprintf(stderr, "Directory '%s' not found\n\n", resolved_path);
        return -1;
    }

    /*
     *  Step 3: If database still empty, auto-discover from schema files
     */
    if(empty_string(resolved_database)) {
        find_schema_t fs;
        memset(&fs, 0, sizeof(fs));
        walk_dir_tree(
            0,
            resolved_path,
            ".*\\.treedb_schema\\.json",
            WD_MATCH_REGULAR_FILE,
            find_schema_cb,
            &fs
        );
        if(fs.count == 0) {
            fprintf(stderr, "No treedb database found in '%s'\n\n", resolved_path);
            return -1;
        }
        if(fs.count > 1) {
            fprintf(stderr, "Multiple treedb databases found, specify --database\n\n");
            list_databases(resolved_path);
            return -1;
        }
        snprintf(resolved_database, resolved_database_size, "%s", fs.treedb_name);
    } else {
        /*
         *  Validate that the database schema file exists
         */
        char schema_file[NAME_MAX + 50];
        snprintf(schema_file, sizeof(schema_file), "%s.treedb_schema.json", resolved_database);
        if(!file_exists(resolved_path, schema_file)) {
            fprintf(stderr, "Database '%s' not found in '%s'\n\n",
                resolved_database, resolved_path);
            list_databases(resolved_path);
            return -1;
        }
    }

    return 0;
}

/***************************************************************************
 *  Open a tranger database at path, open the named treedb,
 *  and list its records.
 ***************************************************************************/
PRIVATE int _list_messages(
    const char *path,
    const char *treedb_name,
    const char *topic_name)
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
        const char *topic_name_; json_t *topic_data;
        json_object_foreach(treedb, topic_name_, topic_data) {
            if(!json_is_object(topic_data)) {
                // Can be __schema_version__ or other attribute
                continue;
            }
            if(!empty_string(topic_name)) {
                if(strcmp(topic_name, topic_name_)!=0) {
                    continue;
                }
            }

            printf("%s%s%s\n", On_Yellow BIWhite, topic_name_, Color_Off);

            json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name_);

            BOOL first_time = TRUE;
            json_t *iter = treedb_list_nodes( // Return MUST be decref
                tranger,
                treedb_name,
                topic_name_,
                json_incref(jn_filter),
                0  // match_fn
            );

            int idx; json_t *node;
            json_array_foreach(iter, idx, node) {
                json_t *jn_record = node_collapsed_view(
                    tranger,
                    node,
                    json_incref(jn_options)
                );

                if(table_mode) {
                    const char *key;
                    json_t *desc;
                    int len;
                    int col;
                    if(first_time) {
                        first_time = FALSE;
                        col = 0;
                        json_object_foreach(cols, key, desc) {
                            if(*key == '_') {
                                continue;
                            }
                            if(list_fields) {
                                if(!str_in_list(list_fields, key, FALSE)) {
                                    continue;
                                }
                            }

                            len = (int)kw_get_int(gobj, desc, "fillspace", (int)strlen(key), 0);
                            if(col == 0) {
                                printf("%-*.*s", len, len, key);
                            } else {
                                printf(" %-*.*s", len, len, key);
                            }
                            col++;
                        }
                        printf("\n");
                        col = 0;
                        json_object_foreach(cols, key, desc) {
                            if(*key == '_') {
                                continue;
                            }
                            if(list_fields) {
                                if(!str_in_list(list_fields, key, FALSE)) {
                                    continue;
                                }
                            }
                            len = (int)kw_get_int(gobj, desc, "fillspace", (int)strlen(key), 0);
                            if(col == 0) {
                                printf("%*.*s", len, len, "=======================================");
                            } else {
                                printf(" %*.*s", len, len, "=======================================");
                            }
                            col++;
                        }
                        printf("\n");
                    }
                    col = 0;

                    json_object_foreach(cols, key, desc) {
                        if(*key == '_') {
                            continue;
                        }
                        if(list_fields) {
                            if(!str_in_list(list_fields, key, FALSE)) {
                                continue;
                            }
                        }
                        len = (int)kw_get_int(gobj, desc, "fillspace", (int)strlen(key), 0);
                        json_t *jn_value = kw_get_dict_value(gobj, jn_record, key, 0, 0);
                        char *s = json2uglystr(jn_value);
                        if(col == 0) {
                            printf("%-*.*s", len, len, s);
                        } else {
                            printf(" %-*.*s", len, len, s);
                        }
                        GBMEM_FREE(s);
                        col++;
                    }
                    printf("\n");

                } else {
                    print_json(topic_name_, jn_record);
                }

                total_counter++;
                partial_counter++;
                printf("\n");
            }

            json_decref(cols);
            json_decref(iter);
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
 *  Resolve path/database/topic and list treedb records.
 ***************************************************************************/
PRIVATE int list_messages(
    const char *path,
    const char *database,
    const char *topic)
{
    char resolved_path[PATH_MAX];
    char resolved_database[NAME_MAX];
    char resolved_topic[NAME_MAX];

    if(resolve_treedb_path(
        path, database, topic,
        resolved_path, sizeof(resolved_path),
        resolved_database, sizeof(resolved_database),
        resolved_topic, sizeof(resolved_topic)
    ) != 0) {
        exit(-1);
    }

    return _list_messages(resolved_path, resolved_database, resolved_topic);
}

/***************************************************************************
 *  Callback for walk_dir_tree: called for each treedb schema found.
 *  Extracts treedb name from filename and lists its records.
 *  user_data carries the topic filter string.
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
    const char *topic = (const char *)user_data;

    // name is e.g. "mydb.treedb_schema.json" — extract treedb name
    char treedb_name[NAME_MAX];
    snprintf(treedb_name, sizeof(treedb_name), "%s", name);
    char *p = strstr(treedb_name, ".treedb_schema.json");
    if(p) {
        *p = 0;
    }

    partial_counter = 0;
    _list_messages(
        directory,
        treedb_name,
        topic
    );

    printf("====> %s: %d records\n\n", treedb_name, partial_counter);

    return TRUE; // to continue
}

PRIVATE int list_recursive_databases(const char *path, const char *topic)
{
    walk_dir_tree(
        0,
        path,
        ".*\\.treedb_schema\\.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_recursive_db_cb,
        (void *)topic
    );

    return 0;
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
     *  Match conditions from Arguments
     *----------------------------------*/
    if(arguments.mode) {
        if(strcasecmp(arguments.mode, "form")==0) {
            table_mode = FALSE;
        } else if(strcasecmp(arguments.mode, "table")==0) {
            table_mode = TRUE;
        }
    }
    if(!empty_string(arguments.fields)) {
        list_fields = split2(arguments.fields, ", ", 0);
        table_mode = TRUE;
    }

    /*----------------------------------*
     *  Ids
     *----------------------------------*/
    jn_filter = json_object();
    if(arguments.id) {
        int list_size;
        const char **ss = split2(arguments.id, ", ", &list_size);
        json_t *jn_ids = json_array();
        for(int i=0; i<list_size; i++) {
            json_array_append_new(jn_ids, json_string(ss[i]));
        }
        json_object_set_new(jn_filter, "id", jn_ids);
        split_free2(ss);
    }

    /*----------------------------------*
     *          Options
     *----------------------------------*/
    jn_options = json_pack("{s:b}", "with_metadata", TRUE);

    /*----------------------------------*
     *          Get path
     *----------------------------------*/
    char path[PATH_MAX];
    delete_right_slash(arguments.path);
    if(arguments.path[0] != '/' && arguments.path[0] != '.') {
        snprintf(path, sizeof(path), "./%s", arguments.path);
    } else {
        snprintf(path, sizeof(path), "%s", arguments.path);
    }
    arguments.path = path;

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

    if(arguments.recursive) {
        list_recursive_databases(
            arguments.path,
            arguments.topic
        );
    } else {
        list_messages(
            arguments.path,
            arguments.database,
            arguments.topic
        );
    }
    JSON_DECREF(jn_filter);
    JSON_DECREF(jn_options);
    split_free2(list_fields);

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
