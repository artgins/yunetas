/****************************************************************************
 *          TRANGER_SEARCH.C
 *
 *          Search messages of tranger database
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
#define NAME        "tranger_search"
#define DOC         "Search messages in TimeRanger database.\n" \
                    "Example: \n" \
                    "tranger_search \\\n" \
                    "  -a /yuneta/store/queues/frames/{{yuno_role}}^{{yuno_name}}/frames \\\n" \
                    "  --search-content-filter=base64 \\\n" \
                    "  --from-rowid=-10 \\\n" \
                    "  --search-content-key=frame64 \\\n" \
                    "  -l3"

#define VERSION     __ghelpers_version__
#define SUPPORT     "<niyamaka at yuneta.io>"
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

    char *search_content_key;
    char *search_content_filter;
    char *search_content_text;
    char *display_format;
};

typedef struct {
    struct arguments *arguments;
    json_t *match_cond;
} list_params_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);
PRIVATE int list_topics(const char *path);

/***************************************************************************
 *      Data
 ***************************************************************************/
struct arguments arguments;
int total_found = 0;
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
{"path",                'a',    "PATH",             0,      "Path of database/topic.",2},
{"database",            'b',    "DATABASE",         0,      "Tranger database name.",2},
{"topic",               'c',    "TOPIC",            0,      "Topic name.",      2},
{"recursive",           'r',    0,                  0,      "List recursively.",  2},

{0,                     0,      0,                  0,      "Presentation",     3},
{"verbose",             'l',    "LEVEL",            0,      "Verbose level (0=total, 1=metadata, 2=metadata+path, 3=metadata+record.", 3},
{"mode",                'm',    "MODE",             0,      "Mode: form or table", 3},
{"fields",              'f',    "FIELDS",           0,      "Print only this fields", 3},

{0,                     0,      0,                  0,      "Search record conditions", 4},
{"from-t",              1,      "TIME",             0,      "From time.",       4},
{"to-t",                2,      "TIME",             0,      "To time.",         4},
{"from-rowid",          4,      "TIME",             0,      "From rowid.",      5},
{"to-rowid",            5,      "TIME",             0,      "To rowid.",        5},

{"user-flag-set",       9,      "MASK",             0,      "Mask of User Flag set.",   6},
{"user-flag-not-set",   10,     "MASK",             0,      "Mask of User Flag not set.",6},

{"system-flag-set",     13,     "MASK",             0,      "Mask of System Flag set.",   7},
{"system-flag-not-set", 14,     "MASK",             0,      "Mask of System Flag not set.",7},

{"key",                 15,     "KEY",              0,      "Key.",             9},
{"not-key",             16,     "KEY",              0,      "Not key.",         9},

{"from-tm",             17,     "TIME",             0,      "From msg time.",       10},
{"to-tm",               18,     "TIME",             0,      "To msg time.",         10},

{0,                     0,      0,                  0,      "Search content conditions", 11},
{"search-content-key",  20,     "CONTENT-KEY",      0,      "Content key where to search.", 11},
{"search-content-filter", 21,   "CONTENT-FILTER",   0,      "Filter to apply to content (clear, base64,)", 11},
{"search-content-text", 22,     "CONTENT-TEXT",     0,      "Text to search in content.", 11},
{"diplay-format",       19,     "DISPLAY-FORMAT",   0,      "Display format (json, hexdump,)", 11},

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
    case 'r':
        arguments->recursive = 1;
        break;
    case 'l':
        if(arg) {
            arguments->verbose = atoi(arg);
        }
        break;
    case 'm':
        arguments->mode = arg;
        break;
    case 'f':
        arguments->fields = arg;
        break;

    case 1: // from_t
        arguments->from_t = arg;
        break;
    case 2: // to_t
        arguments->to_t = arg;
        break;

    case 4: // from_rowid
        arguments->from_rowid = arg;
        break;
    case 5: // to_rowid
        arguments->to_rowid = arg;
        break;

    case 9:
        arguments->user_flag_mask_set = arg;
        break;
    case 10:
        arguments->user_flag_mask_notset = arg;
        break;

    case 13:
        arguments->system_flag_mask_set = arg;
        break;
    case 14:
        arguments->system_flag_mask_notset = arg;
        break;

    case 15:
        arguments->key = arg;
        break;
    case 16:
        arguments->notkey = arg;
        break;

    case 17: // from_tm
        arguments->from_tm = arg;
        break;
    case 18: // to_tm
        arguments->to_tm = arg;
        break;

    case 20: // search-content-key
        arguments->search_content_key = arg;
        break;
    case 21: // search-content-filter
        arguments->search_content_filter = arg;
        break;
    case 22: // search-content-text
        arguments->search_content_text = arg;
        break;
    case 19: // diplay-format
        arguments->display_format = arg;
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
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
)
{
    char *p = strrchr(fullpath, '/');
    if(p) {
        *p = 0;
    }
    char topic_path[PATH_MAX];
    snprintf(topic_path, sizeof(topic_path), "%s", fullpath);
    printf("TimeRanger ==> '%s'\n", fullpath);
    printf("  TimeRanger database: '%s'\n", pop_last_segment(fullpath));
    printf("  Path: '%s'\n", fullpath);
    list_topics(topic_path);

    return TRUE; // to continue
}

