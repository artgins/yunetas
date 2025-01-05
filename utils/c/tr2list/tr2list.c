/****************************************************************************
 *          tr2list.c
 *
 *          List messages of tranger database
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <limits.h>
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
#include <argtable3.h>
#include <timeranger2.h>
#include <stacktrace_with_backtrace.h>
#include <cpu.h>
#include <yev_loop.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define APP         "tr2list"
#define DOC         "List messages of TimeRanger2 database.\n Examples TIME:\n  1.seconds (minutes,hours,days,weeks,months,years)"

#define VERSION     "1.0" // __ghelpers_version__
#define SUPPORT     "<niyamaka at yuneta.io>"
#define DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Arguments
 ***************************************************************************/
#define MIN_ARGS 1
#define MAX_ARGS 1
struct arguments {
    char *path;
    char *database;
    char *topic;
    int recursive;
    char *mode;
    char *fields;
    int print_local_time;
    int verbose;
    int show_md2;

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

PRIVATE struct arguments arguments;

/**
 * Parse arguments using argtable3.
 */
void parse_arguments(int argc, char **argv, struct arguments *arguments) {
    struct arg_str *path = arg_str1(NULL, NULL, "PATH", "Database path (required)");
    struct arg_lit *recursive = arg_lit0("r", "recursive", "List recursively.");
    struct arg_lit *print_local_time = arg_lit0("t", "print-local-time", "Print in local time.");
    struct arg_int *verbose = arg_int0("l", "verbose", "LEVEL", "Verbose level.");
    struct arg_str *mode = arg_str0("m", "mode", "MODE", "Mode: form or table.");
    struct arg_str *fields = arg_str0("f", "fields", "FIELDS", "Print only these fields.");
    struct arg_lit *show_md2 = arg_lit0("d", "show-md2", "Show __md_tranger__ in records.");
    struct arg_str *from_t = arg_str0(NULL, "from-t", "TIME", "From time.");
    struct arg_str *to_t = arg_str0(NULL, "to-t", "TIME", "To time.");
    struct arg_str *from_rowid = arg_str0(NULL, "from-rowid", "ROWID", "From rowid.");
    struct arg_str *to_rowid = arg_str0(NULL, "to-rowid", "ROWID", "To rowid.");
    struct arg_str *user_flag_mask_set = arg_str0(NULL, "user-flag-set", "MASK", "Mask of User Flag set.");
    struct arg_str *user_flag_mask_notset = arg_str0(NULL, "user-flag-not-set", "MASK", "Mask of User Flag not set.");
    struct arg_str *system_flag_mask_set = arg_str0(NULL, "system-flag-set", "MASK", "Mask of System Flag set.");
    struct arg_str *system_flag_mask_notset = arg_str0(NULL, "system-flag-not-set", "MASK", "Mask of System Flag not set.");
    struct arg_str *key = arg_str0(NULL, "key", "KEY", "Key.");
    struct arg_str *notkey = arg_str0(NULL, "not-key", "KEY", "Not key.");
    struct arg_str *from_tm = arg_str0(NULL, "from-tm", "TIME", "From message time.");
    struct arg_str *to_tm = arg_str0(NULL, "to-tm", "TIME", "To message time.");
    struct arg_str *rkey = arg_str0(NULL, "rkey", "RKEY", "Regular expression of Key.");
    struct arg_str *filter = arg_str0(NULL, "filter", "FILTER", "Filter of fields in JSON dict string.");
    struct arg_lit *list_databases = arg_lit0(NULL, "list-databases", "List databases.");
    struct arg_lit *help = arg_lit0("h", "help", "Display this help and exit");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        path, recursive, print_local_time, verbose, mode, fields, show_md2,
        from_t, to_t, from_rowid, to_rowid, user_flag_mask_set, user_flag_mask_notset,
        system_flag_mask_set, system_flag_mask_notset, key, notkey, from_tm, to_tm,
        rkey, filter, list_databases, help, end
    };

