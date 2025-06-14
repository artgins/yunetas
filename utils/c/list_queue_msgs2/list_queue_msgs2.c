/****************************************************************************
 *          LIST_QUEUE_MSGS.C
 *
 *          List messages in message's queue.
 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <argp.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>

#include <gobj.h>
#include <testing.h>
#include <helpers.h>
#include <timeranger2.h>
#include <tr_queue.h>
#include <cpu.h>
#include <yev_loop.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "list_queue_msgs2"
#define DOC         "List messages in message's queue, for tranger2."

#define VERSION     "1.9.0"
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
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */

    char *path;

    int verbose;                /* verbose */
    int all;                    /* all message*/
    int print_local_time;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE error_t parse_opt (int key, char *arg, struct argp_state *state);

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
/*-name-------------key-----arg---------flags---doc-----------------group */
{"verbose",         'l',    "LEVEL",    0,      "Verbose level (empty=total, 1=metadata, 2=metadata+record)", 0},
{"all",             'a',    0,          0,      "List all messages, not only pending.", 0},
{"print-local-time",'t',    0,          0,      "Print in local time", 0},
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
    case 'l':
        arguments->verbose = atoi(arg);
        break;

    case 'a':
        arguments->all = 1;
        break;

    case 't':
        arguments->print_local_time = 1;
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
PRIVATE int list_queue_msgs(struct arguments *arguments)
{
    char path_tranger[PATH_MAX];

    build_path(path_tranger, sizeof(path_tranger),
        arguments->path,
        NULL
    );

    char *path = NULL;
    char *database = NULL;
    char *topic_name = NULL;

    if(file_exists(path_tranger, "topic_desc.json")) {
        topic_name = pop_last_segment(path_tranger);
        if(!file_exists(path_tranger, "__timeranger2__.json")) {
            fprintf(stderr, "Cannot find a timeranger2 in  %s\n\n", path_tranger);
            exit(-1);
        }
        database = pop_last_segment(path_tranger);
        path = path_tranger;
    } else {
        fprintf(stderr, "Cannot find any topic in  %s\n\n", arguments->path);
        exit(-1);
    }

    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    /*-------------------------------*
     *      Startup TimeRanger
     *-------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:s}",
        "path", path,
        "database", database
    );

    json_t * tranger = tranger2_startup(0, jn_tranger, 0);
    if(!tranger) {
        fprintf(stderr, "Can't startup tranger %s\n\n", arguments->path);
        exit(-1);
    }

    tr_queue trq_output = trq_open(
        tranger,
        topic_name,
        "tm",
        sf_string_key,
        0
    );
    if(!trq_output) {
        exit(-1);
    }

    /*
        In the topic, here we have the cache of keys

        "cache": {
            "DVES_0012D8": {
                "files": [
                    {
                        "id": "2025-06-10",
                        "fr_t": 1749553992,
                        "to_t": 1749555073,
                        "fr_tm": 1749553992,
                        "to_tm": 1749555073,
                        "rows": 193
                    }
                ],
                "total": {
                    "fr_t": 1749553992,
                    "to_t": 1749555073,
                    "fr_tm": 1749553992,
                    "to_tm": 1749555073,
                    "rows": 193
                }
            },
            ...
        }
    */

    if(arguments->all) {
        trq_load_all(trq_output, 0, 0);
    } else {
        trq_load(trq_output);
    }

    // size_t total_counter = trq_size(trq_output);
    uint64_t total_counter = tranger2_topic_size(tranger, topic_name);

    /*
        In the topic, here we have the fd of files

       "rd_fd_files": {
            "DVES_0012D8": {
                "2025-06-10.md2": 3
            },

        and the metadata & key of the records records in trq

    */
    int counter = 0;
    q_msg msg;
    qmsg_foreach_forward(trq_output, msg) {
        counter++;
        if(arguments->verbose == 1) {
            md2_record_ex_t *md_record = trq_msg_md(msg);
            char temp[1024];
            tranger2_print_md1_record(
                temp,
                sizeof(temp),
                "",
                trq_msg_rowid(msg),
                md_record,
                arguments->print_local_time
            );
            printf("%s\n", temp);

        } else if(arguments->verbose > 1) {
            md2_record_ex_t *md_record = trq_msg_md(msg);
            const json_t *jn_gate_msg = trq_msg_json(msg); // Return json is NOT YOURS!!

            char temp[1024];
            tranger2_print_md1_record(
                temp,
                sizeof(temp),
                "",
                trq_msg_rowid(msg),
                md_record,
                arguments->print_local_time
            );

            print_json2(temp, (json_t *)jn_gate_msg);
        }
    }

    /*-------------------------------------*
     *  Print times
     *-------------------------------------*/
    MT_SET_COUNT(time_measure, total_counter)
    MT_PRINT_TIME(time_measure, "list_queue_msgs2")

    if(counter > 0) {
        printf("%sTotal: %d records%s\n\n", On_Red BWhite, counter, Color_Off);
    } else {
        printf("Total: %d records\n\n", counter);
    }

    tranger2_shutdown(tranger);

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
    struct arguments arguments;

    /*
     *  Default values
     */
    memset(&arguments, 0, sizeof(arguments));
    arguments.verbose = 0;
    arguments.all = 0;
    arguments.path = 0;

    /*
     *  Parse arguments
     */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    arguments.path = arguments.args[0];
    // if(empty_string(arguments.timeranger)) {
    //     fprintf(stderr, "What timeranger?\n");
    //     exit(-1);
    // }
    // if(empty_string(arguments.topic)) {
    //     fprintf(stderr, "What topic?\n");
    //     exit(-1);
    // }

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

    gobj_setup_memory(
        MEM_MAX_BLOCK,  // max_block, largest memory block
        MEM_MAX_SYSTEM_MEMORY, // max_system_memory, maximum system memory
        false,
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
        NULL  // global_authenticate_parser
    );

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

    /*
     *  Do your work
     */
    if(empty_string(arguments.path)) {
        fprintf(stderr, "What TimeRanger path?\n");
        fprintf(stderr, "You must supply --path option\n\n");
        exit(-1);
    }

    char path[PATH_MAX];
    delete_right_slash(arguments.path);
    if(arguments.path[0] != '/' && arguments.path[0] != '.') {
        snprintf(path, sizeof(path), "./%s", arguments.path);
    } else {
        snprintf(path, sizeof(path), "%s", arguments.path);
    }
    arguments.path = path;

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

    return list_queue_msgs(&arguments);
}
