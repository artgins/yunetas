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

#include <gobj.h>
#include <testing.h>
#include <helpers.h>
#include <kwid.h>
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
#define MIN_ARGS 0
#define MAX_ARGS 0
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */
    int verbose;                /* verbose */
    int all;                    /* all message*/
    char *timeranger;
    char *topic;
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
static char args_doc[] = "";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-------------key-----arg---------flags---doc-----------------group */
{"verbose",         'l',    0,          0,      "Verbose mode.",    0},
{"all",             'a',    0,          0,      "List all messages, not only pending.", 0},
{0,                 0,      0,          0,      "Database keys",    4},
{"timeranger",      'd',    "STRING",   0,      "Timeranger.",       4},
{"topic",           'p',    "STRING",   0,      "Topic.",           4},
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
        arguments->verbose = 1;
        break;

    case 'a':
        arguments->all = 1;
        break;

    case 'd':
        arguments->timeranger = arg;
        break;
    case 'p':
        arguments->topic = arg;
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
PRIVATE int list_queue_msgs(
    const char *timeranger,
    const char *topic_name,
    int all,
    int verbose)
{
    /*-------------------------------*
     *      Startup TimeRanger
     *-------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:s}",
        "path", timeranger,
        "database", ""
    );

    json_t * tranger = tranger2_startup(0, jn_tranger, 0);
    if(!tranger) {
        fprintf(stderr, "Can't startup tranger %s\n\n", timeranger);
        exit(-1);
    }

    tr_queue trq_output = trq_open(
        tranger,
        topic_name,
        "id",
        "tm",
        sf_string_key,
        0
    );
    if(!trq_output) {
        exit(-1);
    }

    if(all) {
        trq_load_all(trq_output, 0, 0, 0);
    } else {
        trq_load(trq_output);
    }

    int counter = 0;
    q_msg msg;
    qmsg_foreach_forward(trq_output, msg) {
        counter++;
        if(verbose) {
            md2_record_ex_t *md_record = trq_msg_md(msg);
            const json_t *jn_gate_msg = trq_msg_json(msg); // Return json is NOT YOURS!!
            const char *key = trq_msg_key(msg);
            uint32_t mark_ = md_record->user_flag;
            time_t t = (time_t)trq_msg_time(msg);

            char fecha[80];
            tm2timestamp(fecha, sizeof(fecha), gmtime(&t));
            char title[256];
            snprintf(title, sizeof(title), "t: %s, mark: 0x%"PRIX32", key: %s  ",
                fecha, mark_, key
            );
            print_json2(title, (json_t *)jn_gate_msg);
        }
    }

    if(counter > 0) {
        printf("%sTotal: %d records%s\n\n", On_Red BWhite, counter, Color_Off);
    } else {
        printf("Total: %d records\n\n", counter);
    }

    tranger2_shutdown(tranger);

    return 0;
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
    arguments.timeranger = 0;

    /*
     *  Parse arguments
     */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    if(empty_string(arguments.timeranger)) {
        fprintf(stderr, "What timeranger?\n");
        exit(-1);
    }
    if(empty_string(arguments.topic)) {
        fprintf(stderr, "What topic?\n");
        exit(-1);
    }

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
        NULL, // persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL, // global_authenticate_parser
        MEM_MAX_BLOCK,  // max_block, largest memory block
        MEM_MAX_SYSTEM_MEMORY, // max_system_memory, maximum system memory
        FALSE,
        0,
        0
    );

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);


    /*
     *  Do your work
     */
    return list_queue_msgs(
        arguments.timeranger,
        arguments.topic,
        arguments.all,
        arguments.verbose
    );
}