    if (arg_nullcheck(argtable) != 0) {
        fprintf(stderr, "Error: insufficient memory\n");
        exit(1);
    }

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        printf("%s %s\n\n%s\n\n", APP, VERSION, DOC);
        printf("Usage: ");
        arg_print_syntax(stdout, argtable, "\n");
        printf("\nOptions:\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        exit(0);
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        fprintf(stderr, "Usage: ");
        arg_print_syntax(stderr, argtable, "\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        exit(1);
    }

    // Populate the arguments structure
    arguments->path = strdup(*path->sval);
    arguments->recursive = recursive->count > 0;
    arguments->print_local_time = print_local_time->count > 0;
    arguments->verbose = verbose->count > 0 ? *verbose->ival : -1;
    arguments->mode = mode->count > 0 ? strdup(*mode->sval) : NULL;
    arguments->fields = fields->count > 0 ? strdup(*fields->sval) : NULL;
    arguments->show_md2 = show_md2->count > 0;
    arguments->from_t = from_t->count > 0 ? strdup(*from_t->sval) : NULL;
    arguments->to_t = to_t->count > 0 ? strdup(*to_t->sval) : NULL;
    arguments->from_rowid = from_rowid->count > 0 ? strdup(*from_rowid->sval) : NULL;
    arguments->to_rowid = to_rowid->count > 0 ? strdup(*to_rowid->sval) : NULL;
    arguments->user_flag_mask_set = user_flag_mask_set->count > 0 ? strdup(*user_flag_mask_set->sval) : NULL;
    arguments->user_flag_mask_notset = user_flag_mask_notset->count > 0 ? strdup(*user_flag_mask_notset->sval) : NULL;
    arguments->system_flag_mask_set = system_flag_mask_set->count > 0 ? strdup(*system_flag_mask_set->sval) : NULL;
    arguments->system_flag_mask_notset = system_flag_mask_notset->count > 0 ? strdup(*system_flag_mask_notset->sval) : NULL;
    arguments->key = key->count > 0 ? strdup(*key->sval) : NULL;
    arguments->notkey = notkey->count > 0 ? strdup(*notkey->sval) : NULL;
    arguments->from_tm = from_tm->count > 0 ? strdup(*from_tm->sval) : NULL;
    arguments->to_tm = to_tm->count > 0 ? strdup(*to_tm->sval) : NULL;
    arguments->rkey = rkey->count > 0 ? strdup(*rkey->sval) : NULL;
    arguments->filter = filter->count > 0 ? strdup(*filter->sval) : NULL;
    arguments->list_databases = list_databases->count > 0;

    // Free memory
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PUBLIC void yuno_catch_signals(void);
PRIVATE int list_topics(const char *path);

/***************************************************************************
 *      Data
 ***************************************************************************/
yev_loop_h yev_loop;
int time2exit = 10;
int total_counter = 0;
int partial_counter = 0;
json_t *match_cond = 0;

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
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *jn_record  // must be owned
)
{
    static BOOL first_time = TRUE;
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

    total_counter++;
    partial_counter++;
    int verbose = arguments.verbose;
    char title[1024];

    tranger2_print_md1_record(title, sizeof(title), key, md_record, arguments.print_local_time);

    BOOL table_mode = FALSE;
    if(!empty_string(arguments.mode) || !empty_string(arguments.fields)) {
        verbose = 3;
        table_mode = TRUE;
    }
    if(kw_has_key(match_cond, "filter")) {
        verbose = 3;
    }

    if(verbose < 0) {
        JSON_DECREF(jn_record)
        return 0;
    }
    if(verbose == 0) {
        tranger2_print_md0_record(title, sizeof(title), key, md_record, arguments.print_local_time);
        printf("%s\n", title);
        JSON_DECREF(jn_record)
        return 0;
    }
    if(verbose == 1) {
        printf("%s\n", title);
        JSON_DECREF(jn_record)
        return 0;
    }
    if(verbose == 2) {
        tranger2_print_md2_record(title, sizeof(title), tranger, topic, key, md_record, arguments.print_local_time);
        printf("%s\n", title);
        JSON_DECREF(jn_record)
        return 0;
    }

    /*
     *  Here is verbose 3
     */
    if(!arguments.show_md2) {
        json_object_del(jn_record, "__md_tranger__");
    }

    if(kw_has_key(match_cond, "filter")) {
        json_t *fields2match = kw_get_dict(gobj, match_cond, "filter", 0, KW_REQUIRED);
        json_t *record1 = kw_clone_by_keys(gobj, json_incref(jn_record), json_incref(fields2match), FALSE);
        if(!json_equal(
            record1,        // NOT owned
            fields2match    // NOT owned
        )) {
            total_counter--;
            partial_counter--;
            JSON_DECREF(record1)
            JSON_DECREF(jn_record)
            return 0;
        }
        JSON_DECREF(record1);
    }

    if(table_mode) {
        if(!empty_string(arguments.fields)) {
            tranger2_print_md0_record(title, sizeof(title), key, md_record, arguments.print_local_time);
            const char ** keys = 0;
            keys = split2(arguments.fields, ", ", 0);
            json_t *jn_record_with_fields = kw_clone_by_path(
                gobj,
                jn_record,   // owned
                keys
            );
            split_free2(keys);
            jn_record = jn_record_with_fields;

        }
        if(json_object_size(jn_record)>0) {
            const char *key;
            json_t *jn_value;
            int len;
            int col;
            if(first_time) {
                first_time = FALSE;
                col = 0;
                json_object_foreach(jn_record, key, jn_value) {
                    len = (int)strlen(key);
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

            printf("%s ", title);
            json_object_foreach(jn_record, key, jn_value) {
                char *s = json2uglystr(jn_value);
                if(col == 0) {
                    printf("%s", s);
                } else {
                    printf(" %s", s);
                }
                GBMEM_FREE(s);
                col++;
            }
            printf("\n");
        }

    } else {
        print_json2(title, jn_record);
    }

    JSON_DECREF(jn_record)

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

    json_object_set_new(
        match_cond,
        "load_record_callback",
        json_integer((json_int_t)(size_t)load_record_callback)
    );

    if(arguments.key) {
        json_t *rt = tranger2_open_iterator(
            tranger,
            arguments.topic,
            arguments.key,
            json_incref(match_cond), // owned
            load_record_callback,
            NULL,   // rt_id
            NULL,   // creator
            NULL,   // data
            NULL    // extra
        );
        tranger2_close_iterator(tranger, rt);

    } else {
        json_t *rt = tranger2_open_list(
            tranger,
            arguments.topic,
            json_incref(match_cond), // owned
            NULL,   // extra
            NULL,   // rt_id
            FALSE,  // rt_by_disk
            NULL    // creator
        );
        tranger2_close_list(tranger, rt);
    }

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
    parse_arguments(argc, argv, &arguments);

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
     *  Match conditions from Arguments
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

    json_decref(match_cond);

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