PRIVATE int list_databases(const char *path)
{
    walk_dir_tree(
        path,
        "__timeranger__.json",
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
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
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
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    json_t *jn_record
)
{
    static BOOL first_time = TRUE;
    total_counter++;
    partial_counter++;
    int verbose = kw_get_int(list, "verbose", 0, KW_REQUIRED);
    char title[1024];

    print_md1_record(tranger, topic, md_record, title, sizeof(title));

    BOOL table_mode = TRUE; // same logic as tranger_list.c
    if(table_mode) {
        table_mode = TRUE;
    }

    if(!jn_record) {
        jn_record = tranger_read_record_content(tranger, topic, md_record);
    }
    const char *search_content_key = kw_get_str(list, "match_cond`search_content_key", "", 0);
    const char *search_content_filter = kw_get_str(list, "match_cond`search_content_filter", "", 0);
    const char *search_content_text = kw_get_str(list, "match_cond`search_content_text", "", 0);
    const char *display_format = kw_get_str(list, "match_cond`display_format", "", 0);

    GBUFFER *gbuf_value = kw_get_gbuf_value(jn_record, search_content_key, 0, 0);
    if(!gbuf_value) {
        JSON_DECREF(jn_record);
        return 0;
    }
    BOOL base64 = FALSE;
    SWITCHS(search_content_filter) {
        // Engine RPM - Engine Speed
        CASES("base64")
            {
                gbuf_value = gbuf_decodebase64(gbuf_value);
                base64 = TRUE;
            }
            break;
        DEFAULTS
            break;
    } SWITCHS_END;

    char *p = gbuf_cur_rd_pointer(gbuf_value);

    if(base64) {
        if(empty_string(search_content_text) || strstr(p, search_content_text)) {
            total_found++;
            if(verbose == 1) {
                printf("===> %s\n", title);
            }
            if(verbose == 2) {
                print_md2_record(tranger, topic, md_record, title, sizeof(title));
                printf("===> %s\n", title);
            }
            if(verbose == 3) {
                printf("===> %s\n", title);
                const char *key;
                json_t *jn_value;
                json_object_foreach(jn_record, key, jn_value) {
                    if(strcmp(search_content_key, key)==0) {
                        if(strcmp(display_format, "json")==0) {
                            json_t *jn = json_string(p);
                            char *s = json2uglystr(jn);
                            printf("\"%s\": %s\n", key, s);
                            gbmem_free(s);
                            json_decref(jn);

                        } else { // hexdump
                            printf("\"%s\":\n", key);
                            int l = gbuf_leftbytes(gbuf_value);
                            tdump2(p, l, printf);
                        }
                    } else {
                        char *s = json2uglystr(jn_value);
                        printf("\"%s\": %s\n", key, s);
                        gbmem_free(s);
                    }
                }
            }
        }
    } else if(empty_string(search_content_text) || strstr(p, search_content_text)) {
        total_found++;

        if(verbose == 1) {
            printf("%s\n", title);
        }
        if(verbose == 2) {
            print_md2_record(tranger, topic, md_record, title, sizeof(title));
            printf("%s\n", title);
        }
        if(!empty_string(arguments.fields)) {
            const char ** keys = 0;
            keys = split2(arguments.fields, ", ", 0);
            json_t *jn_record_with_fields = kw_clone_by_path(
                jn_record,   // owned
                keys
            );
            split_free2(keys);
            jn_record = jn_record_with_fields;

        }
        if(json_object_size(jn_record)>0 && verbose >= 3) {
            const char *key;
            json_t *jn_value;
            int len;
            int col;
            if(first_time) {
                first_time = FALSE;
                col = 0;
                json_object_foreach(jn_record, key, jn_value) {
                    len = strlen(key);
                    if(col == 0) {
                        printf("%*.*s", len, len, key);
                    } else {
                        printf(" %*.*s", len, len, key);
                    }
                    col++;
                }
                printf("\n");
                col = 0;
                json_object_foreach(jn_record, key, jn_value) {
                    len = strlen(key);
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
            json_object_foreach(jn_record, key, jn_value) {
                char *s = json2uglystr(jn_value);
                if(col == 0) {
                    printf("%s", s);
                } else {
                    printf(" %s", s);
                }
                gbmem_free(s);
                col++;
            }
            printf("\n");
        }
    }

    GBUF_DECREF(gbuf_value);
    JSON_DECREF(jn_record);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _search_messages(list_params_t *list_params)
{
    char *path = list_params->arguments->path;
    char *database = list_params->arguments->database;
    char *topic_name = list_params->arguments->topic;
    int verbose = list_params->arguments->verbose;
    json_t *match_cond = list_params->match_cond;

    /*-------------------------------*
     *  Startup TimeRanger
     *-------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:s}",
        "path", path,
        "database", database
    );
    json_t * tranger = tranger_startup(jn_tranger);
    if(!tranger) {
        fprintf(stderr, "Can't startup tranger %s/%s\n\n", path, database);
        exit(-1);
    }

    /*-------------------------------*
     *  Open topic
     *-------------------------------*/
    json_t * htopic = tranger_open_topic(
        tranger,
        topic_name,
        FALSE
    );
    if(!htopic) {
        fprintf(stderr, "Can't open topic %s\n\n", topic_name);
        exit(-1);
    }

    JSON_INCREF(match_cond);
    json_t *jn_list = json_pack("{s:s, s:o, s:I, s:i}",
        "topic_name", topic_name,
        "match_cond", match_cond?match_cond:json_object(),
        "load_record_callback", (json_int_t)(size_t)load_record_callback,
        "verbose", verbose
    );

    json_t *tr_list = tranger_open_list(
        tranger,
        jn_list
    );
    if(tr_list) {
        tranger_close_list(tranger, tr_list);
    }

    /*-------------------------------*
     *  Free resources
     *-------------------------------*/
    tranger_close_topic(tranger, topic_name);
    tranger_shutdown(tranger);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_topic_messages(list_params_t *list_params)
{
    char path_topic[PATH_MAX];

    build_path3(path_topic, sizeof(path_topic),
        list_params->arguments->path,
        list_params->arguments->database,
        list_params->arguments->topic
    );

    if(!file_exists(path_topic, "topic_desc.json")) {
        if(!is_directory(path_topic)) {
            fprintf(stderr, "Path not found: '%s'\n\n", path_topic);
            exit(-1);
        }
        fprintf(stderr, "What Database/Topic?\n\nFound:\n\n");
        list_databases(list_params->arguments->path);
        exit(-1);
    }
    list_params->arguments->topic = pop_last_segment(path_topic);
    list_params->arguments->database = pop_last_segment(path_topic);
    list_params->arguments->path = path_topic;
    return _search_messages(list_params);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_recursive_topic_cb(
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

    partial_counter = 0;
    pop_last_segment(fullpath);
    list_params->arguments->topic = pop_last_segment(fullpath);
    list_params->arguments->database = pop_last_segment(fullpath);
    list_params->arguments->path = fullpath;
    _search_messages(list_params);

    printf("====> %s %s: %d records\n\n",
        list_params->arguments->database,
        list_params->arguments->topic,
        partial_counter
    );

    return TRUE; // to continue
}

PRIVATE int list_recursive_topics(list_params_t *list_params)
{
    walk_dir_tree(
        list_params->arguments->path,
        "topic_desc.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_recursive_topic_cb,
        list_params
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_recursive_topic_messages(list_params_t *list_params)
{
    char path_tranger[PATH_MAX];

    build_path2(path_tranger, sizeof(path_tranger),
        list_params->arguments->path,
        list_params->arguments->database
    );

    if(!file_exists(path_tranger, "__timeranger__.json")) {
        if(!is_directory(path_tranger)) {
            fprintf(stderr, "Path not found: '%s'\n\n", path_tranger);
            exit(-1);
        }
        if(file_exists(path_tranger, "topic_desc.json")) {
            list_params->arguments->topic = pop_last_segment(path_tranger);
            list_params->arguments->database = pop_last_segment(path_tranger);
            list_params->arguments->path = path_tranger;
            return _search_messages(list_params);
        }

        fprintf(stderr, "What Database?\n\nFound:\n\n");
        list_databases(list_params->arguments->path);
        exit(-1);
    }

    list_params->arguments->topic = "";
    list_params->arguments->database = "";
    list_params->arguments->path = path_tranger;
    return list_recursive_topics(list_params);
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

    if(empty_string(arguments.search_content_key)) {
        printf("\nYou must input a key where search in his content"
            " (--search-content-key option)\n\n"
        );
        exit(-1);
    }

    uint64_t MEM_MAX_SYSTEM_MEMORY = free_ram_in_kb() * 1024LL;
    MEM_MAX_SYSTEM_MEMORY /= 100LL;
    MEM_MAX_SYSTEM_MEMORY *= 90LL;  // Coge el 90% de la memoria

    uint64_t MEM_MAX_BLOCK = (MEM_MAX_SYSTEM_MEMORY / sizeof(md_record_t)) * sizeof(md_record_t);

    MEM_MAX_BLOCK = MIN(1*1024*1024*1024LL, MEM_MAX_BLOCK);  // 1*G max

    gbmem_startup_system(
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY
    );
    json_set_alloc_funcs(
        gbmem_malloc,
        gbmem_free
    );

    log_startup(
        NAME,       // application name
        VERSION,    // applicacion version
        NAME        // executable program, to can trace stack
    );
    log_add_handler(NAME, "stdout", LOG_OPT_LOGGER, 0);


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
    if(arguments.from_tm) {
        timestamp_t timestamp;
        if(all_numbers(arguments.from_tm)) {
            timestamp = atoll(arguments.from_tm);
        } else {
            timestamp = approxidate(arguments.from_tm);
        }
        json_object_set_new(match_cond, "from_tm", json_integer(timestamp));
    }
    if(arguments.to_tm) {
        timestamp_t timestamp;
        if(all_numbers(arguments.to_tm)) {
            timestamp = atoll(arguments.to_tm);
        } else {
            timestamp = approxidate(arguments.to_tm);
        }
        json_object_set_new(match_cond, "to_tm", json_integer(timestamp));
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

    if(arguments.search_content_key) {
        json_object_set_new(
            match_cond,
            "search_content_key",
            json_string(arguments.search_content_key)
        );
    }
    if(arguments.search_content_filter) {
        json_object_set_new(
            match_cond,
            "search_content_filter",
            json_string(arguments.search_content_filter)
        );
    }
    if(arguments.search_content_text) {
        json_object_set_new(
            match_cond,
            "search_content_text",
            json_string(arguments.search_content_text)
        );
    }
    if(arguments.display_format) {
        json_object_set_new(
            match_cond,
            "display_format",
            json_string(arguments.display_format)
        );
    }

    if(json_object_size(match_cond)>0) {
        ;
    } else {
        JSON_DECREF(match_cond);
    }

    /*
     *  Do your work
     */
    struct timespec st, et;
    double dt;

    clock_gettime (CLOCK_MONOTONIC, &st);

    if(empty_string(arguments.path)) {
        fprintf(stderr, "What TimeRanger path?\n");
        fprintf(stderr, "You must supply --path option\n\n");
        exit(-1);
    }

    list_params_t list_params;
    memset(&list_params, 0, sizeof(list_params));
    list_params.arguments = &arguments;
    list_params.match_cond = match_cond;

    if(arguments.recursive) {
        list_recursive_topic_messages(&list_params);
    } else {
        list_topic_messages(&list_params);
    }

    JSON_DECREF(match_cond);

    clock_gettime (CLOCK_MONOTONIC, &et);

    /*-------------------------------------*
     *  Print times
     *-------------------------------------*/
    dt = ts_diff2(st, et);

    setlocale(LC_ALL, "");
    printf("====> Found %'d records in total of %'d;  %'f seconds; %'lu op/sec\n\n",
        total_found,
        total_counter,
        dt,
        (unsigned long)(((double)total_counter)/dt)
    );

    gbmem_shutdown();
    return 0;
}
