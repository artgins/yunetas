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
#include <ghelpers.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "list_queue_msgs"
#define DOC         "List messages in message's queue."

#define VERSION     "1.9.0"
#define SUPPORT     "<niyamaka at yuneta.io>"
#define DATETIME    __DATE__ " " __TIME__

#define MEM_MIN_BLOCK           512
#define MEM_MAX_BLOCK           209715200   // 200*M
#define MEM_SUPERBLOCK          209715200   // 200*M
#define MEM_MAX_SYSTEM_MEMORY   4294967296  // 4*G

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
static error_t parse_opt (int key, char *arg, struct argp_state *state);

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
{"verbose",         'l',    0,          0,      "Verbose mode."},
{"all",             'a',    0,          0,      "List all messages, not only pending."},
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
    json_t *tranger = tranger_startup(
        json_pack("{s:s, s:s, s:b}",
            "path", timeranger,
            "database", "",
            "master", 0
        )
    );
    if(!tranger) {
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
            md_record_t md_record = trq_msg_md_record(msg);
            const json_t *jn_gate_msg = trq_msg_json(msg); // Return json is NOT YOURS!!
            const char *key = md_record.key.s;
            uint32_t mark_ = md_record.__user_flag__;
            time_t t = trq_msg_time(msg);


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

    tranger_shutdown(tranger);

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

    gbmem_startup_system(
        MEM_MAX_BLOCK,
        (INT_MAX < MEM_MAX_SYSTEM_MEMORY)? INT_MAX:MEM_MAX_SYSTEM_MEMORY
    );
//     gbmem_startup( /* Create memory core */
//         MEM_MIN_BLOCK,
//         MEM_MAX_BLOCK,
//         MEM_SUPERBLOCK,
//         MEM_MAX_SYSTEM_MEMORY,
//         NULL,               /* system memory functions */
//         0
//     );
    json_set_alloc_funcs(
        gbmem_malloc,
        gbmem_free
    );

    log_startup(
        "test",             // application name
        "1.9.0",            // applicacion version
        "test_glogger"     // executable program, to can trace stack
    );
    log_add_handler("test_stdout", "stdout", LOG_OPT_LOGGER, 0);

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
