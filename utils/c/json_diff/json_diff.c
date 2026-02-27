/****************************************************************************
 *          JSON_DIFF.C
 *
 *          Compare two json files. Items can be disordered.
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <regex.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <argp-standalone.h>
#include <gobj.h>
#include <testing.h>
#include <helpers.h>
#include <kwid.h>
#include <yev_loop.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define APP         "json_diff"
#define DOC         "Compare two json files. Items can be disordered."

#define VERSION     YUNETA_VERSION
#define SUPPORT     "<support at artgins.com>"
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
const char *argp_program_version = APP " " VERSION;
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
    uint64_t MEM_MAX_BLOCK = MEM_MAX_SYSTEM_MEMORY;
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

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

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

    json_t *jn1 = anyfile2json(arguments.file1, 1);
    if(!jn1) {
        exit(-1);
    }
    json_t *jn2 = anyfile2json(arguments.file2, 1);
    if(!jn2) {
        exit(-2);
    }

    int equal = kwid_compare_records(
        0,
        jn1, // NOT owned
        jn2, // NOT owned
        arguments.without_metadata,
        arguments.without_private,
        arguments.verbose?TRUE:FALSE
    );
    printf("Same json? %s\n", equal?"yes":"no");

    JSON_DECREF(jn1)
    JSON_DECREF(jn2)

    gbmem_shutdown();
    return 0;
}
