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
#include <gobj.h>
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

yev_loop_t *yev_loop;

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
 *                      Main
 ***************************************************************************/
PRIVATE int migrate(const char *source, const char *destine)
{

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
    struct timespec st, et;
    double dt;

    clock_gettime (CLOCK_MONOTONIC, &st);

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
        fprintf(stderr, "source path is not a directory: %s\n", arguments.source);
        exit(-1);
    }
    if(access(arguments.destine, 0)==0) {
        fprintf(stderr, "destine path is already exists: %s\n", arguments.destine);
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

    clock_gettime (CLOCK_MONOTONIC, &et);

    /*-------------------------------------*
     *  Print times
     *-------------------------------------*/
    dt = ts_diff2(st, et);

    setlocale(LC_ALL, "");
    printf("====>  %'f seconds;\n\n",
        dt
    );

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
