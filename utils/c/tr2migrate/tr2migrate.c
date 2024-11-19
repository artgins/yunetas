/****************************************************************************
 *          tr2migrate.c
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

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <copyfile.h>
#elif defined(__linux__)
#include <sys/sendfile.h>
#endif

#include <gobj.h>
#include <testing.h>
#include <helpers.h>
#include <kwid.h>
#include <timeranger2.h>
#include <stacktrace_with_bfd.h>
#include <cpu.h>
#include <yev_loop.h>

#include "30_timeranger.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "tr2migrate"
#define DOC         "Migrate a timeranger to timeranger2"

#define VERSION     "1.0" // __ghelpers_version__
#define SUPPORT     "<niyamaka at yuneta.io>"
#define DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Arguments
 ***************************************************************************/
#define MIN_ARGS 2
#define MAX_ARGS 2
struct arguments {
    char *args[MAX_ARGS+1];     /* positional args */

    int verbose;
    char *source;
    char *destine;
};

const char *argp_program_version = NAME " " VERSION;
const char *argp_program_bug_address = SUPPORT;

/* Program documentation. */
static char doc[] = DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "PATH_TRANGER PATH_TRANGER2";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-----------------key-----arg-----------------flags---doc-----------------group */
{"verbose",             'l',    "LEVEL",            0,      "Verbose", 0},
{0}
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    /*
     *  Get the input argument from argp_parse,
     *  which we know is a pointer to our arguments structure.
     */
    struct arguments *arguments_ = state->input;

    switch (key) {
    case 'l':
        if(arg) {
            arguments_->verbose = atoi(arg);
        }
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
PUBLIC void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
json_t *tranger1 = 0;
json_t *tranger2 = 0;
json_t *topic2 = 0;
size_t topic_records = 0;
size_t total_counter = 0;

yev_loop_t *yev_loop;

/***************************************************************************
 *
 ***************************************************************************/
//PRIVATE BOOL list_topic_cb(
//    hgobj gobj,
//    void *user_data,
//    wd_found_type type,     // type found
//    char *fullpath,         // directory+filename found
//    const char *directory,  // directory of found filename
//    char *name,             // dname[255]
//    int level,              // level of tree where file found
//    wd_option opt           // option parameter
//)
//{
//    char *p = strrchr(directory, '/');
//    if(p) {
//        printf("        %s\n", p+1);
//    } else {
//        printf("        %s\n", directory);
//    }
//    return TRUE; // to continue
//}
//
//PRIVATE int list_topics(const char *path)
//{
//    printf("    Topics:\n");
//    walk_dir_tree(
//        0,
//        path,
//        "topic_desc.json",
//        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
//        list_topic_cb,
//        0
//    );
//    printf("\n");
//    return 0;
//}

/***************************************************************************
 *
 ***************************************************************************/
//PRIVATE int load_record_callback(
//    json_t *tranger,
//    json_t *topic,
//    const char *key,
//    json_t *list, // iterator or rt_list/rt_disk id, don't own
//    json_int_t rowid,
//    md2_record_ex_t *md_record,
//    json_t *jn_record  // must be owned
//)
//{
//    JSON_DECREF(jn_record)
//    return 0;
//}

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
 *  Copy file in kernel mode.
 ***************************************************************************/
PRIVATE int copy_file(const char *source, const char *destination)
{
    int input, output;
    if ((input = open(source, O_RDONLY)) == -1) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot open source file",
            "path",         "%s", source,
            NULL
        );
        return -1;
    }

    if ((output = newfile(destination, 0660, FALSE)) == -1) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot create destine file",
            "path",         "%s", destination,
            NULL
        );
        close(input);
        return -1;
    }

    struct stat file_stat;
    if (fstat(input, &file_stat)<0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "fstat() FAILE",
            "path",         "%s", source,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        close(input);
        return -1;
    }

    off_t offset = 0;
    ssize_t bytes_copied;
    while (offset < file_stat.st_size) {
        bytes_copied = sendfile(output, input, &offset, file_stat.st_size - offset);
        if (bytes_copied<0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Failed to copy file contents using sendfile",
                "path",         "%s", destination,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            close(input);
            close(output);
            return -1;
        }
    }

    if (fchown(output, file_stat.st_uid, file_stat.st_gid) == -1) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "fchown() FAILED",
            "path",         "%s", destination,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }

    close(input);
    close(output);

    if (chmod(destination, file_stat.st_mode) == -1) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "chmod() FAILED",
            "path",         "%s", destination,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL copy_files_first_level_cb(
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
    char *destine = user_data;

    char path_source[PATH_MAX];
    char path_destine[PATH_MAX];

    if(strcmp(name, "__timeranger__.json")==0) {
        build_path(path_source, sizeof(path_source), directory, name, NULL);
        json_t *timeranger_file = load_json_from_file(0, directory,  name, 0);
        if(!timeranger_file) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Cannot read tranger file",
                "path",         "%s", path_source,
                NULL
            );
            gobj_set_exit_code(-1);
            return FALSE;
        }

        json_t *jn_tranger = json_pack("{s:s, s:s, s:i, s:i, s:i, s:b}",
            "path", destine,
            "filename_mask", kw_get_str(0, timeranger_file, "filename_mask", "", KW_REQUIRED),
            "xpermission", (int)kw_get_int(0, timeranger_file, "xpermission", 0,  KW_REQUIRED),
            "rpermission", (int)kw_get_int(0, timeranger_file, "rpermission", 0,  KW_REQUIRED),
            "on_critical_error", -1,
            "master", 1
        );

        tranger2 = tranger2_startup(
            0,
            jn_tranger, // owned, See tranger2_json_desc for parameters
            yev_loop
        );
        if(!tranger2) {
            exit(-1);
        }
        return TRUE; // to continue

    } else {
        build_path(path_source, sizeof(path_source), directory, name, NULL);
        build_path(path_destine, sizeof(path_destine), destine, name, NULL);
    }
    if(copy_file(path_source, path_destine)<0) {
        exit(-1);
    }

    return TRUE; // to continue
}

