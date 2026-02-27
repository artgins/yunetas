/****************************************************************************
 *          JSON_DIFF.C
 *
 *          Compare two json files. Items can be disordered.
 *
 *          Copyright (c) 2020 Niyamaka.
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
#include <ghelpers.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define APP_NAME    "json_diff"
#define DOC         "Compare two json files. Items can be disordered."

#define VERSION     __ghelpers_version__
#define SUPPORT     "<niyamaka at yuneta.io>"
#define DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    char file1[256];
    char file2[256];
    int without_metadata;
    int without_private;
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

    char *file1;
    char *file2;
    int without_metadata;
    int without_private;
    int verbose;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *      Data
 ***************************************************************************/
struct arguments arguments;
int total_counter = 0;
int partial_counter = 0;
const char *argp_program_version = APP_NAME " " VERSION;
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
{"file1",               'a',    "PATH",             0,      "Json file 1",      2},
{"file2",               'b',    "PATH",             0,      "Json file 2",      2},
{"without_metadata",    'm',    0,                  0,      "Without metadata (__* fields)",  2},
{"without_private",     'p',    0,                  0,      "Without private (_* fields)",  2},
{"verbose",             'v',    0,                  0,      "Verbose",  2},
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
        arguments->file1= arg;
        break;
    case 'b':
        arguments->file2= arg;
        break;
    case 'm':
        if(arg) {
            arguments->without_metadata = atoi(arg);
        }
        break;
    case 'p':
        if(arg) {
            arguments->without_private = atoi(arg);
        }
        break;
    case 'v':
        arguments->verbose = 1;
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

    /*-------------------------------------*
     *  Your start code
     *-------------------------------------*/
    init_ghelpers_library(APP_NAME);
    log_startup(
        "test",             // application name
        "1.0.0",            // applicacion version
        "test_glogger"     // executable program, to can trace stack
    );

    /*------------------------------------------------*
     *          Setup memory
     *------------------------------------------------*/
    #define MEM_MIN_BLOCK   512
    uint64_t MEM_MAX_SYSTEM_MEMORY = free_ram_in_kb() * 1024LL;
    MEM_MAX_SYSTEM_MEMORY /= 100LL;
    MEM_MAX_SYSTEM_MEMORY *= 90LL;  // Coge el 90% de la memoria

    uint64_t MEM_MAX_BLOCK = (MEM_MAX_SYSTEM_MEMORY / sizeof(md_record_t)) * sizeof(md_record_t);
    MEM_MAX_BLOCK = MIN(1*1024*1024*1024LL, MEM_MAX_BLOCK);  // 1*G max

    uint64_t MEM_SUPERBLOCK = MEM_MAX_BLOCK;

    static uint32_t mem_list[] = {2037, 0};
    gbmem_trace_alloc_free(0, mem_list);

    if(1) {
        gbmem_startup(
            MEM_MIN_BLOCK,
            MEM_MAX_BLOCK,
            MEM_SUPERBLOCK,
            MEM_MAX_SYSTEM_MEMORY,
            NULL,
            0
        );
    } else {
        gbmem_startup_system(
            MEM_MAX_BLOCK,
            MEM_MAX_SYSTEM_MEMORY
        );
    }
    json_set_alloc_funcs(
        gbmem_malloc,
        gbmem_free
    );
    uv_replace_allocator(
        gbmem_malloc,
        gbmem_realloc,
        gbmem_calloc,
        gbmem_free
    );

    log_startup(
        APP_NAME,       // application name
        VERSION,        // applicacion version
        APP_NAME        // executable program, to can trace stack
    );
    log_add_handler(APP_NAME, "stdout", LOG_OPT_LOGGER, 0);

    /*------------------------*
     *      Do your work
     *------------------------*/
    if(empty_string(arguments.file1)) {
        fprintf(stderr, "What file1 path?\n");
        exit(-1);
    }
    if(empty_string(arguments.file2)) {
        fprintf(stderr, "What file2 path?\n");
        exit(-1);
    }

    json_t *jn1 = nonlegalfile2json(arguments.file1, 1);
    if(!jn1) {
        exit(-1);
    }
    json_t *jn2 = nonlegalfile2json(arguments.file2, 2);
    if(!jn2) {
        exit(-2);
    }

    int equal = kwid_compare_records(
        jn1, // NOT owned
        jn2, // NOT owned
        arguments.without_metadata,
        arguments.without_private,
        arguments.verbose?TRUE:FALSE
    );
    printf("Same json? %s\n", equal?"yes":"no");

    JSON_DECREF(jn1);
    JSON_DECREF(jn2);

    gbmem_shutdown();
    return 0;
}