PRIVATE int copy_files_first_level(char *source, char *destine)
{
    walk_dir_tree(
        0,
        source,
        ".*",
        WD_MATCH_REGULAR_FILE,
        copy_files_first_level_cb,
        destine
    );
    printf("\n");
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
int load_record_callback1(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md_record_t *md_record,
    /*
     *  can be null if only_md (use tranger_read_record_content() to load content)
     */
    json_t *jn_record // must be owned
)
{
    md2_record_ex_t md2_record_ex;

    topic_records++;
    total_counter++;

    tranger2_append_record(
        tranger2,
        tranger_topic_name(topic),
        md_record->__t__,
        md_record->__user_flag__,
        &md2_record_ex, // required
        jn_record       // owned
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL copy_topics_cb(
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
//    char *destine = user_data;

    char path_source[PATH_MAX];

    /*
     *  Check if exist the file of topic
     *  topic_cols.json  topic_desc.json  topic_var.json
     */
    build_path(path_source, sizeof(path_source), fullpath, "topic_desc.json", NULL);
    if(!is_regular_file(path_source)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "topic_desc.json NOT FOUND",
            "path",         "%s", path_source,
            NULL
        );
        return TRUE;
    }

    /*
     *  topic_cols.json  topic_desc.json  topic_var.json
     */
    json_t *topic_desc = load_json_from_file(0, fullpath,  "topic_desc.json", 0);

    json_t *topic_cols = 0;
    if(file_exists(fullpath, "topic_cols.json")) {
        topic_cols = load_json_from_file(0, fullpath,  "topic_cols.json", 0);
    }

    json_t *topic_var = 0;
    if(file_exists(fullpath, "topic_var.json")) {
        topic_var = load_json_from_file(0, fullpath,  "topic_var.json", 0);
    }

    const char *topic_name = kw_get_str(0, topic_desc, "topic_name", "", KW_REQUIRED);
    const char *pkey = kw_get_str(0, topic_desc, "pkey", "", KW_REQUIRED);
    const char *tkey = kw_get_str(0, topic_desc, "tkey", "", KW_REQUIRED);
    int system_flag = (int)kw_get_int(0, topic_desc, "system_flag", 0, KW_REQUIRED);

    json_t *topic1 = tranger_open_topic( // WARNING returned json IS NOT YOURS
        tranger1,
        name,
        TRUE
    );
    if(!topic1) {
        exit(-1);
    }

    topic_records = 0;
    topic2 = tranger2_create_topic( // WARNING returned json IS NOT YOURS
        tranger2,    // If the topic exists then only needs (tranger, topic_name) parameters
        topic_name,
        pkey,
        tkey,
        0,      // owned, See topic_json_desc for parameters
        system_flag,
        topic_cols,    // owned
        topic_var      // owned
    );

    json_t *list1 = tranger_open_list(
        tranger1,
        json_pack("{s:s, s:I}",
            "topic_name", topic_name,
            "load_record_callback", load_record_callback1
        )
    );

    tranger_close_list(tranger1, list1);
    topic2 = 0;

    json_decref(topic_desc);

    printf("Migrated topic: %s, records: %ld\n", topic_name, topic_records);

    return TRUE; // to continue
}

PRIVATE int copy_topics(char *source, char *destine)
{
    walk_dir_tree(
        0,
        source,
        ".*",
        WD_MATCH_DIRECTORY,
        copy_topics_cb,
        destine
    );
    printf("\n");
    return 0;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
PRIVATE int migrate(char *source, char *destine)
{
    char path[PATH_MAX];

    build_path(path, sizeof(path), source, "__timeranger__.json", NULL);
    if(!is_regular_file(path)) {
        fprintf(stderr, "TimeRanger __timeranger__.json NOT FOUND: %s\n", path);
        exit(-1);
    }

    if(mkrdir(arguments.destine, 02775)<0) {
        fprintf(stderr, "Cannot create destination path: %s\n", arguments.destine);
        exit(-1);
    }

    /*
     *  Copy files of first level, include __timeranger__ and possible schemas
     */
    copy_files_first_level(source, destine);

    tranger1 = tranger_startup(
        json_pack("{s:s}", "path", source)
    );
    if(!tranger1) {
        exit(-1);
    }

    /*
     *  Copy topics
     */
    copy_topics(source, destine);

    return 0;
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
    arguments.source = arguments.args[0];
    arguments.destine = arguments.args[1];

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
    init_backtrace_with_bfd(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_bfd);
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

    /*
     *  Do your work
     */
    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    delete_right_slash(arguments.source);
    delete_right_slash(arguments.destine);

    if(empty_string(arguments.source)) {
        fprintf(stderr, "What TimeRanger source path?\n");
        exit(-1);
    }
    if(empty_string(arguments.destine)) {
        fprintf(stderr, "What TimeRanger2 destine path?\n");
        exit(-1);
    }

    if(!is_directory(arguments.source)) {
        fprintf(stderr, "Source path is not a directory: %s\n", arguments.source);
        exit(-1);
    }
    if(access(arguments.destine, 0)==0) {
        fprintf(stderr, "Destination path already exists: %s\n", arguments.destine);
        exit(-1);
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

    migrate(arguments.source, arguments.destine);

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    /*-------------------------------------*
     *  Print times
     *-------------------------------------*/
    MT_INCREMENT_COUNT(time_measure, total_counter)
    MT_PRINT_TIME(time_measure, "migration")

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

    alarm(1);
}
